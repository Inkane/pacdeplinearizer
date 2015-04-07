#include <alpm.h>

#include <iostream>
#include <cstdlib>
#include <unordered_map>
#include <vector>
#include <string>
#include <utility> // std::pair

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>

constexpr auto ROOTDIR = "/";
constexpr auto dbpath = "/var/lib/pacman";


std::vector<alpm_db_t*> databases;

std::vector<std::string> find_packages_by_name(std::string name) {
  std::vector<std::string> result;
  alpm_list_t* queries = nullptr;
  alpm_list_add(queries, strdup(name.c_str()));
  for (auto db: databases) {
    auto found_packages = alpm_db_search(db, queries);
    for (auto entry = found_packages; entry; entry = alpm_list_next(entry)) {
      auto package = reinterpret_cast<alpm_pkg_t*>(entry->data);
      result.push_back(alpm_pkg_get_name(package));
    }
  }
  FREELIST(queries);
  return result;
}

using package = std::string;
using dependency_map = std::unordered_map<package, std::vector<package>>;

std::unordered_map<std::string, unsigned int> construct_pkg2number(const std::vector<std::string> packages) {
  std::unordered_map<std::string, unsigned int> pkg2number;
  const auto size = packages.size();
  for (std::remove_const<decltype(size)>::type i=0u; i < size; ++i) {
    pkg2number[packages.at(i)] = i;
  }
  return pkg2number;
}

using namespace boost;
using Graph = adjacency_list<vecS, vecS, directedS>;

Graph construct_graph(const std::vector<std::string>& packages, const dependency_map& pkg2deps,  const dependency_map& pkg2optdeps) {

  
  Graph graph {packages.size()};  
  auto pkg2number = construct_pkg2number(packages);
  
  for (const auto& kv_pair: pkg2number) {
    const auto source = kv_pair.second;
    for (const auto& dependency: pkg2deps.at(kv_pair.first)) {
      try {
      add_edge(source, pkg2number.at(dependency), graph);
      } catch (std::out_of_range &e) {
	const auto resolves_to = find_packages_by_name(dependency);
	if (resolves_to.empty()) {
	  std::cerr << dependency << ", a dependency of " << packages.at(source) << " could not be found!\n";
	} else {
	  for (const auto& package: resolves_to) {
	    add_edge(source, pkg2number.at(package), graph);
	  }
	}
      }
    }
    for (const auto& opt_dependency: pkg2optdeps.at(kv_pair.first)) {
      try {
      add_edge(source, pkg2number.at(opt_dependency), graph);
      } catch (std::out_of_range &e) {
	std::cerr << opt_dependency << ", an optional dependency of " << packages.at(source) << "could not be found!\n";
      }
    }
  }
  
  return graph;
}

void compute_order(Graph graph, const std::vector<std::string>& packages) {
  using Vertex = graph_traits<Graph>::vertex_descriptor;
  using PackageOrder = std::vector<Vertex>;
  PackageOrder package_order;
  topological_sort(graph, std::back_inserter(package_order));
  for (const auto& elem: package_order)
    std::cout << packages.at(elem);
  std::cout<< std::endl;
}

int main(int argc, char *argv[]) {
  std::ios_base::sync_with_stdio(false);
  auto packages = std::vector<package>{};
  auto pkg2deps = dependency_map{};
  auto pkg2optdeps = dependency_map{};
  alpm_errno_t err;
  auto handle = alpm_initialize(ROOTDIR, dbpath, &err);
  if (!handle) {
    std::cerr << "No cake for you:" << alpm_strerror(err) << std::endl;
    return -1;
  }
  const auto repos = {"core", "platform", "desktop", "apps", "games", "kde-next", "extra"};
  for (const auto &repo : repos)
    databases.push_back(alpm_register_syncdb(handle, repo, ALPM_SIG_USE_DEFAULT));;
  alpm_list_t *dblist = alpm_get_syncdbs(handle);
  if (!dblist)
    std::cerr << "No dblists?!?" << std::endl;
  for (auto dbs = dblist; dbs; dbs = alpm_list_next(dbs)) {
    auto db = reinterpret_cast<alpm_db_t *>(dbs->data);
    for (auto pkgs = alpm_db_get_pkgcache(db); pkgs;
         pkgs = alpm_list_next(pkgs)) {
      auto pkg = reinterpret_cast<alpm_pkg_t *>(pkgs->data);
      std::string pkgname = alpm_pkg_get_name(pkg);
      packages.push_back(pkgname);
      pkg2deps[pkgname]; // ensure it
      for (auto depends = alpm_pkg_get_depends(pkg); depends;
           depends = alpm_list_next(depends)) {
        auto dependency = reinterpret_cast<alpm_depend_t *>(depends->data);
        std::string dependency_name = dependency->name;
        pkg2deps[pkgname].push_back(dependency_name);
      }
      pkg2optdeps[pkgname]; // ensure it exits
      for (auto opt_depends = alpm_pkg_get_optdepends(pkg); opt_depends;
           opt_depends = alpm_list_next(opt_depends)) {
        auto opt_dependency =
            reinterpret_cast<alpm_depend_t *>(opt_depends->data);
        std::string dependency_name = opt_dependency->name;
        pkg2optdeps[pkgname].push_back(dependency_name);
      }
    }
  }
#ifdef PRINT_IT
  for (const auto &pkg : packages) {
    std::cout << pkg << ":\n";
    std::cout << "\tdependencies: ";
    for (const auto &dependency : pkg2deps[pkg])
      std::cout << dependency << " ";
    std::cout << "\n\toptional dependencies: ";
    for (const auto &opt_dependency : pkg2optdeps[pkg])
      std::cout << opt_dependency << " ";
    std::cout << std::endl;
  }
  for (const auto &pkg : packages) {
    if (pkg2deps[pkg].empty() && pkg2optdeps[pkg].empty())
      std::cout << "Root: " << pkg << "\n";
  }
  #endif
  auto graph = construct_graph(packages, pkg2deps, pkg2optdeps);
  //compute_order(graph, packages);
  return alpm_release(handle);
}
