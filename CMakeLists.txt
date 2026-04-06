cmake_minimum_required(VERSION 3.20)
project(FarhanDB VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(include)

# Source files
set(SOURCES
    src/storage/page.cpp
    src/storage/disk_manager.cpp
    src/storage/buffer_pool.cpp
    src/storage/wal.cpp
    src/query/lexer.cpp
    src/query/parser.cpp
	src/query/catalog.cpp
	src/query/executor.cpp
    src/transaction/lock_manager.cpp
    src/transaction/transaction_manager.cpp
    src/main.cpp
)

add_executable(farhandb ${SOURCES})

# Enable warnings
target_compile_options(farhandb PRIVATE -Wall -Wextra -O2)
