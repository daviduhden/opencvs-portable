# Portable OpenCVS.
#
# Copyright (c) 2026, David Uhden Collado <david@uhden.dev>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

cmake_minimum_required(VERSION 3.20)

get_filename_component(PC_MANUAL_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${CMAKE_CURRENT_LIST_DIR}/ManualPages.cmake")
find_program(PC_CHM_PERL NAMES perl REQUIRED)
find_program(PC_CHM_MANDOC NAMES mandoc REQUIRED)
set(generator "${PC_MANUAL_SOURCE_DIR}/scripts/generate-chm.pl")
if(PC_CHM_LINT_ONLY)
  execute_process(
    COMMAND "${PC_CHM_PERL}" "${generator}" --mandoc "${PC_CHM_MANDOC}"
      --lint-only -- ${PC_MANUAL_PAGES}
    COMMAND_ERROR_IS_FATAL ANY)
  return()
endif()

if(NOT PC_CHM_OUTPUT_DIR)
  message(FATAL_ERROR "Set PC_CHM_OUTPUT_DIR to the directory for Windows help")
endif()
cmake_path(ABSOLUTE_PATH PC_CHM_OUTPUT_DIR NORMALIZE)
if(NOT PC_CHM_VERSION)
  file(STRINGS "${PC_MANUAL_SOURCE_DIR}/CMakeLists.txt" project
    REGEX "^project\\(PortableOpenCVS VERSION [0-9.]+ ")
  if(NOT project MATCHES "VERSION ([0-9.]+) ")
    message(FATAL_ERROR "Cannot find the Portable OpenCVS version")
  endif()
  set(PC_CHM_VERSION "${CMAKE_MATCH_1}")
endif()

find_program(PC_CHM_COMPILER NAMES hhc chmcmd chmcmd-3.2.2
  HINTS "$ENV{ProgramFiles}/HTML Help Workshop"
        "$ENV{ProgramFiles\(x86\)}/HTML Help Workshop"
  REQUIRED)
find_program(PC_CHM_7ZIP NAMES 7z 7zz HINTS "$ENV{ProgramFiles}/7-Zip" REQUIRED)
get_filename_component(compiler_name "${PC_CHM_COMPILER}" NAME)
string(TOLOWER "${compiler_name}" compiler_name)
if(compiler_name MATCHES "^hhc([.]exe)?$")
  set(success_code "1")
  set(compiler_args opencvs.hhp)
elseif(compiler_name MATCHES "^chmcmd(-[0-9.]+)?([.]exe)?$")
  set(success_code "0")
  set(compiler_args --no-html-scan opencvs.hhp)
else()
  message(FATAL_ERROR "Use hhc.exe or Free Pascal chmcmd for PC_CHM_COMPILER")
endif()

set(work "${PC_CHM_OUTPUT_DIR}/project")
set(chm "${work}/opencvs.chm")
file(MAKE_DIRECTORY "${work}")
file(REMOVE "${chm}")
execute_process(
  COMMAND "${PC_CHM_PERL}" "${generator}"
    --mandoc "${PC_CHM_MANDOC}" --output-dir "${work}"
    --version "${PC_CHM_VERSION}" -- ${PC_MANUAL_PAGES}
  COMMAND_ERROR_IS_FATAL ANY)

# Workshop reports success as 1. Its diagnostics and artifact still need checking.
execute_process(
  COMMAND "${PC_CHM_COMPILER}" ${compiler_args}
  WORKING_DIRECTORY "${work}"
  TIMEOUT 120 RESULT_VARIABLE result
  OUTPUT_VARIABLE output ERROR_VARIABLE errors)
file(WRITE "${PC_CHM_OUTPUT_DIR}/compiler.log" "${output}${errors}")
if(NOT "${output}${errors}" STREQUAL "")
  message(STATUS "HTML Help compiler:\n${output}${errors}")
endif()
if(NOT result STREQUAL success_code OR
    "${output}${errors}" MATCHES "(^|[\r\n])[ \t]*([Ee]rror|[Ww]arning|HHC[0-9][0-9][0-9][0-9])")
  message(FATAL_ERROR "HTML Help compilation failed (status ${result})")
endif()
execute_process(
  COMMAND "${PC_CHM_PERL}" "${PC_MANUAL_SOURCE_DIR}/scripts/check-chm.pl"
    --7z "${PC_CHM_7ZIP}" --chm "${chm}" --project "${work}/opencvs.hhp"
  COMMAND_ERROR_IS_FATAL ANY)
file(RENAME "${chm}" "${PC_CHM_OUTPUT_DIR}/opencvs.chm")
message(STATUS "Windows help: ${PC_CHM_OUTPUT_DIR}/opencvs.chm")
