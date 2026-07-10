cmake_minimum_required(VERSION 3.5)

# Set extension name here
set(TARGET_NAME deferred_columns)

# DuckDB's extension distribution supports vcpkg. As such, dependencies can be added in ./vcpkg.json and then
# used in cmake with find_package. Feel free to remove or replace with other dependencies.
# Note that it should also be removed from vcpkg.json to prevent needlessly installing it..
find_package(OpenSSL REQUIRED)

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})

set(CMAKE_CXX_STANDARD "17" CACHE STRING "C++ standard to enforce")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(src/include)

include(FetchContent)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        set(ORT_ARCH "linux-x64")
        set(ORT_FILENAME "onnxruntime-linux-x64-1.17.1.tgz")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        set(ORT_ARCH "linux-aarch64")
        set(ORT_FILENAME "onnxruntime-linux-aarch64-1.17.1.tgz")
    else()
        message(FATAL_ERROR "Unsupported Linux architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(ORT_ARCH "osx-universal2")
    set(ORT_FILENAME "onnxruntime-osx-universal2-1.17.1.tgz")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|AMD64")
        set(ORT_ARCH "win-x64")
        set(ORT_FILENAME "onnxruntime-win-x64-1.17.1.zip")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        set(ORT_ARCH "win-arm64")
        set(ORT_FILENAME "onnxruntime-win-arm64-1.17.1.zip")
    else()
        message(FATAL_ERROR "Unsupported Windows architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
else()
    message(FATAL_ERROR "Unsupported OS: ${CMAKE_SYSTEM_NAME}")
endif()

# Check if we already have it locally to avoid downloading every time
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-${ORT_ARCH}-1.17.1")
    set(ORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-${ORT_ARCH}-1.17.1")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-linux-x64-1.17.1" AND ORT_ARCH STREQUAL "linux-x64")
    set(ORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-linux-x64-1.17.1")
else()
    message(STATUS "Downloading ONNX Runtime 1.17.1 for ${ORT_ARCH}...")
    FetchContent_Declare(
      onnxruntime_download
      URL "https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/${ORT_FILENAME}"
    )
    FetchContent_MakeAvailable(onnxruntime_download)
    if(EXISTS "${onnxruntime_download_SOURCE_DIR}/include")
        set(ORT_DIR "${onnxruntime_download_SOURCE_DIR}")
    else()
        set(ORT_DIR "${onnxruntime_download_SOURCE_DIR}/onnxruntime-${ORT_ARCH}-1.17.1")
    endif()
endif()

message(STATUS "ONNX Runtime root directory set to: ${ORT_DIR}")
include_directories(${ORT_DIR}/include)

set(EXTENSION_SOURCES src/deferred_columns_extension.cpp src/deferred_scan.cpp src/deferred_optimizer.cpp src/model_registry.cpp src/deferred_aggregates.cpp)

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(ORT_LIB "${ORT_DIR}/lib/onnxruntime.lib")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(ORT_LIB "${ORT_DIR}/lib/libonnxruntime.dylib")
else()
    set(ORT_LIB "${ORT_DIR}/lib/libonnxruntime.so")
endif()

# Link OpenSSL in both the static library as the loadable extension
target_link_libraries(${EXTENSION_NAME} OpenSSL::SSL OpenSSL::Crypto ${ORT_LIB})
target_link_libraries(${LOADABLE_EXTENSION_NAME} OpenSSL::SSL OpenSSL::Crypto ${ORT_LIB})

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
