cmake_minimum_required(VERSION 3.30)

# Get version
file(READ "${CMAKE_SOURCE_DIR}/VERSION" VER_RAW)
string(STRIP ${VER_RAW} VER)

project(
  Abyssland
  DESCRIPTION "A Modern C++ Wayland Compositor"
  VERSION ${VER})

include(CTest)
include(CheckIncludeFile)
include(GNUInstallDirs)

set(ABYSSLAND_VERSION ${VER})
set(PREFIX ${CMAKE_INSTALL_PREFIX})
set(INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR})
set(BINDIR ${CMAKE_INSTALL_BINDIR})

set(CMAKE_MESSAGE_LOG_LEVEL "STATUS")

message(STATUS "Gathering git info")

# Make shader files includable
execute_process(COMMAND ./scripts/generateShaderIncludes.sh
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                RESULT_VARIABLE ABYSS_SHADER_GEN_RESULT)
if(NOT ABYSS_SHADER_GEN_RESULT EQUAL 0)
  message(
    FATAL_ERROR
      "Failed to generate shader includes (scripts/generateShaderIncludes.sh), exit code: ${ABYSS_SHADER_GEN_RESULT}"
  )
endif()

find_package(PkgConfig REQUIRED)
find_package(Python3 COMPONENTS Interpreter QUIET)

# Try to find canihavesomecoffee's udis86 using pkgconfig vmd/udis86 does not
# provide a .pc file and won't be detected this way
pkg_check_modules(udis_dep IMPORTED_TARGET udis86>=1.7.2)

# Find non-pkgconfig udis86, otherwise fallback to subproject
if(NOT udis_dep_FOUND)
  find_library(udis_nopc udis86)
  if(NOT("${udis_nopc}" MATCHES "udis_nopc-NOTFOUND"))
    message(STATUS "Found udis86 at ${udis_nopc}")
  else()
    add_subdirectory("subprojects/udis86")
    include_directories("subprojects/udis86")
    message(STATUS "udis86 dependency not found, falling back to subproject")
  endif()
endif()

find_library(librt rt)
if("${librt}" MATCHES "librt-NOTFOUND")
  unset(LIBRT)
else()
  set(LIBRT rt)
endif()

if(CMAKE_BUILD_TYPE)
  string(TOLOWER ${CMAKE_BUILD_TYPE} BUILDTYPE_LOWER)
  if(BUILDTYPE_LOWER STREQUAL "release")
    # Pass.
  elseif(BUILDTYPE_LOWER STREQUAL "debug")
    # Pass.
  elseif(BUILDTYPE_LOWER STREQUAL "relwithdebinfo")
    set(BUILDTYPE_LOWER "debugoptimized")
  elseif(BUILDTYPE_LOWER STREQUAL "minsizerel")
    set(BUILDTYPE_LOWER "minsize")
  elseif(BUILDTYPE_LOWER STREQUAL "none")
    set(BUILDTYPE_LOWER "plain")
  else()
    set(BUILDTYPE_LOWER "release")
  endif()
else()
  set(BUILDTYPE_LOWER "release")
endif()

pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
message(STATUS "Found wayland-protocols at ${WAYLAND_PROTOCOLS_DIR}")
pkg_get_variable(WAYLAND_SCANNER_PKGDATA_DIR wayland-scanner pkgdatadir)
message(
  STATUS "Found wayland-scanner pkgdatadir at ${WAYLAND_SCANNER_PKGDATA_DIR}")

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
  message(STATUS "Configuring Abyssland in Debug with CMake")
  add_compile_definitions(ABYSSLAND_DEBUG)
  set(BUILD_TESTING ON)
else()
  add_compile_options(-O3)
  message(STATUS "Configuring Abyssland in Release with CMake")
  set(BUILD_TESTING OFF)
endif()

add_compile_definitions(ABYSSLAND_VERSION="${ABYSSLAND_VERSION}")

include_directories(. "src/" "protocols/")

set(CMAKE_CXX_STANDARD 26)
set(CXX_STANDARD_REQUIRED ON)
add_compile_options(
  -Wall
  -Wextra
  -Wpedantic
  -Wno-keyword-macro
  -Wno-unused-result
  -Wno-unused-parameter
  -Wno-unused-value
  -Wno-missing-field-initializers
  -Wno-gnu-zero-variadic-macro-arguments
  -Wno-narrowing
  -Wno-pointer-arith
  -Wno-clobbered
  -frtti
  -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=)

# disable lto as it may break plugins
add_compile_options(-fno-lto)

set(CMAKE_EXECUTABLE_ENABLE_EXPORTS TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

message(STATUS "Checking deps...")

find_package(Threads REQUIRED)

set(GLES_VERSION "GLES3")
find_package(OpenGL REQUIRED COMPONENTS ${GLES_VERSION})
find_package(glslang CONFIG REQUIRED)

find_package(glaze 7...<8 QUIET)
if(NOT glaze_FOUND)
  set(GLAZE_VERSION v7.2.0)
  message(STATUS "glaze dependency not found, retrieving ${GLAZE_VERSION} with FetchContent")
  include(FetchContent)
  FetchContent_Declare(
    glaze
    GIT_REPOSITORY https://github.com/stephenberry/glaze.git
    GIT_TAG ${GLAZE_VERSION}
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL)
  FetchContent_MakeAvailable(glaze)
endif()

set(ABYSSAL_MINIMUM_VERSION 0.9.3)
set(ABYSSPARSE_MINIMUM_VERSION 0.6.7)
set(ABYSSCURSOR_MINIMUM_VERSION 0.1.7)
set(ABYSSUTILS_MINIMUM_VERSION 0.14.0)
set(ABYSSGRAPHICS_MINIMUM_VERSION 0.5.1)

pkg_check_modules(abyssal_dep REQUIRED IMPORTED_TARGET abyssal>=${ABYSSAL_MINIMUM_VERSION})
pkg_check_modules(abyssparse_dep REQUIRED IMPORTED_TARGET abyssparse>=${ABYSSPARSE_MINIMUM_VERSION})
pkg_check_modules(abysscursor_dep REQUIRED IMPORTED_TARGET abysscursor>=${ABYSSCURSOR_MINIMUM_VERSION})
pkg_check_modules(abyssutils_dep REQUIRED IMPORTED_TARGET abyssutils>=${ABYSSUTILS_MINIMUM_VERSION})
pkg_check_modules(abyssgraphics_dep REQUIRED IMPORTED_TARGET abyssgraphics>=${ABYSSGRAPHICS_MINIMUM_VERSION})

string(REPLACE "." ";" ABYSSAL_VERSION_LIST ${abyssal_dep_VERSION})
list(GET ABYSSAL_VERSION_LIST 0 ABYSSAL_VERSION_MAJOR)
list(GET ABYSSAL_VERSION_LIST 1 ABYSSAL_VERSION_MINOR)
list(GET ABYSSAL_VERSION_LIST 2 ABYSSAL_VERSION_PATCH)

set(ABYSSAL_VERSION "${abyssal_dep_VERSION}")
set(ABYSSAL_VERSION_MAJOR "${ABYSSAL_VERSION_MAJOR}")
set(ABYSSAL_VERSION_MINOR "${ABYSSAL_VERSION_MINOR}")
set(ABYSSAL_VERSION_PATCH "${ABYSSAL_VERSION_PATCH}")
set(ABYSSPARSE_VERSION "${abyssparse_dep_VERSION}")
set(ABYSSUTILS_VERSION "${abyssutils_dep_VERSION}")
set(ABYSSCURSOR_VERSION "${abysscursor_dep_VERSION}")
set(ABYSSGRAPHICS_VERSION "${abyssgraphics_dep_VERSION}")


find_package(Git QUIET)

# Populate variables with env vars if present
set(GIT_COMMIT_HASH "$ENV{GIT_COMMIT_HASH}")
if(NOT GIT_COMMIT_HASH)
  set(GIT_COMMIT_HASH "unknown")
endif()

set(GIT_BRANCH "$ENV{GIT_BRANCH}")
if(NOT GIT_BRANCH)
  set(GIT_BRANCH "unknown")
endif()

set(GIT_COMMIT_MESSAGE "$ENV{GIT_COMMIT_MESSAGE}")
if(NOT GIT_COMMIT_MESSAGE)
  set(GIT_COMMIT_MESSAGE "unknown")
endif()

set(GIT_COMMIT_DATE "$ENV{GIT_COMMIT_DATE}")
if(NOT GIT_COMMIT_DATE)
  set(GIT_COMMIT_DATE "unknown")
endif()

set(GIT_DIRTY "$ENV{GIT_DIRTY}")
if(NOT GIT_DIRTY)
  set(GIT_DIRTY "unknown")
endif()

set(GIT_TAG "$ENV{GIT_TAG}")
if(NOT GIT_TAG)
  set(GIT_TAG "unknown")
endif()

set(GIT_COMMITS "$ENV{GIT_COMMITS}")
if(NOT GIT_COMMITS)
  set(GIT_COMMITS "0")
endif()

if(Git_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --show-toplevel
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TOPLEVEL
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_TOPLEVEL_RESULT
  )

  if(GIT_TOPLEVEL_RESULT EQUAL 0)
    message(STATUS "Detected git repository root: ${GIT_TOPLEVEL}")

    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
      WORKING_DIRECTORY ${GIT_TOPLEVEL}
      OUTPUT_VARIABLE GIT_COMMIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${GIT_EXECUTABLE} branch --show-current
      WORKING_DIRECTORY ${GIT_TOPLEVEL}
      OUTPUT_VARIABLE GIT_BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND sh "-c" "${GIT_EXECUTABLE} show -s --format=%s --no-show-signature | sed \"s/\\\"/\'/g\""
      WORKING_DIRECTORY ${GIT_TOPLEVEL}
      OUTPUT_VARIABLE GIT_COMMIT_MESSAGE OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${GIT_EXECUTABLE} show -s --format=%cd --date=local --no-show-signature
      WORKING_DIRECTORY ${GIT_TOPLEVEL}
      OUTPUT_VARIABLE GIT_COMMIT_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${GIT_EXECUTABLE} diff-index --quiet HEAD --
      WORKING_DIRECTORY ${GIT_TOPLEVEL}
      RESULT_VARIABLE GIT_DIRTY_RESULT)
    if(NOT GIT_DIRTY_RESULT EQUAL 0)
      set(GIT_DIRTY "dirty")
    else()
      set(GIT_DIRTY "clean")
    endif()
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags
      WORKING_DIRECTORY ${GIT_TOPLEVEL}
      OUTPUT_VARIABLE GIT_TAG OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-list --count HEAD
      WORKING_DIRECTORY ${GIT_TOPLEVEL}
      OUTPUT_VARIABLE GIT_COMMITS OUTPUT_STRIP_TRAILING_WHITESPACE)
  else()
    message(WARNING "No Git repository detected in ${CMAKE_SOURCE_DIR}")
  endif()
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/src/version.h.in
    ${CMAKE_SOURCE_DIR}/src/version.h
    @ONLY
)

set_source_files_properties(${CMAKE_SOURCE_DIR}/src/version.h PROPERTIES GENERATED TRUE)

set(XKBCOMMON_MINIMUM_VERSION 1.11.0)
set(WAYLAND_SERVER_MINIMUM_VERSION 1.22.91)
set(WAYLAND_SERVER_PROTOCOLS_MINIMUM_VERSION 1.49)
set(LIBINPUT_MINIMUM_VERSION 1.29)

pkg_check_modules(deps
  REQUIRED
  IMPORTED_TARGET GLOBAL
  xkbcommon>=${XKBCOMMON_MINIMUM_VERSION}
  uuid
  wayland-server>=${WAYLAND_SERVER_MINIMUM_VERSION}
  wayland-protocols>=${WAYLAND_SERVER_PROTOCOLS_MINIMUM_VERSION}
  cairo
  pango
  pangocairo
  pixman-1
  xcursor
  libdrm
  libinput>=${LIBINPUT_MINIMUM_VERSION}
  libeis-1.0
  gbm
  gio-2.0
  re2
  muparser
  lcms2
)

pkg_search_module(LUA REQUIRED IMPORTED_TARGET GLOBAL lua55 lua5.5 lua-55 lua-5.5 lua>=5.5 lua<5.6)

find_package(abyss-scanner 0.3.10 REQUIRED)

file(GLOB_RECURSE SRCFILES "src/*.cpp")
get_filename_component(FULL_MAIN_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp ABSOLUTE)
list(REMOVE_ITEM SRCFILES "${FULL_MAIN_PATH}")

set(TRACY_CPP_FILES "")
if(USE_TRACY)
  set(TRACY_CPP_FILES "subprojects/tracy/public/TracyClient.cpp")
  message(STATUS "Tracy enabled, TRACY_CPP_FILES: " ${TRACY_CPP_FILES})
endif()

add_library(abyssland_lib STATIC ${SRCFILES})
add_executable(Abyssland src/main.cpp ${TRACY_CPP_FILES})
target_link_libraries(Abyssland abyssland_lib)

target_include_directories(abyssland_lib PUBLIC ${deps_INCLUDE_DIRS} ${LUA_INCLUDE_DIRS})
target_include_directories(Abyssland PUBLIC ${deps_INCLUDE_DIRS} ${LUA_INCLUDE_DIRS})
target_link_libraries(abyssland_lib PRIVATE glaze::glaze)

set(USE_GPROF OFF)

if(CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES DEBUG)
  message(STATUS "Setting debug flags")

  if(WITH_ASAN)
    message(STATUS "Enabling ASan")

    target_link_libraries(abyssland_lib PUBLIC asan)
    target_compile_options(abyssland_lib PUBLIC -fsanitize=address)
  endif()

  add_compile_options(-fno-pie -fno-builtin -fno-omit-frame-pointer)
  add_link_options(-no-pie -fno-builtin)
  if(USE_GPROF)
    add_compile_options(-pg)
    add_link_options(-pg)
  endif()
endif()

if(USE_TRACY)
  message(STATUS "Tracy is turned on")

  option(TRACY_ENABLE "" ON)
  option(TRACY_ON_DEMAND "" ON)
  add_subdirectory(subprojects/tracy)

  add_compile_options(-fno-omit-frame-pointer)

  target_link_libraries(abyssland_lib PUBLIC Tracy::TracyClient)

  if(USE_TRACY_GPU)
    message(STATUS "Tracy GPU Profiling is turned on")
    add_compile_definitions(USE_TRACY_GPU)
  endif()
endif()

if(BUILT_WITH_NIX)
  add_compile_definitions(BUILT_WITH_NIX)
endif()

check_include_file("execinfo.h" EXECINFOH)
if(EXECINFOH)
  message(STATUS "Configuration supports execinfo")
  add_compile_definitions(HAS_EXECINFO)
endif()

include(CheckLibraryExists)
check_library_exists(execinfo backtrace "" HAVE_LIBEXECINFO)
if(HAVE_LIBEXECINFO)
  target_link_libraries(abyssland_lib PUBLIC execinfo)
endif()

check_include_file("sys/timerfd.h" HAS_TIMERFD)
pkg_check_modules(epoll IMPORTED_TARGET epoll-shim)
if(NOT HAS_TIMERFD AND epoll_FOUND)
  target_link_libraries(abyssland_lib PUBLIC PkgConfig::epoll)
endif()

check_include_file("sys/inotify.h" HAS_INOTIFY)
pkg_check_modules(inotify IMPORTED_TARGET libinotify)
if(NOT HAS_INOTIFY AND inotify_FOUND)
  target_link_libraries(abyssland_lib PUBLIC PkgConfig::inotify)
endif()

if(NO_XWAYLAND)
  message(STATUS "Using the NO_XWAYLAND flag, disabling XWayland!")
  add_compile_definitions(NO_XWAYLAND)
else()
  message(STATUS "XWAYLAND Enabled (NO_XWAYLAND not defined) checking deps...")
  set(XWAYLAND_DEPENDENCIES
    xcb
    xcb-render
    xcb-xfixes
    xcb-icccm
    xcb-composite
    xcb-res
    xcb-errors)

  pkg_check_modules(
    xdeps
    REQUIRED
    IMPORTED_TARGET
    ${XWAYLAND_DEPENDENCIES})

  string(JOIN ", " PKGCONFIG_XWAYLAND_DEPENDENCIES ${XWAYLAND_DEPENDENCIES})
  string(PREPEND PKGCONFIG_XWAYLAND_DEPENDENCIES ", ")

  target_link_libraries(abyssland_lib PUBLIC PkgConfig::xdeps)
endif()

configure_file(abyssland.pc.in abyssland.pc @ONLY)

if(NO_SYSTEMD)
  message(STATUS "SYSTEMD support is disabled...")
else()
  message(STATUS "SYSTEMD support is requested (NO_SYSTEMD not defined)...")
  add_compile_definitions(USES_SYSTEMD)

  # session file -uwsm
  if(NO_UWSM)
    message(STATUS "UWSM support is disabled...")
  else()
    message(STATUS "UWSM support is enabled (NO_UWSM not defined)...")
    install(FILES ${CMAKE_SOURCE_DIR}/systemd/abyssland-uwsm.desktop
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/wayland-sessions)
  endif()
endif()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

if(CMAKE_DISABLE_PRECOMPILE_HEADERS)
  message(STATUS "Not using precompiled headers")
else()
  message(STATUS "Setting precompiled headers")
  target_precompile_headers(abyssland_lib PRIVATE
                            $<$<COMPILE_LANGUAGE:CXX>:src/pch/pch.hpp>)
endif()

message(STATUS "Setting link libraries")

target_link_libraries(
  abyssland_lib
  PUBLIC
  PkgConfig::abyssal_dep
  PkgConfig::abyssparse_dep
  PkgConfig::abyssutils_dep
  PkgConfig::abysscursor_dep
  PkgConfig::abyssgraphics_dep
  PkgConfig::deps
  PkgConfig::LUA
)

target_link_libraries(
  Abyssland
  ${LIBRT}
  abyssland_lib)
if(udis_dep_FOUND)
  target_link_libraries(abyssland_lib PUBLIC PkgConfig::udis_dep)
elseif(NOT("${udis_nopc}" MATCHES "udis_nopc-NOTFOUND"))
  target_link_libraries(abyssland_lib PUBLIC ${udis_nopc})
else()
  target_link_libraries(abyssland_lib PUBLIC libudis86)
endif()

# used by `make installheaders`, to ensure the headers are generated
add_custom_target(generate-protocol-headers)

if(Python3_Interpreter_FOUND)
  add_custom_target(
    generate-lua-stubs ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/meta/generateLuaStubs.py --root ${CMAKE_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating Lua API stubs for LuaLS"
    VERBATIM)

  add_dependencies(abyssland_lib generate-lua-stubs)
  add_dependencies(Abyssland generate-lua-stubs)

  add_custom_target(
    check-lua-stubs
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/meta/generateLuaStubs.py --root ${CMAKE_SOURCE_DIR} --check
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking Lua API stubs are up to date"
    VERBATIM)
else()
  message(FATAL_ERROR "Python3 interpreter not found, which is required for the build")
endif()

set(PROTOCOL_SOURCES "")

function(protocolnew protoPath protoName external)
  if(external)
    set(path ${protoPath})
  else()
    set(path ${WAYLAND_PROTOCOLS_DIR}/${protoPath})
  endif()
  add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/protocols/${protoName}.cpp
           ${CMAKE_SOURCE_DIR}/protocols/${protoName}.hpp
    COMMAND abyss-scanner ${path}/${protoName}.xml
            ${CMAKE_SOURCE_DIR}/protocols/
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  target_sources(abyssland_lib PRIVATE protocols/${protoName}.cpp
                                  protocols/${protoName}.hpp)
  target_sources(generate-protocol-headers
                 PRIVATE ${CMAKE_SOURCE_DIR}/protocols/${protoName}.hpp)

  list(APPEND PROTOCOL_SOURCES "${CMAKE_SOURCE_DIR}/protocols/${protoName}.cpp")
  set(PROTOCOL_SOURCES "${PROTOCOL_SOURCES}" PARENT_SCOPE)
  list(APPEND PROTOCOL_SOURCES "${CMAKE_SOURCE_DIR}/protocols/${protoName}.hpp")
  set(PROTOCOL_SOURCES "${PROTOCOL_SOURCES}" PARENT_SCOPE)
endfunction()
function(protocolWayland)
  add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/protocols/wayland.cpp
           ${CMAKE_SOURCE_DIR}/protocols/wayland.hpp
    COMMAND
      abyss-scanner --wayland-enums
      ${WAYLAND_SCANNER_PKGDATA_DIR}/wayland.xml ${CMAKE_SOURCE_DIR}/protocols/
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  target_sources(abyssland_lib PRIVATE protocols/wayland.cpp protocols/wayland.hpp)
  target_sources(generate-protocol-headers
                 PRIVATE ${CMAKE_SOURCE_DIR}/protocols/wayland.hpp)

  list(APPEND PROTOCOL_SOURCES "${CMAKE_SOURCE_DIR}/protocols/wayland.hpp")
  set(PROTOCOL_SOURCES "${PROTOCOL_SOURCES}" PARENT_SCOPE)
  list(APPEND PROTOCOL_SOURCES "${CMAKE_SOURCE_DIR}/protocols/wayland.cpp")
  set(PROTOCOL_SOURCES "${PROTOCOL_SOURCES}" PARENT_SCOPE)
endfunction()

if(TARGET OpenGL::GL)
  target_link_libraries(abyssland_lib PUBLIC OpenGL::EGL OpenGL::GL glslang::glslang glslang::glslang-default-resource-limits glslang::SPIRV Threads::Threads)
else()
  target_link_libraries(abyssland_lib PUBLIC OpenGL::EGL OpenGL::GLES3 glslang::glslang glslang::glslang-default-resource-limits glslang::SPIRV Threads::Threads)
endif()

pkg_check_modules(abyssland_protocols_dep abyssland-protocols>=0.7.0)
if(abyssland_protocols_dep_FOUND)
  pkg_get_variable(ABYSSLAND_PROTOCOLS abyssland-protocols pkgdatadir)
  message(STATUS "abyssland-protocols dependency set to ${ABYSSLAND_PROTOCOLS}")
else()
  set(ABYSSLAND_PROTOCOLS "subprojects/abyssland-protocols")
  message(STATUS "abyssland-protocols subproject set to ${ABYSSLAND_PROTOCOLS}")
endif()

protocolnew("${ABYSSLAND_PROTOCOLS}/protocols" "abyssland-global-shortcuts-v1"
            true)
protocolnew("unstable/text-input" "text-input-unstable-v1" false)
protocolnew("${ABYSSLAND_PROTOCOLS}/protocols" "abyssland-toplevel-export-v1"
            true)
protocolnew("protocols" "wlr-screencopy-unstable-v1" true)
protocolnew("protocols" "wlr-gamma-control-unstable-v1" true)
protocolnew("protocols" "wlr-foreign-toplevel-management-unstable-v1" true)
protocolnew("protocols" "wlr-output-power-management-unstable-v1" true)
protocolnew("protocols" "virtual-keyboard-unstable-v1" true)
protocolnew("protocols" "wlr-virtual-pointer-unstable-v1" true)
protocolnew("protocols" "input-method-unstable-v2" true)
protocolnew("protocols" "wlr-output-management-unstable-v1" true)
protocolnew("protocols" "kde-server-decoration" true)
protocolnew("protocols" "vicinae-hotkey-v1" true)
protocolnew("protocols" "wlr-data-control-unstable-v1" true)
protocolnew("${ABYSSLAND_PROTOCOLS}/protocols" "abyssland-focus-grab-v1" true)
protocolnew("protocols" "wlr-layer-shell-unstable-v1" true)
protocolnew("protocols" "wayland-drm" true)
protocolnew("${ABYSSLAND_PROTOCOLS}/protocols" "abyssland-ctm-control-v1" true)
protocolnew("${ABYSSLAND_PROTOCOLS}/protocols" "abyssland-surface-v1" true)
protocolnew("${ABYSSLAND_PROTOCOLS}/protocols" "abyssland-lock-notify-v1" true)
protocolnew("${ABYSSLAND_PROTOCOLS}/protocols" "abyssland-toplevel-mapping-v1"
            true)

protocolnew("staging/tearing-control" "tearing-control-v1" false)
protocolnew("staging/fractional-scale" "fractional-scale-v1" false)
protocolnew("unstable/xdg-output" "xdg-output-unstable-v1" false)
protocolnew("staging/cursor-shape" "cursor-shape-v1" false)
protocolnew("unstable/idle-inhibit" "idle-inhibit-unstable-v1" false)
protocolnew("unstable/relative-pointer" "relative-pointer-unstable-v1" false)
protocolnew("unstable/xdg-decoration" "xdg-decoration-unstable-v1" false)
protocolnew("staging/alpha-modifier" "alpha-modifier-v1" false)
protocolnew("staging/ext-foreign-toplevel-list" "ext-foreign-toplevel-list-v1"
            false)
protocolnew("unstable/pointer-gestures" "pointer-gestures-unstable-v1" false)
protocolnew("unstable/keyboard-shortcuts-inhibit"
            "keyboard-shortcuts-inhibit-unstable-v1" false)
protocolnew("unstable/text-input" "text-input-unstable-v3" false)
protocolnew("unstable/pointer-constraints" "pointer-constraints-unstable-v1"
            false)
protocolnew("unstable/xdg-foreign" "xdg-foreign-unstable-v2" false)
protocolnew("staging/xdg-activation" "xdg-activation-v1" false)
protocolnew("staging/ext-idle-notify" "ext-idle-notify-v1" false)
protocolnew("staging/ext-session-lock" "ext-session-lock-v1" false)
protocolnew("stable/tablet" "tablet-v2" false)
protocolnew("stable/presentation-time" "presentation-time" false)
protocolnew("stable/xdg-shell" "xdg-shell" false)
protocolnew("unstable/primary-selection" "primary-selection-unstable-v1" false)
protocolnew("staging/xwayland-shell" "xwayland-shell-v1" false)
protocolnew("stable/viewporter" "viewporter" false)
protocolnew("stable/linux-dmabuf" "linux-dmabuf-v1" false)
protocolnew("staging/drm-lease" "drm-lease-v1" false)
protocolnew("staging/linux-drm-syncobj" "linux-drm-syncobj-v1" false)
protocolnew("staging/xdg-dialog" "xdg-dialog-v1" false)
protocolnew("staging/single-pixel-buffer" "single-pixel-buffer-v1" false)
protocolnew("staging/security-context" "security-context-v1" false)
protocolnew("staging/content-type" "content-type-v1" false)
protocolnew("staging/color-management" "color-management-v1" false)
protocolnew("staging/xdg-toplevel-tag" "xdg-toplevel-tag-v1" false)
protocolnew("staging/xdg-system-bell" "xdg-system-bell-v1" false)
protocolnew("staging/ext-workspace" "ext-workspace-v1" false)
protocolnew("staging/ext-data-control" "ext-data-control-v1" false)
protocolnew("staging/pointer-warp" "pointer-warp-v1" false)
protocolnew("staging/fifo" "fifo-v1" false)
protocolnew("staging/commit-timing" "commit-timing-v1" false)
protocolnew("staging/ext-image-capture-source" "ext-image-capture-source-v1" false)
protocolnew("staging/ext-image-copy-capture" "ext-image-copy-capture-v1" false)
protocolnew("staging/ext-background-effect" "ext-background-effect-v1" false)

protocolnew("${ABYSSLAND_PROTOCOLS}/protocols" "abyssland-input-capture-v1"
            true)
protocolwayland()

# tools
add_subdirectory(abyssctl)
add_subdirectory(start)

if(NO_ABYSSPM)
  message(STATUS "abysspm is disabled")
else()
  add_subdirectory(abysspm)
  message(STATUS "abysspm is enabled (NO_ABYSSPM not defined)")
endif()

# Set proper permissions for directories, otherwise they could end up 700
set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

# binary and symlink
install(TARGETS Abyssland)

install(
  CODE "execute_process( \
        COMMAND ${CMAKE_COMMAND} -E create_symlink \
        ${CMAKE_INSTALL_FULL_BINDIR}/Abyssland \
        \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}/abyssland\" \
        )")
# session file
configure_file(
    ${CMAKE_SOURCE_DIR}/example/abyssland.desktop.in
    ${CMAKE_SOURCE_DIR}/example/abyssland.desktop
    @ONLY
)
install(FILES ${CMAKE_SOURCE_DIR}/example/abyssland.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/wayland-sessions)

# allow Abyssland to find assets
add_compile_definitions(DATAROOTDIR="${CMAKE_INSTALL_FULL_DATAROOTDIR}")

# installable assets
file(GLOB_RECURSE INSTALLABLE_ASSETS "assets/install/*")
install(FILES ${INSTALLABLE_ASSETS}
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/abyss)

# default lua config
install(FILES ${CMAKE_SOURCE_DIR}/example/abyssland.lua
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/abyss)

# LuaLS stubs
install(FILES ${CMAKE_SOURCE_DIR}/meta/hl.meta.lua
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/abyss/stubs)

# portal config
install(FILES ${CMAKE_SOURCE_DIR}/assets/abyssland-portals.conf
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/xdg-desktop-portal)

# man pages
file(GLOB_RECURSE MANPAGES "docs/*.1")
install(FILES ${MANPAGES} DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

# pkgconfig entry
install(FILES ${CMAKE_BINARY_DIR}/abyssland.pc
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)

# protocol headers
set(HEADERS_PROTO "${CMAKE_CURRENT_SOURCE_DIR}/protocols")
install(
  DIRECTORY ${HEADERS_PROTO}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/abyssland
  FILES_MATCHING
  PATTERN "*.h*")

# abyssland headers
set(HEADERS_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src")
install(
  DIRECTORY ${HEADERS_SRC}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/abyssland
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "*.hpp"
  PATTERN "*.inc")

if(BUILD_TESTING OR WITH_TESTS)
  message(STATUS "Building tests")

  # abysstester
  add_subdirectory(abysstester)

  # GTest
  find_package(GTest CONFIG REQUIRED)
  include(GoogleTest)
  file(GLOB_RECURSE TESTFILES "tests/*.cpp")
  add_executable(abyssland_gtests ${TESTFILES})
  target_compile_options(abyssland_gtests PRIVATE --coverage)
  target_link_options(abyssland_gtests PRIVATE --coverage)
  target_include_directories(
    abyssland_gtests
    PUBLIC "./include"
    PRIVATE "./src" "./src/include" "./protocols" "${CMAKE_BINARY_DIR}")

  target_link_libraries(abyssland_gtests abyssland_lib GTest::gtest_main)

  gtest_discover_tests(abyssland_gtests)

  # Enable coverage in main abyssland lib
  target_compile_options(abyssland_lib PRIVATE --coverage)
  target_link_options(abyssland_lib PRIVATE --coverage)
  target_link_libraries(abyssland_lib PUBLIC gcov)

  # Enable coverage in abyssland exe
  target_compile_options(Abyssland PRIVATE --coverage)
  target_link_options(Abyssland PRIVATE --coverage)
  target_link_libraries(Abyssland gcov)
endif()

if(BUILD_TESTING)
  message(STATUS "Testing is enabled")

  enable_testing()
  add_custom_target(tests)

  add_dependencies(tests abyssland_gtests)

else()
  message(STATUS "Testing is disabled")
endif()
