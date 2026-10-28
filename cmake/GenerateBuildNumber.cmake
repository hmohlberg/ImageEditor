# cmake/GenerateBuildNumber.cmake
# Two calling modes (select via -DMODE=):
#   init       (default)  read current values, write header — no increment
#   post_build            called after a successful link; increments local counter
#
# Strategy:
#   git available  →  BUILD_NUMBER = commit count, BUILD_HASH = short hash  (server)
#   no git         →  BUILD_NUMBER read from build_number.txt; incremented in post_build
#
# Required inputs:
#   SOURCE_DIR   — project root (where build_number.txt lives)
#   BINARY_DIR   — cmake binary dir (where core/build_number.h is written)

if(NOT DEFINED MODE)
    set(MODE "init")
endif()

set(_file "${SOURCE_DIR}/build_number.txt")
set(BUILD_HASH "")
set(BUILD_NUMBER "")

# --- Try git (server path) ---
find_program(GIT_EXEC NAMES git)
if(GIT_EXEC)
    execute_process(
        COMMAND "${GIT_EXEC}" -C "${SOURCE_DIR}" rev-list --count HEAD
        OUTPUT_VARIABLE _git_count
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _rc_count
    )
    execute_process(
        COMMAND "${GIT_EXEC}" -C "${SOURCE_DIR}" rev-parse --short HEAD
        OUTPUT_VARIABLE _git_hash
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _rc_hash
    )
    if(_rc_count EQUAL 0 AND _git_count MATCHES "^[0-9]+$")
        set(BUILD_NUMBER "${_git_count}")
        if(_rc_hash EQUAL 0 AND _git_hash)
            set(BUILD_HASH "${_git_hash}")
        endif()
    endif()
endif()

# --- Fallback: no git — file-based counter ---
if(NOT BUILD_NUMBER)
    if(EXISTS "${_file}")
        file(READ "${_file}" _stored)
        string(STRIP "${_stored}" _stored)
    else()
        set(_stored "0")
    endif()
    if(NOT _stored MATCHES "^[0-9]+$")
        set(_stored "0")
    endif()

    if(MODE STREQUAL "post_build")
        # Increment only after a successful link (binary actually changed).
        math(EXPR BUILD_NUMBER "${_stored} + 1")
        file(WRITE "${_file}" "${BUILD_NUMBER}\n")
    else()
        set(BUILD_NUMBER "${_stored}")
    endif()
endif()

# --- Compose build string ---
if(BUILD_HASH)
    set(BUILD_STRING "${BUILD_NUMBER}-${BUILD_HASH}")
else()
    set(BUILD_STRING "${BUILD_NUMBER}")
endif()

# --- Write header only when content changed (avoids unnecessary recompilation) ---
set(_header
"#pragma once\n\
#define APP_BUILD_NUMBER ${BUILD_NUMBER}\n\
#define APP_BUILD_HASH   \"${BUILD_HASH}\"\n\
#define APP_BUILD_STRING \"${BUILD_STRING}\"\n")

set(_out "${BINARY_DIR}/core/build_number.h")
set(_existing "")
if(EXISTS "${_out}")
    file(READ "${_out}" _existing)
endif()
if(NOT _header STREQUAL _existing)
    file(WRITE "${_out}" "${_header}")
    if(MODE STREQUAL "post_build")
        message(STATUS "Build number updated: ${BUILD_STRING}")
    endif()
endif()
