cmake_minimum_required(VERSION 2.8)

project(myeditor)

find_package(PkgConfig)

pkg_check_modules(GTKMM gtkmm-3.0)
pkg_check_modules(GTKSOURCEVIEWMM gtksourceviewmm-3.0)

add_subdirectory(src)

set(CMAKE_CXX_FLAGS "-std=c++0x -Wall")

