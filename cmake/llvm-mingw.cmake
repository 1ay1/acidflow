# Cross-compile for Windows with llvm-mingw:
#   cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/llvm-mingw.cmake \
#         -DLLVM_MINGW=/path/to/llvm-mingw
# Add -DMINGW_ARCH=aarch64 for Windows on ARM.
if(NOT MINGW_ARCH)
    set(MINGW_ARCH x86_64)
endif()
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR ${MINGW_ARCH})
if(NOT LLVM_MINGW)
    set(LLVM_MINGW "$ENV{LLVM_MINGW}")
endif()
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES MINGW_ARCH LLVM_MINGW)
set(CMAKE_C_COMPILER   ${LLVM_MINGW}/bin/${MINGW_ARCH}-w64-mingw32-clang)
set(CMAKE_CXX_COMPILER ${LLVM_MINGW}/bin/${MINGW_ARCH}-w64-mingw32-clang++)
set(CMAKE_RC_COMPILER  ${LLVM_MINGW}/bin/${MINGW_ARCH}-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH ${LLVM_MINGW}/${MINGW_ARCH}-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# Ship self-contained binaries (a plugin DLL can't rely on libc++.dll).
set(CMAKE_EXE_LINKER_FLAGS_INIT    "-static")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-static")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-static")
