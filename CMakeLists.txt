cmake_minimum_required(VERSION 3.15)
project(CppApp)

set(CMAKE_CXX_STANDARD 17)

find_package(fmt CONFIG REQUIRED)

add_executable(app src/main.cpp)

target_link_libraries(app PRIVATE fmt::fmt)
