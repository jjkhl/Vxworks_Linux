cmake_minimum_required(VERSION 3.10)
project(rt_system)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) #自动生成compile_commands.json

file(GLOB_RECURSE SOURCES "rt_system.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_options(${PROJECT_NAME} PRIVATE -O2)
target_link_libraries(${PROJECT_NAME} pthread rt)