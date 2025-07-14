cmake_minimum_required(VERSION 3.10)
project(linux-stageshow)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_VIDEO gstreamer-video-1.0 REQUIRED)

add_executable(linux-stageshow src/main.cpp src/playlistwindow.cpp src/playbackwindow.cpp src/cuepropertiesdialog.cpp)

target_include_directories(linux-stageshow PRIVATE
    ${GTKMM_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_VIDEO_LIBRARY_DIRS})

target_link_libraries(linux-stageshow
    ${GTKMM_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_VIDEO_LIBRARIES})

target_compile_options(linux-stageshow PRIVATE
    ${GTKMM_CFLAGS_OTHER}
    ${GSTREAMER_CFLAGS_OTHER})
