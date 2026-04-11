cmake_minimum_required(VERSION 3.20)
project(FarhanDB-GUI VERSION 2.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
include_directories(include)
include_directories(src)

set(ENGINE_SOURCES
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
)

set(GUI_SOURCES
    src/main.cpp
    src/bridge/DBBridge.cpp
    src/bridge/DBBridge.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/SqlEditor.cpp
    src/ui/SqlEditor.h
    src/ui/ResultTable.cpp
    src/ui/ResultTable.h
    src/ui/SidePanel.cpp
    src/ui/SidePanel.h
    resources.qrc
)

add_executable(FarhanDB-GUI WIN32 ${ENGINE_SOURCES} ${GUI_SOURCES})

# Embed icon using qt_add_resources or target_sources with .res
if(WIN32)
    target_sources(FarhanDB-GUI PRIVATE icons/farhandb.res)
    target_link_libraries(FarhanDB-GUI PRIVATE ws2_32)
endif()

target_link_libraries(FarhanDB-GUI PRIVATE Qt6::Widgets)
target_compile_options(FarhanDB-GUI PRIVATE -Wall -O2)
