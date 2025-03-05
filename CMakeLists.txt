cmake_minimum_required (VERSION 3.5)

project (KvStore)

set(CMAKE_CXX_STANDARD 23)

find_package (Seastar REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_executable(kv_store
  "src/main.cc"
  "src/start.cc"
  "src/request_handlers.cc"
  "src/request_handlers.hh"
  "src/cache.cc"
  "src/cache.hh"
  "src/dispatcher.cc"
  "src/dispatcher.hh"
  "src/service.cc"
  "src/service.hh"
  "src/common.cc"
  "src/common.hh"
  "src/backup.cc"
  "src/backup.hh"
)

target_include_directories(kv_store
  PRIVATE
  src
)

target_link_libraries (kv_store
  PRIVATE Seastar::seastar)