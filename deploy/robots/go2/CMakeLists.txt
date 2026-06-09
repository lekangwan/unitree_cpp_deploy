cmake_minimum_required(VERSION 3.12)
project(go2_controller)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "-I/usr/local/include ${CMAKE_CXX_FLAGS}")
add_compile_options(-I/usr/local/include)

find_package(Boost REQUIRED COMPONENTS program_options)
find_package(yaml-cpp REQUIRED)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(terrain_msgs REQUIRED)

include_directories(BEFORE
  /usr/include/eigen3
  /usr/local/include/. # dds
  /usr/local/include/ddscxx # dds
  /usr/local/include/iceoryx/v2.0.2 # dds
  include
  ${PROJECT_SOURCE_DIR}/../../thirdparty/onnxruntime-linux-aarch64-gpu-1.16.0/include
  # ${PROJECT_SOURCE_DIR}/../../thirdparty/onnxruntime-linux-x64-1.23.2/include
  ${PROJECT_SOURCE_DIR}/../../thirdparty/
  ${PROJECT_SOURCE_DIR}/../../thirdparty/unitree_dds_include/
  ${PROJECT_SOURCE_DIR}/../../include/
)

link_libraries(
  unitree_sdk2 ddsc ddscxx rt pthread # dds
  libboost_program_options.a libyaml-cpp.a fmt
  ${PROJECT_SOURCE_DIR}/../../thirdparty/onnxruntime-linux-aarch64-gpu-1.16.0/lib/libonnxruntime.so.1.16.0
  # ${PROJECT_SOURCE_DIR}/../../thirdparty/onnxruntime-linux-x64-1.23.2/lib/libonnxruntime.so.1.23.2
)

file(GLOB_RECURSE ADD_SRC_LIST
  src/*.cpp
)

add_library(${PROJECT_NAME}_lib ${ADD_SRC_LIST})
link_libraries(${PROJECT_NAME}_lib)

add_executable(go2_ctrl main.cpp)

ament_target_dependencies(go2_ctrl
  rclcpp
  terrain_msgs
)
