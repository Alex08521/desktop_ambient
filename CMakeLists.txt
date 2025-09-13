cmake_minimum_required(VERSION 3.10)
project(desktop_ambient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPULSE REQUIRED libpulse)
pkg_check_modules(VORBISFILE REQUIRED vorbisfile)
pkg_check_modules(LIBPULSE_MAINLOOP REQUIRED libpulse-mainloop-glib)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(OGG REQUIRED ogg)

add_executable(desktop_ambient
    src/main.cpp
    src/audio_controller.cpp
    src/audio_player.cpp
    src/ogg_decoder.cpp
)

target_include_directories(desktop_ambient PRIVATE
    ${LIBPULSE_INCLUDE_DIRS}
    ${LIBPULSE_MAINLOOP_INCLUDE_DIRS}
    ${VORBISFILE_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${OGG_INCLUDE_DIRS}
    src)

target_link_libraries(desktop_ambient
    ${LIBPULSE_LIBRARIES}
    ${LIBPULSE_MAINLOOP_LIBRARIES}
    ${VORBISFILE_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${OGG_LIBRARIES}
    pulse
    pulse-simple
    pthread)