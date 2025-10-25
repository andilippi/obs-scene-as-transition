# --- Detect if the plugin is built out of tree or not ---
if(CMAKE_PROJECT_NAME STREQUAL "obs-studio")
  set(BUILD_OUT_OF_TREE OFF)
else()
  set(BUILD_OUT_OF_TREE ON)
  cmake_minimum_required(VERSION 3.16...3.26)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${PROJECT_NAME} MODULE)

target_link_libraries(${PROJECT_NAME} PRIVATE OBS::libobs)

if(BUILD_OUT_OF_TREE)
  find_package(libobs REQUIRED)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE OBS::frontend-api)
endif()

# Qt
find_package(Qt6 COMPONENTS Widgets Core)
if(BUILD_OUT_OF_TREE)
  if(OS_LINUX OR OS_FREEBSD OR OS_OPENBSD)
    find_package(Qt6 REQUIRED Gui)
  endif()
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Core Qt::Widgets)

# CURL
find_package(CURL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)

# Determine shared directory path (support both in-tree and out-of-tree builds)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shared")
  set(OBS_SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shared")
else()
  set(OBS_SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../shared")
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OBS_SHARED_DIR}
)

# Extra Qt linkage on older Qt versions for *nix
if((OS_LINUX OR OS_FREEBSD OR OS_OPENBSD) AND Qt_VERSION VERSION_LESS "6.9.0")
  target_link_libraries(${PROJECT_NAME} PRIVATE Qt::GuiPrivate)
endif()

# Compiler / build flags
target_compile_options(${PROJECT_NAME} PRIVATE
  $<$<C_COMPILER_ID:Clang,AppleClang>:
    -Wno-quoted-include-in-framework-header
    -Wno-comma
  >
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  AUTOMOC ON
  AUTOUIC ON
  AUTORCC ON
)

# Version header
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
  ${CMAKE_CURRENT_SOURCE_DIR}/version.h
)

# Windows installer script template
if(OS_WINDOWS)
  configure_file(
    cmake/windows/resources/installer-Windows.iss.in
    "${CMAKE_CURRENT_BINARY_DIR}/installer-Windows.generated.iss"
  )
endif()

# Sources
target_sources(${PROJECT_NAME} PRIVATE
	scene-as-transition.c
	version.h)

# Install / properties depending on build context
if(BUILD_OUT_OF_TREE)
  # out-of-tree plugin build
  set_target_properties_plugin(${PROJECT_NAME}
    PROPERTIES
      OUTPUT_NAME ${_name}
  )
else()
  # in-tree OBS build
  set_target_properties_obs(${PROJECT_NAME}
    PROPERTIES
      FOLDER "plugins/streamup"
      PREFIX ""
  )
endif()
