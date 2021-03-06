cmake_minimum_required(VERSION 2.8.11)

project(navguide)
set(target navguide)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

set(MAR_VERSION_MAJOR  "0")
set(MAR_VERSION_MINOR  "1")
set(MAR_VERSION "${MAR_VERSION_MAJOR}.${MAR_VERSION_MINOR}")

find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(GDK3 REQUIRED gdk-3.0)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(FT2 REQUIRED freetype2)
pkg_check_modules(SDL2_IMG REQUIRED SDL2_image)

option(USE_OPENGL "Enable OpengGL accel" OFF)
option(USE_PG "profiling" OFF)

if (USE_PG)
    message("profiling")
    add_definitions(-static -pg -g)
endif()

if (USE_OPENGL)
    message("use opengl")
    add_definitions(-DUSE_OPENGL=1)
endif()

add_definitions(-std=c++1y ${SDL2_CFLAGS} ${SDL2_IMG_CFLAGS} ${GTK3_CFLAGS} ${FT2_CFLAGS})

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${SDL2_INCLUDE_DIRS})
include_directories(${GTK3_INCLUDE_DIRS})
include_directories(${GDK3_INCLUDE_DIRS})
include_directories(${FT2_INCLUDE_DIRS})
include_directories(${SDL2_IMG_INCLUDE_DIRS})

set(SRCS navguide.cc)

set(libs ${SDL2_LIBRARIES} ${GLIB2_LIBRARIES} ${SDL2_IMG_LIBRARIES})
if (USE_PG)
    set(libs ${libs} -pg)
endif()

add_executable(${target} ${SRCS})
target_link_libraries(${target} ${libs})

add_executable(navguide-gtk navguide-gtk.cc)
target_link_libraries(navguide-gtk ${GLIB2_LIBRARIES} ${GTK3_LIBRARIES} ${GDK3_LIBRARIES}
    ${FT2_LIBRARIES})

# install stage
install(TARGETS ${target} RUNTIME DESTINATION bin)
