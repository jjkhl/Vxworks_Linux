cmake_minimum_required(VERSION 3.5)
project(Qt_Plot LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_PREFIX_PATH "/home/jhl/Qt/5.15.2/gcc_64")   # Qt Kit Dir
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ====================== 查找 Qt5 ======================
find_package(Qt5 COMPONENTS Widgets PrintSupport REQUIRED)

# ====================== 查找 Qwt ======================
set(QWT_ROOT "/usr/local/qwt-6.3.0")

find_path(QWT_INCLUDE_DIR NAMES qwt_plot.h
    PATHS ${QWT_ROOT}/include
    NO_DEFAULT_PATH
)

#NO_DEFAULT_PATH表示不在默认路径(如/usr/lib、/usr/local/lib、/lib以及CMake自身维护的默认路径)
find_library(QWT_LIBRARY NAMES qwt
    PATHS ${QWT_ROOT}/lib
    NO_DEFAULT_PATH
)

if(NOT QWT_INCLUDE_DIR OR NOT QWT_LIBRARY)
    message(FATAL_ERROR "Qwt not found! Please check if Qwt is installed correctly at ${QWT_ROOT}")
endif()

message(STATUS "Found Qwt: ${QWT_INCLUDE_DIR}")
message(STATUS "Qwt Library: ${QWT_LIBRARY}")

# ====================== 源文件 ======================
aux_source_directory(./src srcs)

add_executable(${PROJECT_NAME}
    WIN32
    ${srcs}
)

# ====================== 包含目录 ======================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${QWT_INCLUDE_DIR}          # ← 新增 Qwt 头文件路径
)

# ====================== 链接库 ======================
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Widgets
    Qt5::PrintSupport
    ${QWT_LIBRARY}              # ← 新增 Qwt 库
)