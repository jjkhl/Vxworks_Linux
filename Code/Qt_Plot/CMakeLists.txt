cmake_minimum_required(VERSION 3.5) # CMake install : https://cmake.org/download/
project(Qt_Plot LANGUAGES CXX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_PREFIX_PATH "/home/jhl/Qt/5.15.2/gcc_64") # Qt Kit Dir
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt5 COMPONENTS Widgets PrintSupport REQUIRED) # Qt COMPONENTS
aux_source_directory(./src srcs)

# Specify MSVC UTF-8 encoding   
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

add_executable(${PROJECT_NAME}
    WIN32 # If you need a terminal for debug, please comment this statement 
    ${srcs} 
) 
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Widgets Qt5::PrintSupport) # Qt5 Shared Library