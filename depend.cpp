#include <alpm.h>

#include <iostream>
#include <unordered_map>
#include <vector>

constexpr auto ROOTDIR = "/";
constexpr auto dbpath = "/var/lib/pacman";

int main(int argc, char* argv[]) {
  using package = std::string;
  using dependency_map = std::unordered_map<package, std::vector<package>>;
  auto packages = std::vector<package> {};
  auto pkg2deps = dependency_map {};
  auto pkg2optdeps = dependency_map {};
  alpm_errno_t err;
  auto handle = alpm_initialize(ROOTDIR, dbpath, &err);
  alpm_list_t *dblist = alpm_get_syncdbs(handle);
  for (auto dbs = dblist;
       dbs;
       dbs = alpm_list_next(dbs)) {
    auto db = reinterpret_cast<alpm_db_t*>(dbs->data);
    for(auto pkgs = alpm_db_get_pkgcache(db);
	pkgs;
	pkgs = alpm_list_next(pkgs)) {
      auto pkg = reinterpret_cast<alpm_pkg_t*>(pkgs->data);
      std::string pkgname = alpm_pkg_get_name(pkg);
      packages.push_back(pkgname);
      for (auto depends = alpm_pkg_get_depends(pkg);
	   depends;
           depends = alpm_list_next(depends)
	  ) {
            auto dependency = reinterpret_cast<alpm_pkg_t*>(depends->data);
	    std::string dependency_name = alpm_pkg_get_name(dependency);
	    pkg2deps[pkgname].push_back(dependency_name);
	  }
      for (auto opt_depends = alpm_pkg_get_optdepends(pkg);
	   opt_depends;
           opt_depends = alpm_list_next(opt_depends)
	  ) {
	    auto opt_dependency = reinterpret_cast<alpm_pkg_t*>(opt_depends->data);
	    std::string dependency_name = alpm_pkg_get_name(opt_dependency);
	    pkg2optdeps[pkgname].push_back(dependency_name);
	  }
    }
  }
  for (const auto& pkg: packages) {
    std::cout << pkg << ":\n";
    std::cout << "\tdependencies \n";
    for (const auto& dependency: pkg2deps[pkg])
      std::cout << dependency << " ";
    std::cout << "\toptional dependencies \n";
    for (const auto& opt_dependency: pkg2optdeps[pkg])
      std::cout << opt_dependency << " ";
  }
}
