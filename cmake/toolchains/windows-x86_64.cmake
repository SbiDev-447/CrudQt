# Toolchain de compilacion cruzada Linux -> Windows (x86_64).
# Usa LLVM-MinGW (clang) + Qt 6 for Windows descargados en ~/devtools
# (portable: no toca el sistema). Requiere las herramientas del HOST Qt
# (QT_HOST_PATH) para AUTOMOC/AUTOUIC/AUTORCC.
#
# Uso:
#   cmake -S . -B build-win \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/windows-x86_64.cmake \
#     -DQT_HOST_PATH=$HOME/devtools/6.8.2/gcc_64

set(DEVTOOLS "$ENV{HOME}/devtools")

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER "${DEVTOOLS}/llvm-mingw/bin/x86_64-w64-mingw32-clang")
set(CMAKE_CXX_COMPILER "${DEVTOOLS}/llvm-mingw/bin/x86_64-w64-mingw32-clang++")

# El Qt for Windows (target) es la unica fuente de librerias/headers.
set(CMAKE_FIND_ROOT_PATH "${DEVTOOLS}/6.8.2/mingw_64")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
