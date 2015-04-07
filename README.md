pacdeplinearizer
================

A tool which helps to find a build order of multiple packages

It only uses information provided by the packages, namely their
dependencies.
It does NOT use any advanced techniques, like checking elf headers or
parsing CMakeLists
So if your package's dependencies are not correct, you are out of
luck!

```
USAGE: 

   ./build/pacdeplinearizer  [-f <packgages.txt>] [-u] [-c] [--]
                             [--version] [-h] <package> ...


Where: 

   -f <packgages.txt>,  --file <packgages.txt>
     A file listing the packages which should be ordered

   -u,  --unresolvable
     Print a warning when an unresolvable dependency is detected

   -c,  --cycle
     Print a warning when a cyclic dependency is detected

   --,  --ignore_rest
     Ignores the rest of the labeled arguments following this flag.

   --version
     Displays version information and exits.

   -h,  --help
     Displays usage information and exits.

   <package>  (accepted multiple times)
     a list of packages for which you want a possible buildorder
```
