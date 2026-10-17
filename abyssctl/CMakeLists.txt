cmake_minimum_required(VERSION 3.19)

project(
    abyssctl
    DESCRIPTION "Control utility for Abyssland"
)

pkg_check_modules(abyssctl_deps REQUIRED IMPORTED_TARGET abyssutils>=0.2.4 abysswire re2 readline)

file(GLOB_RECURSE ABYSSCTL_SRCFILES CONFIGURE_DEPENDS "src/*.cpp" "include/*.hpp")

add_executable(abyssctl ${ABYSSCTL_SRCFILES})
set_target_properties(abyssctl PROPERTIES CXX_VISIBILITY_PRESET hidden)

include(GNUInstallDirs)

# build identity for `abyssctl doctor` (commit + dirty flag of the tree at configure time)
find_program(ABYSSCTL_GIT git)
set(ABYSSCTL_GIT_COMMIT "unknown")
set(ABYSSCTL_GIT_DIRTY "unknown")
if(ABYSSCTL_GIT)
    execute_process(COMMAND ${ABYSSCTL_GIT} rev-parse --short HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _c OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE _rc)
    if(_rc EQUAL 0)
        set(ABYSSCTL_GIT_COMMIT "${_c}")
        execute_process(COMMAND ${ABYSSCTL_GIT} status --porcelain --untracked-files=no WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE _d OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if(_d STREQUAL "")
            set(ABYSSCTL_GIT_DIRTY "clean")
        else()
            set(ABYSSCTL_GIT_DIRTY "dirty")
        endif()
    endif()
endif()
target_compile_definitions(abyssctl PRIVATE ABYSSCTL_GIT_COMMIT="${ABYSSCTL_GIT_COMMIT}" ABYSSCTL_GIT_DIRTY="${ABYSSCTL_GIT_DIRTY}")

# used by `abyssctl reload` to find the tree to rebuild (overridable via env ABYSS_SOURCE_DIR / ABYSS_BUILD_DIR / ABYSS_PREFIX)
target_compile_definitions(abyssctl PRIVATE
    ABYSS_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    ABYSS_BUILD_DIR="${CMAKE_BINARY_DIR}"
    ABYSS_DEFAULT_PREFIX="${CMAKE_INSTALL_PREFIX}")

target_link_libraries(abyssctl PUBLIC PkgConfig::abyssctl_deps)
target_include_directories(abyssctl PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/hw-protocols")

# Abysswire

find_program(ABYSSWIRE_SCANNER abysswire-scanner REQUIRED)

function(abyssprotocol protoPath protoName)
  set(protocol "${CMAKE_CURRENT_SOURCE_DIR}/${protoPath}/${protoName}.xml")
  set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/hw-protocols")
  add_custom_command(
    OUTPUT "${output_dir}/${protoName}-client.cpp"
           "${output_dir}/${protoName}-client.hpp"
           "${output_dir}/${protoName}-spec.hpp"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${output_dir}"
    COMMAND "${ABYSSWIRE_SCANNER}" --client "${protocol}" "${output_dir}/"
    DEPENDS "${protocol}" "${ABYSSWIRE_SCANNER}"
    VERBATIM)
  target_sources(abyssctl PRIVATE "${output_dir}/${protoName}-client.cpp"
                                   "${output_dir}/${protoName}-client.hpp"
                                   "${output_dir}/${protoName}-spec.hpp")
endfunction()

abyssprotocol(hw-protocols abysspaper_core)

# binary
install(TARGETS abyssctl)

# shell completions
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/abyssctl.bash
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
        RENAME abyssctl)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/abyssctl.fish
        DESTINATION ${CMAKE_INSTALL_DATADIR}/fish/vendor_completions.d)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/abyssctl.zsh
        DESTINATION ${CMAKE_INSTALL_DATADIR}/zsh/site-functions
        RENAME _abyssctl)
