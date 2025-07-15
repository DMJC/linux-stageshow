cmake_minimum_required(VERSION 3.10)
project(linux-stageshow)

# Allow prefix override; default is /usr
include(GNUInstallDirs)

# Dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)

# Source files
add_executable(linux-stageshow
    src/main.cpp
    src/playlistwindow.cpp
    src/playbackwindow.cpp
    src/cuepropertiesdialog.cpp
)

#define install location of shared resources (e.g. images)
set(STAGESHOW_DATA_DIR "${CMAKE_INSTALL_DATADIR}/linux-stageshow")

# Make this available to C++ via a preprocessor define
add_compile_definitions(STAGESHOW_DATA_DIR="${STAGESHOW_DATA_DIR}")

# Include dirs and compile flags
target_include_directories(linux-stageshow PRIVATE
    ${GTKMM_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_VIDEO_INCLUDE_DIRS}
)

target_compile_options(linux-stageshow PRIVATE
    ${GTKMM_CFLAGS_OTHER}
    ${GSTREAMER_CFLAGS_OTHER}
    ${GSTREAMER_VIDEO_CFLAGS_OTHER}
)

# Link libraries
target_link_libraries(linux-stageshow
    ${GTKMM_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_VIDEO_LIBRARIES}
)

# Install binary to /usr/bin (or user-defined)
install(TARGETS linux-stageshow
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install images to /usr/share/linux-stageshow
install(DIRECTORY images
    DESTINATION ${CMAKE_INSTALL_DATADIR}/linux-stageshow
    FILES_MATCHING PATTERN "*.png"
)
