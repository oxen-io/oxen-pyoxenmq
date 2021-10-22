cmake_minimum_required(VERSION 3.10)  # bionic's cmake version


find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

project(pyoxenmq
    VERSION 1.0.0
    DESCRIPTION "python interface to OxenMQ"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SUBMODULE_CHECK "Enables checking that vendored library submodules are up to date" ON)
if(SUBMODULE_CHECK)
  find_package(Git)
  if(GIT_FOUND)
    function(check_submodule relative_path)
      execute_process(COMMAND git rev-parse "HEAD" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${relative_path} OUTPUT_VARIABLE localHead)
      execute_process(COMMAND git rev-parse "HEAD:${relative_path}" WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE checkedHead)
      string(COMPARE EQUAL "${localHead}" "${checkedHead}" upToDate)
      if (upToDate)
        message(STATUS "Submodule '${relative_path}' is up-to-date")
      else()
        message(FATAL_ERROR "Submodule '${relative_path}' is not up-to-date. Please update with\ngit submodule update --init --recursive\nor run cmake with -DSUBMODULE_CHECK=OFF")
      endif()
    endfunction ()
    
    check_submodule(external/pybind11)
    check_submodule(external/oxen-mq)
  endif()
endif()

add_subdirectory(external/pybind11)

pybind11_add_module(oxenmq MODULE
    src/bencode.cpp
    src/oxenmq.cpp
    src/module.cpp
)

if(TARGET oxenmq::oxenmq)
    target_link_libraries(pyoxenmq PRIVATE oxenmq::oxenmq)
else()
    include(FindPkgConfig)
    pkg_check_modules(oxenmq REQUIRED IMPORTED_TARGET liboxenmq>=1.2.8)
    target_link_libraries(oxenmq PRIVATE PkgConfig::oxenmq)
endif()
