cmake_minimum_required(VERSION 3.16)
project(ftl_reliability_simulator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(FTL_BUILD_TESTS "Build the GoogleTest suite" ON)
option(FTL_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(FTL_WERROR "Treat compiler warnings as errors (CI)" OFF)

# ---------------------------------------------------------------------------
# Build provenance: recorded in every run.json so a replay can be tied to code.
# ---------------------------------------------------------------------------
set(FTL_BUILD_COMMIT "nogit")
find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE _ftl_commit
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _ftl_git_rc)
  if(_ftl_git_rc EQUAL 0 AND _ftl_commit)
    set(FTL_BUILD_COMMIT ${_ftl_commit})
  endif()
endif()
message(STATUS "FTL build commit: ${FTL_BUILD_COMMIT}")

function(ftl_apply_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
    if(FTL_WERROR)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
    if(FTL_WERROR)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

function(ftl_apply_sanitizers target)
  if(FTL_SANITIZE AND NOT MSVC)
    target_compile_options(${target} PRIVATE
      -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Core library (all FTL logic lives here, in C++17)
# ---------------------------------------------------------------------------
add_library(ftl_core STATIC
  src/ftl/checksum.cpp
  src/ftl/config.cpp
  src/ftl/types.cpp
  src/ftl/ftl.cpp
  src/nand/nand.cpp
  src/recovery/wal.cpp
  src/recovery/checkpoint.cpp
  src/recovery/crash.cpp
  src/invariants/invariants.cpp
  src/workload/trace.cpp
  src/workload/runner.cpp
  src/workload/report.cpp
)
target_include_directories(ftl_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(ftl_core SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_compile_definitions(ftl_core PUBLIC FTL_BUILD_COMMIT="${FTL_BUILD_COMMIT}")
ftl_apply_warnings(ftl_core)
ftl_apply_sanitizers(ftl_core)

add_executable(ftl-sim src/main.cpp)
target_link_libraries(ftl-sim PRIVATE ftl_core)
ftl_apply_warnings(ftl-sim)
ftl_apply_sanitizers(ftl-sim)
if(MINGW)
  # Static runtime so Python orchestration can spawn the binary without DLL lookup issues.
  target_link_options(ftl-sim PRIVATE -static)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(FTL_BUILD_TESTS)
  enable_testing()
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(googletest)
  endif()

  add_executable(ftl_tests
    tests/test_helpers.cpp
    tests/unit/test_checksum.cpp
    tests/unit/test_config.cpp
    tests/unit/test_nand.cpp
    tests/unit/test_wal.cpp
    tests/unit/test_checkpoint.cpp
    tests/unit/test_trace.cpp
    tests/unit/test_mapping.cpp
    tests/integration/test_gc.cpp
    tests/integration/test_wear_op.cpp
    tests/integration/test_restart.cpp
    tests/crash/test_crash_points.cpp
    tests/crash/test_vulnerable_defect.cpp
    tests/regression/test_seed_regression.cpp
  )
  target_include_directories(ftl_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_link_libraries(ftl_tests PRIVATE ftl_core GTest::gtest_main)
  ftl_apply_warnings(ftl_tests)
  ftl_apply_sanitizers(ftl_tests)
  if(MINGW)
    target_link_options(ftl_tests PRIVATE -static)
  endif()

  include(GoogleTest)
  gtest_discover_tests(ftl_tests DISCOVERY_TIMEOUT 120)
endif()
