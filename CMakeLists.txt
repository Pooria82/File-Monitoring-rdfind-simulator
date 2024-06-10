cmake_minimum_required(VERSION 3.10)
project(untitled)

set(CMAKE_C_STANDARD 11)

# تنظیم مسیرهای MSYS2
set(CMAKE_PREFIX_PATH "C:/msys64/mingw64")

# یافتن بسته‌های GTK
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(OpenSSL REQUIRED openssl)

include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})

include_directories(${OpenSSL_INCLUDE_DIRS})
link_directories(${OpenSSL_LIBRARY_DIRS})

add_executable(untitled main.c duplicatefilechecker.c)

target_link_libraries(untitled ${GTK3_LIBRARIES} ${OpenSSL_LIBRARIES})
