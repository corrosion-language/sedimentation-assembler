cmake_minimum_required(VERSION 3.14)
project(sedimentation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(sedimentation src/coff.cpp src/elf.cpp src/main.cpp src/translate.cpp src/utility.cpp src/vex.cpp src/instr.dat src/vex.dat)
target_precompile_headers(sedimentation PRIVATE src/coff.hpp src/defines.hpp src/elf.hpp src/translate.hpp src/utility.hpp src/vex.hpp)
