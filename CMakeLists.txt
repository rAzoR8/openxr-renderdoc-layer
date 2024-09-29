cmake_minimum_required(VERSION 3.14)
set(CMAKE_CXX_STANDARD 11)

#### Layer control

project(
  DefaultLayerAPI
  VERSION 1.0
  DESCRIPTION "Layer control API"
  LANGUAGES CXX)

set(LayerApi_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/api/DefaultLayerApi.cpp
)

add_library(DefaultLayerAPI SHARED ${LayerApi_SRC} ${Layer_INC})

target_include_directories(DefaultLayerAPI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(LAYER_CONTROL_LIB_NAME $<TARGET_FILE_NAME:DefaultLayerAPI>)
set(LAYER_CONTROL_LIB_PATH $<TARGET_FILE:DefaultLayerAPI>)

#### Layer impl

project(
  OpenXRRenderDocLayer
  VERSION 1.0
  DESCRIPTION "RenderDoc capturing layer for OpenXR"
  LANGUAGES CXX)

set(Layer_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Layer.cpp
)

set(Layer_INC
    ${CMAKE_CURRENT_SOURCE_DIR}/include/Layer.h
)

add_library(OpenXRRenderDocLayer SHARED ${Layer_SRC} ${Layer_INC})

add_compile_definitions(LAYER_CONTROL_LIB_NAME="${LAYER_CONTROL_LIB_NAME}")
add_compile_definitions(LAYER_CONTROL_LIB_PATH="${LAYER_CONTROL_LIB_PATH}")

target_include_directories(OpenXRRenderDocLayer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(OpenXRRenderDocLayer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external/openxr)
target_include_directories(OpenXRRenderDocLayer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external/renderdoc)

set(LAYER_BINARY_DIR $<TARGET_FILE_DIR:OpenXRRenderDocLayer>)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/renderdoc-layer.json.in ${CMAKE_CURRENT_BINARY_DIR}/renderdoc-layer.json.in @ONLY)
file(
  GENERATE
  OUTPUT ${LAYER_BINARY_DIR}/renderdoc-layer.json
  INPUT ${CMAKE_CURRENT_BINARY_DIR}/renderdoc-layer.json.in
)