cmake_minimum_required(VERSION 3.19)

project(
    abysspm
    DESCRIPTION "A Abyssland Plugin Manager"
)

file(GLOB_RECURSE SRCFILES CONFIGURE_DEPENDS "src/*.cpp")

set(CMAKE_CXX_STANDARD 23)

pkg_check_modules(abysspm_deps REQUIRED IMPORTED_TARGET tomlplusplus abyssutils>=0.7.0)

if (NOT TARGET glaze::glaze)
    find_package(glaze 7...<8 QUIET)
endif()
if (NOT glaze_FOUND AND NOT TARGET glaze::glaze)
    set(GLAZE_VERSION v7.2.0)
    message(STATUS "abysspm: glaze dependency not found, retrieving ${GLAZE_VERSION} with FetchContent")
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

add_executable(abysspm ${SRCFILES})
set_target_properties(abysspm PROPERTIES CXX_VISIBILITY_PRESET hidden)

target_link_libraries(abysspm PUBLIC PkgConfig::abysspm_deps glaze::glaze)

# binary
install(TARGETS abysspm)

# shell completions
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/abysspm.bash
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
        RENAME abysspm)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/abysspm.fish
        DESTINATION ${CMAKE_INSTALL_DATADIR}/fish/vendor_completions.d)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/abysspm.zsh
        DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
        RENAME _abysspm)
