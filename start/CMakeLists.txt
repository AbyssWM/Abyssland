cmake_minimum_required(VERSION 3.19)

project(start-abyssland DESCRIPTION "Abyssland watchdog binary")

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

find_package(PkgConfig REQUIRED)

pkg_check_modules(startabyssland_deps REQUIRED IMPORTED_TARGET abyssutils>=0.10.3)

if (NOT TARGET glaze::glaze)
    find_package(glaze 7...<8 QUIET)
endif()
if (NOT glaze_FOUND AND NOT TARGET glaze::glaze)
    set(GLAZE_VERSION v7.2.0)
    message(STATUS "start: glaze dependency not found, retrieving ${GLAZE_VERSION} with FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        glaze
        GIT_REPOSITORY https://github.com/stephenberry/glaze.git
        GIT_TAG ${GLAZE_VERSION}
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(glaze)
endif()

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(start-abyssland ${SRCFILES})

target_link_libraries(start-abyssland PUBLIC PkgConfig::startabyssland_deps)
target_link_libraries(start-abyssland PUBLIC glaze::glaze)

install(TARGETS start-abyssland)

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
  set(CMAKE_EXECUTABLE_ENABLE_EXPORTS TRUE)
endif()
