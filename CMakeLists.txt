cmake_minimum_required (VERSION 3.5)

project (KvStore)

set(CMAKE_CXX_STANDARD 23)

find_package (Seastar REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_executable (kv_store
  src/main.cc)

target_link_libraries (kv_store
  PRIVATE Seastar::seastar)