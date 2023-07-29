@echo Must be run in a prompt context that defines vcvars.
@echo For example in 'x64 Native Tools Command Prompt for VS 2017'

cmake -G "Visual Studio 15 2017" -A x64 -B build -DWITH_IPOPT=ON -DIPOPT_LIBRARIES:FILEPATH="C:/usr/local/Ipopt-3.14.2-win64-msvs2019-md/lib/ipopt.dll.lib" -DIPOPT_LIBRARY_DIRS:PATH="C:/usr/local/Ipopt-3.14.2-win64-msvs2019-md/lib" -DIPOPT_INCLUDE_DIRS:PATH="C:/usr/local/Ipopt-3.14.2-win64-msvs2019-md/include/coin-or"  -DCMAKE_INSTALL_PREFIX:PATH="C:/usr/local/casadi_install"
cmake --build build --config Release
cmake --install build