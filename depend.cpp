#include <alpm.h>

#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <utility> // std::pair

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>

constexpr auto ROOTDIR = "/";
constexpr auto dbpath = "/var/lib/pacman";

using package = std::string;
using dependency_map = std::unordered_map<package, std::vector<package>>;

std::unordered_map<std::string, unsigned int> construct_pkg2number(const std::vector<std::string> packages) {
  std::unordered_map<std::string, unsigned int> pkg2number;
  const auto size = packages.size();
  for (std::remove_const<decltype(size)>::type i=0u; i < size; ++i)
    pkg2number[packages.at(i)] = i;
  return pkg2number;
}

void construct_graph(const std::vector<std::string> packages, const dependency_map& pkg2deps,  const dependency_map& pkg2optdeps) {
  using namespace boost;
  using Graph = adjacency_list<vecS, vecS, directedS>;
  
  Graph graph {packages.size()};  
  auto pkg2number = construct_pkg2number(packages);
  
  for (const auto& kv_pair: pkg2number) {
    const auto source = kv_pair.second;
    for (const auto& dependency: pkg2deps.at(kv_pair.first)) {
      add_edge(source, pkg2number.at(dependency), graph);
    }
  }
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
  const auto repos = {"core", "platform", "desktop", "apps", "games", "kde-next"};
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
      packages.push_back(pkgname);
      for (auto depends = alpm_pkg_get_depends(pkg); depends;
           depends = alpm_list_next(depends)) {
        auto dependency = reinterpret_cast<alpm_depend_t *>(depends->data);
        std::string dependency_name = dependency->name;
        pkg2deps[pkgname].push_back(dependency_name);
      }
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
#endif
  for (const auto &pkg : packages) {
    if (pkg2deps[pkg].empty() && pkg2optdeps[pkg].empty())
      std::cout << "Root: " << pkg << "\n";
  }
  return alpm_release(handle);
}
