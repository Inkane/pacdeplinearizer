#include <alpm.h>

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <utility> // std::pair

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>

#include <tclap/CmdLine.h>
#include <tclap/UnlabeledMultiArg.h>

constexpr auto ROOTDIR = "/";
constexpr auto dbpath = "/var/lib/pacman";
constexpr auto package_graph_sentinel = "";

using package = std::string;
using dependency_map = std::unordered_map<package, std::vector<package>>;

using namespace boost;
using Graph = adjacency_list<vecS, vecS, directedS>;
using Vertex = graph_traits<Graph>::vertex_descriptor;
using PackageOrder = std::vector<Vertex>;

struct DependData {
  std::vector<package> packages = std::vector<package>{package_graph_sentinel};
  dependency_map pkg2deps;
  dependency_map pkg2optdeps;
  dependency_map pkg2provided_by;

  DependData() {
    pkg2deps[package_graph_sentinel];
    pkg2optdeps[package_graph_sentinel];
  }

  package vertex2package(const Vertex vertex) const {
    return packages.at(vertex);
  }
};

std::unordered_map<std::string, unsigned long>
construct_pkg2number(const std::vector<std::string> packages) {
  std::unordered_map<std::string, unsigned long> pkg2number;
  const auto size = packages.size();
  for (std::remove_const<decltype(size)>::type i = 0u; i < size; ++i) {
    pkg2number[packages.at(i)] = i;
  }
  return pkg2number;
}

Graph construct_graph(const DependData &depend_data,
                      bool printIfDependencyIsMissing) {

  Graph graph{depend_data.packages.size()};
  auto pkg2number = construct_pkg2number(depend_data.packages);
  for (const auto &kv_pair : pkg2number) {
    const auto source = kv_pair.second;
    for (const auto &dependency : depend_data.pkg2deps.at(kv_pair.first)) {
      try {
        add_edge(source, pkg2number.at(dependency), graph);
      } catch (std::out_of_range &) {
        if (depend_data.pkg2provided_by.find(dependency) ==
            depend_data.pkg2provided_by.end()) {
          if (printIfDependencyIsMissing) {
            std::cerr << dependency << ", a dependency of "
                      << depend_data.vertex2package(source)
                      << " could not be found!\n";
          }
        } else {
          const auto &resolves_to = depend_data.pkg2provided_by.at(dependency);
          for (const auto &package : resolves_to) {
            add_edge(source, pkg2number.at(package), graph);
          }
        }
      }
    }
  }

  return graph;
}

PackageOrder compute_order(Graph graph) {
  PackageOrder package_order;
  topological_sort(graph, std::back_inserter(package_order));
  return package_order;
}

namespace helper {
auto alpm_list_foreach(alpm_list_t *list, std::function<void(alpm_list_t *)> f)
    -> size_t {
  auto loop_counter = 0u;
  for (auto elem = list; elem; elem = alpm_list_next(elem)) {
    f(elem);
    ++loop_counter;
  }
  return loop_counter;
}
}

// directly taken from boost
template <typename EdgeStorage> struct cycle_detector : public dfs_visitor<> {
  cycle_detector(bool &has_cycle, EdgeStorage &storage)
      : _has_cycle(has_cycle), edge_storage(storage) {}

  template <class Edge, class Graph> void back_edge(Edge e, Graph &) {
    _has_cycle = true;
    edge_storage.push_back(e);
  }

public:
  bool &_has_cycle;
  EdgeStorage &edge_storage;
};

void collect_package_information(alpm_handle_t *handle,
                                 DependData &depend_data) {
  const auto repos = {"core",  "platform", "desktop", "apps",
                      "games", "kde-next", "extra",   "lib32"};
  for (const auto &repo : repos)
    alpm_register_syncdb(handle, repo, ALPM_SIG_USE_DEFAULT);
  alpm_list_t *dblist = alpm_get_syncdbs(handle);
  if (!dblist)
    std::cerr << "No dblists?!?" << std::endl;
  for (auto dbs = dblist; dbs; dbs = alpm_list_next(dbs)) {
    auto db = reinterpret_cast<alpm_db_t *>(dbs->data);
    for (auto pkgs = alpm_db_get_pkgcache(db); pkgs;
         pkgs = alpm_list_next(pkgs)) {
      auto pkg = reinterpret_cast<alpm_pkg_t *>(pkgs->data);
      std::string pkgname = alpm_pkg_get_name(pkg);
      depend_data.packages.push_back(pkgname);
      // ensure that an entry aways exists
      depend_data.pkg2deps[pkgname];
      depend_data.pkg2optdeps[pkgname];
      auto store_depends =
          [&pkgname](dependency_map &storage, alpm_list_t *list_elem) {
        auto dependency = reinterpret_cast<alpm_depend_t *>(list_elem->data);
        std::string dependency_name = dependency->name;
        storage[pkgname].push_back(dependency_name);
      };
      auto dependency_count = helper::alpm_list_foreach(
          alpm_pkg_get_depends(pkg),
          [&depend_data, &store_depends](alpm_list_t *list_elem) {
            store_depends(depend_data.pkg2deps, list_elem);
          });
      if (dependency_count == 0)
        depend_data.pkg2deps[pkgname].push_back(package_graph_sentinel);
      helper::alpm_list_foreach(
          alpm_pkg_get_optdepends(pkg),
          [&depend_data, &store_depends](alpm_list_t *list_elem) {
            store_depends(depend_data.pkg2optdeps, list_elem);
          });
      helper::alpm_list_foreach(
          alpm_pkg_get_provides(pkg),
          [&pkgname, &depend_data](alpm_list_t *list_elem) {
            auto providee = reinterpret_cast<alpm_depend_t *>(list_elem->data);
            std::string providee_name = providee->name;
            depend_data.pkg2provided_by[providee_name].push_back(pkgname);
          });
    }
  }
}

void handle_cycle(const bool notifyCyclic, Graph &graph,
                  const DependData &depend_data) {
  auto has_cycle = false;
  using EdgeStorage = std::vector<Graph::edge_descriptor>;
  do {
    has_cycle = false;
    auto edge_storage = EdgeStorage{};
    cycle_detector<EdgeStorage> vis(has_cycle, edge_storage);
    depth_first_search(graph, visitor(vis));
    if (has_cycle) {
      auto edge = edge_storage[0];
      if (notifyCyclic) {
        std::cout << "Breaking cyclic dependency by removing edge from "
                  << depend_data.vertex2package(source(edge, graph)) << " to "
                  << depend_data.vertex2package(target(edge, graph)) << "\n";
      }
      graph.remove_edge(edge);
    }
  } while (has_cycle);
}

int main(int argc, char *argv[]) {
  try {
    std::ios_base::sync_with_stdio(false);
    // set up command line parsin
    TCLAP::CmdLine cmd("depend: A tool which helps to find a build order\n"
                       "It only uses information provided by the packages, "
                       "namely their dependencies\n"
                       "It does NOT use any advanced techniques, like checking "
                       "elf headers or parsing CMakeLists\n"
                       "So if your package's dependencies are not correct, you "
                       "are out of luck!\n",
                       ' ', "0.1");
    TCLAP::UnlabeledMultiArg<std::string> packageListSwitch(
        "packages",
        "a list of packages for which you want a possible buildorder", false,
        "package", cmd);
    TCLAP::SwitchArg cyclicSwitch(
        "c", "cycle", "Print a warning when a cyclic dependency is detected",
        cmd, false);
    TCLAP::SwitchArg unresolvableSwitch(
        "u", "unresolvable",
        "Print a warning when an unresolvable dependency is detected", cmd,
        false);
    TCLAP::ValueArg<std::string> packageFileSwitch(
        "f", "file", "A file listing the packages which should be ordered",
        false, "", "packgages.txt", cmd);

    cmd.parse(argc, argv);
    bool notifyCyclic = cyclicSwitch.getValue();
    bool notifyUnresolvable = unresolvableSwitch.getValue();
    std::vector<std::string> package_list = packageListSwitch.getValue();
    std::string package_file_name = packageFileSwitch.getValue();

    auto order_those_packages = std::unordered_set<std::string>{
        package_list.begin(), package_list.end()};
    if (package_file_name != "") {
      std::ifstream package_file(package_file_name);
      std::copy(
          std::istream_iterator<std::string>(package_file),
          std::istream_iterator<std::string>(),
          std::inserter(order_those_packages, order_those_packages.begin()));
    }

    auto depend_data = DependData{};
    alpm_errno_t err;
    auto handle = alpm_initialize(ROOTDIR, dbpath, &err);
    if (!handle) {
      std::cerr << "No cake for you:" << alpm_strerror(err) << std::endl;
      return -1;
    }

    collect_package_information(handle, depend_data);
    auto graph = construct_graph(depend_data, notifyUnresolvable);
    handle_cycle(notifyCyclic, graph, depend_data);
    if (order_those_packages.size() > 0) {
      std::cout << "Determining (one possible) correct build order:\n";
      auto topo_order = compute_order(graph);
      for (const auto &vertex : topo_order) {
        auto pkg = depend_data.vertex2package(vertex);
        if (order_those_packages.find(pkg) != order_those_packages.end())
          std::cout << pkg << "\n";
      }
    }
    return alpm_release(handle);
  } catch (TCLAP::ArgException &e) // catch any exceptions
  {
    std::cerr << "error: " << e.error() << " for arg " << e.argId()
              << std::endl;
  } catch (...) {
    std::cerr << "Something went horribly wrong!" << std::endl;
    throw;
  }
}
