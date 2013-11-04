cmake_minimum_required(VERSION 2.8)

link_directories(
    ${GTKMM_LIBRARY_DIRS}
    ${GTKSOURCEVIEWMM_LIBRARY_DIRS}
)

include_directories(
    ${GTKMM_INCLUDE_DIRS}
    ${GTKSOURCEVIEWMM_INCLUDE_DIRS}
)

add_executable(myeditor
    main.cc
    chooser.cc
    column.cc
    command.cc
    editwindow.cc
    file.cc
    filemgr.cc
    filewindow.cc
    scratchwindow.cc
    util.cc
    windowmgr.cc
)

target_link_libraries(myeditor
    ${GTKMM_LIBRARIES}
    ${GTKSOURCEVIEWMM_LIBRARIES}
)

set(CMAKE_CXX_FLAGS "-std=c++0x -Wall")

