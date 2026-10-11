cmake_minimum_required(VERSION 3.19)

project(
    abyssctl
    DESCRIPTION "Control utility for Abyssland"
)

pkg_check_modules(abyssctl_deps REQUIRED IMPORTED_TARGET abyssutils>=0.2.4 abysswire re2 readline)

file(GLOB_RECURSE ABYSSCTL_SRCFILES CONFIGURE_DEPENDS "src/*.cpp" "hw-protocols/*.cpp" "include/*.hpp")

add_executable(abyssctl ${ABYSSCTL_SRCFILES})
set_target_properties(abyssctl PROPERTIES CXX_VISIBILITY_PRESET hidden)

target_link_libraries(abyssctl PUBLIC PkgConfig::abyssctl_deps)
target_include_directories(abyssctl PRIVATE "hw-protocols")

# Abysswire

function(abyssprotocol protoPath protoName)
  set(path ${CMAKE_CURRENT_SOURCE_DIR}/${protoPath})
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/hw-protocols/${protoName}-client.cpp
           ${CMAKE_CURRENT_SOURCE_DIR}/hw-protocols/${protoName}-client.hpp
           ${CMAKE_CURRENT_SOURCE_DIR}/hw-protocols/${protoName}-spec.hpp
    COMMAND abysswire-scanner --client ${path}/${protoName}.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/hw-protocols/
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  target_sources(abyssctl PRIVATE hw-protocols/${protoName}-client.cpp
                                   hw-protocols/${protoName}-client.hpp
                                   hw-protocols/${protoName}-spec.hpp)
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
