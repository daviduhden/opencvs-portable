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
project(PortableOpenCVS VERSION 0.1.0 LANGUAGES C)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(GNUInstallDirs)
option(PC_BUILD_TESTS "Build the native unit and integration test helpers" ON)
option(PC_REQUIRE_GNU_CVS "Fail configuration when GNU CVS interoperability cannot run" OFF)
option(PC_STRICT_WARNINGS "Treat compiler diagnostics as errors" ON)
option(PC_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
if(PC_SANITIZERS AND (MSVC OR WIN32))
  message(FATAL_ERROR "PC_SANITIZERS requires a tested GCC/Clang POSIX toolchain; Windows sanitizer coverage is not implemented.")
endif()
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED YES)
set(CMAKE_C_EXTENSIONS NO)
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  set(CMAKE_C_STANDARD 11)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # OpenBSD must retain its default BSD namespace: these strict feature macros
  # hide native u_int/u_char, fgetln, strtonum, and other interfaces there.
  add_compile_definitions(_POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700 _DEFAULT_SOURCE _FILE_OFFSET_BITS=64)
  set(CMAKE_REQUIRED_DEFINITIONS -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -D_FILE_OFFSET_BITS=64)
endif()
check_c_source_compiles("#include <time.h>
int main(void) { struct timespec value; value.tv_sec = 0; value.tv_nsec = 0; return (int)value.tv_sec; }" PC_HAVE_STRUCT_TIMESPEC)
check_symbol_exists(strlcpy "string.h" PC_HAVE_STRLCPY)
check_symbol_exists(strlcat "string.h" PC_HAVE_STRLCAT)
check_symbol_exists(strtonum "stdlib.h" PC_HAVE_STRTONUM)
check_symbol_exists(reallocarray "stdlib.h" PC_HAVE_REALLOCARRAY)
check_symbol_exists(strsep "string.h" PC_HAVE_STRSEP)
check_symbol_exists(asprintf "stdio.h" PC_HAVE_ASPRINTF)
check_symbol_exists(vasprintf "stdio.h" PC_HAVE_VASPRINTF)
check_symbol_exists(regcomp "regex.h" PC_HAVE_REGEX)
check_symbol_exists(fnmatch "fnmatch.h" PC_HAVE_FNMATCH)
check_symbol_exists(FNM_CASEFOLD "fnmatch.h" PC_HAVE_FNM_CASEFOLD)
check_symbol_exists(fgetln "stdio.h" PC_HAVE_FGETLN)
check_symbol_exists(getopt "unistd.h" PC_HAVE_GETOPT)
check_symbol_exists(optreset "unistd.h" PC_HAVE_OPTRESET)
if(NOT WIN32)
  # Linux libc exposes pipe2 under _GNU_SOURCE. Scope it to this probe and
  # the process backend so the rest of the port keeps its existing namespace.
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
  endif()
  check_symbol_exists(pipe2 "unistd.h;fcntl.h" PC_HAVE_PIPE2)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(REMOVE_ITEM CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
    if(PC_HAVE_PIPE2)
      set_source_files_properties(compat/linux/process.c PROPERTIES COMPILE_DEFINITIONS _GNU_SOURCE)
    endif()
  endif()
endif()
# The engine resets option parsing between global, command, and rc-file passes.
# Use native getopt only when its reset interface is available too.
if(PC_HAVE_GETOPT AND PC_HAVE_OPTRESET)
  set(PC_HAVE_BSD_GETOPT TRUE)
endif()
# The native headers are preferred when they provide the containers we use.
# These probes run only during authorized CI configuration, never locally.
check_c_source_compiles("#include <stddef.h>
#include <sys/queue.h>
struct item { TAILQ_ENTRY(item) tail; SLIST_ENTRY(item) single; };
TAILQ_HEAD(tail_head, item); SLIST_HEAD(single_head, item);
int main(void) {
  struct tail_head tail; struct single_head single; struct item value;
  TAILQ_INIT(&tail); SLIST_INIT(&single);
  TAILQ_INSERT_HEAD(&tail, &value, tail);
  SLIST_INSERT_HEAD(&single, &value, single);
  TAILQ_REMOVE(&tail, &value, tail);
  SLIST_REMOVE(&single, &value, item, single);
  return !TAILQ_EMPTY(&tail);
}" PC_HAVE_SYS_QUEUE_H)
check_c_source_compiles("#include <stddef.h>
#include <sys/tree.h>
struct item { int key; RB_ENTRY(item) link; };
RB_HEAD(item_tree, item);
static int compare(struct item *left, struct item *right) {
  return (left->key > right->key) - (left->key < right->key);
}
RB_PROTOTYPE_STATIC(item_tree, item, link, compare)
RB_GENERATE_STATIC(item_tree, item, link, compare)
int main(void) {
  struct item_tree tree; struct item value = { 0 };
  RB_INIT(&tree); RB_INSERT(item_tree, &tree, &value);
  RB_REMOVE(item_tree, &tree, &value);
  return !RB_EMPTY(&tree);
}" PC_HAVE_SYS_TREE_H)
# OpenCVS needs the BSD/GNU case-folding extension, not just POSIX fnmatch.
# A fallback must provide both function and flag semantics as a matched pair.
if(NOT PC_HAVE_FNM_CASEFOLD)
  set(PC_HAVE_FNMATCH FALSE)
endif()
configure_file(cmake/portable_config.h.in portable_config.h)
add_library(pc_options INTERFACE)
# Do not expose compat/include/sys unconditionally: that would shadow working
# OpenBSD container headers merely to find our project declarations.
set(PC_PORTABLE_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/portable-include")
file(MAKE_DIRECTORY "${PC_PORTABLE_INCLUDE}")
foreach(header portable.h pc_process.h)
  configure_file("compat/include/${header}" "${PC_PORTABLE_INCLUDE}/${header}" COPYONLY)
endforeach()
target_include_directories(pc_options INTERFACE
  "${CMAKE_CURRENT_BINARY_DIR}" "${PC_PORTABLE_INCLUDE}" src)
if(WIN32)
  target_include_directories(pc_options BEFORE INTERFACE compat/windows/include)
endif()
if(MSVC)
  target_compile_options(pc_options INTERFACE "$<$<COMPILE_LANGUAGE:C>:/FIportable.h;/W4;/utf-8>")
  if(PC_STRICT_WARNINGS)
    target_compile_options(pc_options INTERFACE "$<$<COMPILE_LANGUAGE:C>:/WX>")
  endif()
else()
  target_compile_options(pc_options INTERFACE "$<$<COMPILE_LANGUAGE:C>:-include;portable.h;-Wall;-Wextra;-Wpedantic;-Wformat=2;-Wshadow;-Wstrict-prototypes;-Wmissing-prototypes>")
  if(PC_STRICT_WARNINGS)
    target_compile_options(pc_options INTERFACE "$<$<COMPILE_LANGUAGE:C>:-Werror>")
  endif()
  if(PC_SANITIZERS)
    target_compile_options(pc_options INTERFACE -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    target_link_options(pc_options INTERFACE -fsanitize=address,undefined)
  endif()
endif()
# Both the executable and the test helpers link this compatibility library.
# Select the process backend by platform and the libc fallbacks by probe result.
set(PC_COMPAT compat/common.c)
if(NOT PC_HAVE_BSD_GETOPT)
  list(APPEND PC_COMPAT compat/imported/getopt.c)
endif()
foreach(pair "STRLCPY;strlcpy" "STRLCAT;strlcat" "STRTONUM;strtonum" "REALLOCARRAY;reallocarray" "STRSEP;strsep")
  list(GET pair 0 feature)
  list(GET pair 1 source)
  if(NOT PC_HAVE_${feature})
    list(APPEND PC_COMPAT compat/imported/${source}.c)
  endif()
endforeach()
if(WIN32)
  list(APPEND PC_COMPAT
    compat/windows/date_parse.c
    compat/windows/errors.c
    compat/windows/filesystem.c
    compat/windows/path_parts.c
    compat/windows/runtime.c
    compat/windows/startup.c
    platform/windows/process.c)
else()
  list(APPEND PC_COMPAT compat/linux/process.c)
endif()
if(NOT PC_HAVE_REGEX)
  list(APPEND PC_COMPAT compat/imported/regex/regcomp.c compat/imported/regex/regexec.c compat/imported/regex/regerror.c compat/imported/regex/regfree.c)
endif()
if(NOT PC_HAVE_FNMATCH)
  list(APPEND PC_COMPAT compat/imported/fnmatch.c)
endif()
add_library(pc_compat STATIC ${PC_COMPAT})
target_link_libraries(pc_compat PUBLIC pc_options)
# Expose only the header matching the fallback implementation. Adding the
# whole imported include directory could pair native libc regex functions
# with the incompatible imported regex_t layout when only fnmatch is absent.
set(PC_FALLBACK_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/compat-fallback")
file(MAKE_DIRECTORY "${PC_FALLBACK_INCLUDE}")
# Clear old copies on reconfigure so a native header can become visible again.
file(REMOVE "${PC_FALLBACK_INCLUDE}/regex.h" "${PC_FALLBACK_INCLUDE}/fnmatch.h")
foreach(feature REGEX FNMATCH)
  if(NOT PC_HAVE_${feature})
    string(TOLOWER "${feature}" header)
    configure_file("compat/imported/include/${header}.h"
      "${PC_FALLBACK_INCLUDE}/${header}.h" COPYONLY)
  endif()
endforeach()
file(MAKE_DIRECTORY "${PC_FALLBACK_INCLUDE}/sys")
file(REMOVE "${PC_FALLBACK_INCLUDE}/sys/queue.h"
  "${PC_FALLBACK_INCLUDE}/sys/tree.h" "${PC_FALLBACK_INCLUDE}/sys/_null.h")
foreach(header queue tree)
  string(TOUPPER "${header}" container)
  if(NOT PC_HAVE_SYS_${container}_H)
    configure_file("compat/include/sys/${header}.h"
      "${PC_FALLBACK_INCLUDE}/sys/${header}.h" COPYONLY)
  endif()
endforeach()
if(NOT PC_HAVE_SYS_QUEUE_H OR NOT PC_HAVE_SYS_TREE_H)
  configure_file("compat/include/sys/_null.h"
    "${PC_FALLBACK_INCLUDE}/sys/_null.h" COPYONLY)
endif()
target_include_directories(pc_compat PUBLIC "${PC_FALLBACK_INCLUDE}")
if(WIN32)
  target_link_libraries(pc_compat PUBLIC shell32 advapi32 bcrypt)
endif()
# Native builds consume the distributed parser. Hosted regeneration is a
# separate reproducibility check and does not feed files into this build.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/date.c")
  message(FATAL_ERROR "generated/date.c is missing. Dispatch the native portability workflow (ci.yml) and retrieve generated-date-parser from its parser job; release/source archives include this file.")
endif()
set(PC_SOURCES cvs add admin annotate atomicio commit config checkout client buf cmd diff diff3 diff_internals edit entries fatal file getlog hash hash_func history log logmsg modules import init release remove repository rcs rcsnum rcsparse remote root server status tag trigger worklist util update version watch xmalloc)
list(TRANSFORM PC_SOURCES PREPEND "src/")
list(TRANSFORM PC_SOURCES APPEND ".c")
add_executable(opencvs ${PC_SOURCES} generated/date.c)
target_link_libraries(opencvs PRIVATE pc_compat)
if(WIN32)
  enable_language(RC)
  target_sources(opencvs PRIVATE platform/windows/opencvs.manifest.rc)
  target_include_directories(opencvs PRIVATE platform/windows)
  if(MSVC)
    # The resource file already embeds the application manifest.
    target_link_options(opencvs PRIVATE /STACK:8388608 /MANIFEST:NO)
  else()
    target_link_options(opencvs PRIVATE -Wl,--stack,8388608)
  endif()
endif()
install(TARGETS opencvs RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES LICENSE UPSTREAM README.md SECURITY.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY LICENSES/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/licenses)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES src/cvs.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 RENAME opencvs.1)
install(FILES src/cvs.5 DESTINATION ${CMAKE_INSTALL_MANDIR}/man5)
install(FILES src/cvsintro.7 DESTINATION ${CMAKE_INSTALL_MANDIR}/man7)

set(PC_CHM_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/html-help")
set(PC_CHM_FILE "${PC_CHM_OUTPUT_DIR}/opencvs.chm" CACHE FILEPATH
  "Verified compiled HTML Help file to install on Windows")
set(chm_tool_args)
foreach(tool PERL MANDOC COMPILER 7ZIP)
  if(PC_CHM_${tool})
    list(APPEND chm_tool_args "-DPC_CHM_${tool}=${PC_CHM_${tool}}")
  endif()
endforeach()
add_custom_target(windows-help
  COMMAND "${CMAKE_COMMAND}" "-DPC_CHM_OUTPUT_DIR=${PC_CHM_OUTPUT_DIR}"
    "-DPC_CHM_VERSION=${PROJECT_VERSION}" ${chm_tool_args}
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildHtmlHelp.cmake"
  BYPRODUCTS "${PC_CHM_OUTPUT_DIR}/opencvs.chm"
  COMMENT "Build and validate Portable OpenCVS HTML Help"
  VERBATIM)
if(PC_BUILD_TESTS)
  enable_testing()
  find_package(Perl REQUIRED)
  add_executable(test-ssh-shim tests/ssh_shim.c)
  target_link_libraries(test-ssh-shim PRIVATE pc_compat)
  add_executable(test-fs-probe tests/fs_probe.c)
  target_link_libraries(test-fs-probe PRIVATE pc_compat)
  if(WIN32)
    # Use the same long-path opt-in as the executable under test.
    target_sources(test-fs-probe PRIVATE platform/windows/opencvs.manifest.rc)
    target_include_directories(test-fs-probe PRIVATE platform/windows)
    if(MSVC)
      target_link_options(test-fs-probe PRIVATE /MANIFEST:NO)
    endif()
  endif()
  add_executable(test-unit-compat tests/unit_compat.c)
  target_link_libraries(test-unit-compat PRIVATE pc_compat)
  add_test(NAME unit-compat COMMAND test-unit-compat)
  if(PC_SANITIZERS)
    # The overflow case expects NULL/ENOMEM rather than ASan's allocator abort.
    set_tests_properties(unit-compat PROPERTIES
      ENVIRONMENT "ASAN_OPTIONS=allocator_may_return_null=1")
  endif()
  set_tests_properties(unit-compat PROPERTIES TIMEOUT 30)
  set(PC_TEST_REPORT_DIR "${CMAKE_CURRENT_BINARY_DIR}/test-reports")
  foreach(suite core security)
    add_test(NAME ${suite} COMMAND "${PERL_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration.pl" --opencvs $<TARGET_FILE:opencvs> --ssh-shim $<TARGET_FILE:test-ssh-shim> --fs-probe $<TARGET_FILE:test-fs-probe> --suite ${suite} --summary-report --report "${PC_TEST_REPORT_DIR}/${suite}.json")
    set_tests_properties(${suite} PROPERTIES TIMEOUT 240)
  endforeach()
  # A bare cvs executable may be OpenBSD's base implementation. Identify the
  # GNU peer before registering a suite whose assertions rely on it.
  find_program(PC_GNU_CVS NAMES cvs gcvs)
  set(PC_GNU_CVS_CONFIRMED FALSE)
  if(PC_GNU_CVS AND NOT WIN32)
    execute_process(COMMAND "${PC_GNU_CVS}" --version
      RESULT_VARIABLE PC_CVS_VERSION_RESULT OUTPUT_VARIABLE PC_CVS_VERSION
      ERROR_VARIABLE PC_CVS_VERSION_ERROR TIMEOUT 10)
    if(PC_CVS_VERSION_RESULT EQUAL 0 AND
       PC_CVS_VERSION MATCHES "Free Software Foundation")
      set(PC_GNU_CVS_CONFIRMED TRUE)
    endif()
  endif()
  if(PC_REQUIRE_GNU_CVS AND NOT PC_GNU_CVS_CONFIRMED)
    message(FATAL_ERROR "GNU CVS is required for interoperability; install GNU CVS and set PC_GNU_CVS to its executable (OpenBSD base cvs is not GNU CVS).")
  endif()
  if(PC_GNU_CVS_CONFIRMED)
    string(REGEX MATCH "^[^\r\n]+" PC_GNU_CVS_SUMMARY "${PC_CVS_VERSION}")
    message(STATUS "GNU CVS interoperability enabled: ${PC_GNU_CVS_SUMMARY}")
    add_test(NAME interoperability COMMAND "${PERL_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration.pl" --opencvs $<TARGET_FILE:opencvs> --gnu-cvs "${PC_GNU_CVS}" --ssh-shim $<TARGET_FILE:test-ssh-shim> --fs-probe $<TARGET_FILE:test-fs-probe> --suite interop --summary-report --report "${PC_TEST_REPORT_DIR}/interop.json")
    set_tests_properties(interoperability PROPERTIES TIMEOUT 240)
  elseif(WIN32)
    message(STATUS "GNU CVS interoperability omitted: no native Windows GNU CVS oracle is configured")
  else()
    message(STATUS "GNU CVS interoperability omitted: GNU CVS oracle unavailable")
  endif()
endif()
# The package verifier compares this metadata with the tested build. CI supplies
# the source revision and ABI, while CMake records the selected toolchain.
set(PC_BUILD_INFO "${CMAKE_CURRENT_BINARY_DIR}/build-info.txt")
file(WRITE "${PC_BUILD_INFO}"
  "Package version: ${PROJECT_VERSION}\n"
  "Source revision: $ENV{PC_SOURCE_REVISION}\n"
  "Target ABI: $ENV{PC_BUILD_ABI}\n"
  "System: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION} ${CMAKE_SYSTEM_PROCESSOR}\n"
  "Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}\n"
  "CMake: ${CMAKE_VERSION}\n"
  "Configuration: ${CMAKE_BUILD_TYPE}\n"
  "Sanitizers: ${PC_SANITIZERS}\n"
  "GNU CVS: ${PC_GNU_CVS_SUMMARY}\n")
install(FILES "${PC_BUILD_INFO}" DESTINATION ${CMAKE_INSTALL_DOCDIR})
set(CPACK_PACKAGE_NAME opencvs-portable)
set(CPACK_PACKAGE_VENDOR "Portable OpenCVS contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Development build of native portable OpenBSD OpenCVS")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_GENERATOR TGZ ZIP)
set(CPACK_SOURCE_GENERATOR TGZ)
set(CPACK_SOURCE_IGNORE_FILES "/[.]git/;/build[^/]*/;/dist[^/]*/;/artifacts/;/out/;~$;[.]pyc$")
include(CPack)
