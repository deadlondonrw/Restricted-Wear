# Cross-compile Windows x64 binaries from Linux with llvm-mingw
# (https://github.com/mstorsjo/llvm-mingw, UCRT variant).
#   cmake -S . -B build-win -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/llvm-mingw-x86_64.cmake \
#         -DLLVM_MINGW_ROOT=/path/to/llvm-mingw
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

if(NOT LLVM_MINGW_ROOT)
    if(DEFINED ENV{LLVM_MINGW_ROOT})
        set(LLVM_MINGW_ROOT $ENV{LLVM_MINGW_ROOT})
    else()
        set(LLVM_MINGW_ROOT /opt/llvm-mingw)
    endif()
endif()
set(LLVM_MINGW_ROOT ${LLVM_MINGW_ROOT} CACHE PATH "llvm-mingw installation")

set(TRIPLE x86_64-w64-mingw32)
set(CMAKE_C_COMPILER   ${LLVM_MINGW_ROOT}/bin/${TRIPLE}-clang)
set(CMAKE_CXX_COMPILER ${LLVM_MINGW_ROOT}/bin/${TRIPLE}-clang++)
set(CMAKE_RC_COMPILER  ${LLVM_MINGW_ROOT}/bin/${TRIPLE}-windres)
set(CMAKE_AR           ${LLVM_MINGW_ROOT}/bin/llvm-ar CACHE FILEPATH "")
set(CMAKE_RANLIB       ${LLVM_MINGW_ROOT}/bin/llvm-ranlib CACHE FILEPATH "")

set(CMAKE_FIND_ROOT_PATH ${LLVM_MINGW_ROOT}/${TRIPLE})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Target any x86-64 CPU from the last ~12 years (SSE4.2 / POPCNT), Windows 10+
set(CMAKE_C_FLAGS_INIT   "-march=x86-64-v2 -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00")
set(CMAKE_CXX_FLAGS_INIT "-march=x86-64-v2 -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00")
