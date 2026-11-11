cmake_minimum_required(VERSION 3.20)
project(ztb_bench CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(PQ      REQUIRED libpq)
pkg_check_modules(SQLITE  REQUIRED sqlite3)
# sterownik MongoDB: 1.x (Ubuntu 24.04, Docker) albo 2.x (Ubuntu 26.04)
pkg_search_module(MONGOC  REQUIRED libmongoc-1.0 mongoc2)
# mgclient linkowany statycznie (wymaga OpenSSL)
find_library(MGCLIENT_LIB NAMES libmgclient.a mgclient REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(ztb_bench
    src/main.cpp
    src/csv.cpp
    src/dataset.cpp
    src/relational.cpp
    src/benchmark.cpp
    src/scenarios.cpp
    src/postgres_db.cpp
    src/sqlite_db.cpp
    src/mongo_db.cpp
    src/memgraph_db.cpp
)

target_include_directories(ztb_bench PRIVATE
    ${PQ_INCLUDE_DIRS} ${SQLITE_INCLUDE_DIRS} ${MONGOC_INCLUDE_DIRS})
target_link_libraries(ztb_bench PRIVATE
    ${PQ_LIBRARIES} ${SQLITE_LIBRARIES} ${MONGOC_LIBRARIES} ${MGCLIENT_LIB} OpenSSL::SSL OpenSSL::Crypto)
target_compile_options(ztb_bench PRIVATE -Wall -Wextra)

# Domyslne katalogi wzgledem katalogu zrodel - dziala z IDE bez ustawiania zmiennych
# (w Dockerze nadpisywane przez SQL_DIR / RESULTS_DIR)
target_compile_definitions(ztb_bench PRIVATE
    ZTB_SQL_DIR="${CMAKE_SOURCE_DIR}/sql"
    ZTB_RESULTS_DIR="${CMAKE_SOURCE_DIR}/results")
