set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_COMPILER "C:/Users/OT-PC220/Desktop/clang+llvm-18.1.0-x86_64-pc-windows-msvc/bin/clang.exe")
set(CMAKE_CXX_COMPILER "C:/Users/OT-PC220/Desktop/clang+llvm-18.1.0-x86_64-pc-windows-msvc/bin/clang++.exe")

cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(NewCentury VERSION 1.0)

set(CMAKE_C_COMPILER "C:/Users/OT-PC220/Desktop/clang+llvm-18.1.0-x86_64-pc-windows-msvc/bin/clang.exe")
set(CMAKE_CXX_COMPILER "C:/Users/OT-PC220/Desktop/clang+llvm-18.1.0-x86_64-pc-windows-msvc/bin/clang++.exe")

find_package(Torch REQUIRED)
include_directories(.)
include_directories(trainer)
add_executable(NewCentury-avx2 src/main.cpp)
target_compile_options(NewCentury-avx2 PUBLIC -march=x86-64-v3)
target_link_libraries(NewCentury-avx2 "${TORCH_LIBRARIES}")
target_compile_definitions(NewCentury-avx2 PUBLIC ValueNetFileName="../src/value_net.nnue")

# The following code block is suggested to be used on Windows.
# According to https://github.com/pytorch/pytorch/issues/25457,
# the DLLs need to be copied to avoid memory errors.
if (MSVC)
  file(GLOB TORCH_DLLS "${TORCH_INSTALL_PREFIX}/lib/*.dll")
  add_custom_command(TARGET NewCentury-avx2
                     POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different
                     ${TORCH_DLLS}
                     $<TARGET_FILE_DIR:NewCentury-avx2>)
endif (MSVC)