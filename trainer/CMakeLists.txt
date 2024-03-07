cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(trainer)

find_package(Torch REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")
set(CMAKE_CXX_COMPILER clang++) # Use Clang as the compiler
set(CMAKE_BUILD_TYPE Debug) # Set the build type to Debug

include_directories(.)
add_compile_options(/arch:AVX2)
add_executable(trainer trainer.cpp)

add_definitions(-DDEBUG)
string( REPLACE "/DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

target_link_libraries(trainer "${TORCH_LIBRARIES}")
set_property(TARGET trainer PROPERTY CXX_STANDARD 20)

# The following code block is suggested to be used on Windows.
# According to https://github.com/pytorch/pytorch/issues/25457,
# the DLLs need to be copied to avoid memory errors.
if (MSVC)
  file(GLOB TORCH_DLLS "${TORCH_INSTALL_PREFIX}/lib/*.dll")
  add_custom_command(TARGET trainer
                     POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different
                     ${TORCH_DLLS}
                     $<TARGET_FILE_DIR:trainer>)
endif (MSVC)