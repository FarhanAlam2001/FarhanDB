cmake_minimum_required(VERSION 3.20)
project(FarhanDB VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(include)

set(SOURCES
    src/storage/page.cpp
    src/storage/disk_manager.cpp
    src/storage/buffer_pool.cpp
    src/storage/wal.cpp
    src/index/btree.cpp
    src/query/lexer.cpp
    src/query/parser.cpp
    src/query/catalog.cpp
    src/query/optimizer.cpp
    src/query/executor.cpp
    src/transaction/lock_manager.cpp
    src/transaction/transaction_manager.cpp
    src/server/server.cpp
    src/main.cpp
)

add_executable(farhandb ${SOURCES})

target_compile_options(farhandb PRIVATE -Wall -Wextra -O2)

# Link Winsock on Windows
if(WIN32)
    target_link_libraries(farhandb ws2_32)
endif()
