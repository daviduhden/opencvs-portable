# Initial upstream portability and security audit

This records the **pristine upstream baseline**, before portable source edits.
Findings are not claims that a later project revision still contains a defect;
consult the security documentation and regression tests for remediation status.

## Scope and provenance

- Upstream: [OpenBSD src](https://github.com/openbsd/src).
- Exact revision: `4453305eb9b84bbd8b912a2794d5b2b12caa1b0f`.
- Source directory: `usr.bin/cvs/`.
- Import: 68 files, including 45 C translation units, 17 headers, one Yacc
  grammar, the original Makefile, README, and three manual pages.
- Static review: all 63 C/header/grammar files, 24,912 lines.
- Baseline Makefile: `src/Makefile`; original warning and source lists retained.

The baseline README explicitly describes OpenCVS as still in development and
warns against treating it as a complete GNU CVS replacement. The portable
project must establish its own command-level evidence rather than assume that
building the imported source establishes compatibility or data integrity.

## Actual dependencies

The Makefile links no external crypto, compression, networking, or utility
library. In particular, this source revision has **no direct socket, DNS,
OpenSSL, LibreSSL, MD5, SHA, or zlib calls**. Its remote transport is an external
SSH-compatible program connected through pipes; server mode implements the CVS
protocol on standard input and standard output. Winsock is therefore not needed
merely to reproduce this revision's SSH transport. A future direct TCP transport
would need a separate Winsock/socket abstraction and its own tests.

| Area | Actual baseline use | Portable response |
| --- | --- | --- |
| BSD strings and allocation | `strlcpy`, `strlcat`, `strsep`, `strtonum`, `reallocarray` | Feature-detect native functions and build isolated fallbacks only where absent. Check additions before allocation as well as multiplication. |
| Formatted allocation | `vasprintf` through logging and allocation helpers | Supply a checked, `va_copy`-correct implementation where missing; native symbols must not be silently interposed. |
| Line input | `fgetln` in configuration, logs, diff3, and protocol input | Preserve length-delimited, non-NUL-terminated semantics and stream-specific lifetime. Do not copy libc's implementation, which uses private `FILE` internals. |
| Time | `strptime`, `timegm`, `tzset`, `futimes`, `futimens`, POSIX `stat` timestamp members | Feature checks and explicit UTC/time conversion; do not round-trip through local time or silently discard filesystem errors. |
| Program name | External `__progname` | Initialize a portable program-name interface. `getprogname`/`setprogname` are not directly called upstream. |
| Options | `getopt`, BSD `optreset` | Use the namespaced BSD parser consistently: GNU `optind` reset rules differ. |
| BSD containers | `sys/queue.h`, `sys/tree.h` | Preserve original headers/licenses, supply only missing compiler metadata macros. Do not replace containers with unrelated implementations. |
| Pattern matching | `fnmatch`, POSIX `regcomp`/`regexec`/`regfree` | Native POSIX facilities on Linux; audited BSD fallback on Windows, with byte/locale behavior documented. |
| Processes | `fork`, `pipe`, `dup2`, `execv`, `execvp`, `waitpid`, signals | One argv-based process abstraction; Win32 `CreateProcessW` and explicit handle inheritance. No `fork` imitation. |
| Users and permissions | `getpwuid`, `getpwnam`, uid values, umask, chmod/fchmod | Explicit native identity and permission policy, rather than fabricated POSIX identities or ignored failures. |
| Filesystem | open/stat/lstat, DIR/readdir/d_type, rename/unlink, temporary files, offsets, descriptor timestamps | Checked platform layer; UTF-8/UTF-16 and binary I/O on Windows. |
| Resource limits | `getrlimit`/`setrlimit(RLIMIT_DATA)` in the `dlimit` config option | A real native implementation or a clear unsupported error; never silently claim a limit. |
| Memory mapping | mmap/munmap/madvise in compare/copy | Can be replaced with bounded streaming operations preserving content and error behavior. |

The requested `recallocarray`, `freezero`, `explicit_bzero`, `getprogname`,
`setprogname`, `getline`, `getdelim`, `asprintf`, `socketpair`, random-number and
crypto functions are **not called** by this baseline. `getline` may still be a
reasonable implementation primitive for a portable line reader; that does not
make it an upstream dependency. `strmode` is not called either.

## Ranked findings

Line numbers identify the pristine import and may move after fixes. Static
findings are review evidence, not an assertion that an exploit has been
executed. Regressions must validate the actual remediation.

| ID | Severity | Platforms | Difficulty | Baseline location | Finding and required treatment |
| --- | --- | --- | --- | --- | --- |
| SEC-001 | Critical | All | Medium | `src/server.c:333-352` | `Directory` checks only string length before slicing at the repository-root length. Establish prefix and separator boundaries, reject traversal, and validate the resulting path before creating directories. |
| SEC-002 | High | All | Low | `src/server.c:419-425` | `open(..., O_CREAT)` omits its mandatory mode argument. Set an explicit restrictive mode, validate the file leaf, and prevent symlink/reparse-point traversal. |
| SEC-003 | High | All | Medium | `src/repository.c:61-90` | An existing lock owned by the same uid is treated as this process's lock. Creation uses `O_TRUNC` without `O_EXCL`, despite an `EEXIST` branch. Use exclusive acquisition and process-owned tracking; test two concurrent writers and symlink targets. |
| SEC-004 | High | All | Medium | `src/logmsg.c:280-301` | The editor command appends an unquoted temporary pathname to `sh -c`. Parse permitted configuration into argv and spawn directly; paths containing spaces or metacharacters must remain data. |
| SEC-005 | High | All | Medium | `src/util.c:884-927` | Hooks run shell command strings, and the wait loop retries permanent errors such as `ECHILD`. Define an explicit trusted-admin hook policy, avoid accidental shell interpretation, retry only `EINTR`, and close unintended descriptors. |
| PORT-001 | High | Linux/Windows | Low | `src/file.c:1043-1049,1101` | Mapping a zero-length regular file fails on Linux and cannot be assumed valid on Windows. Handle empty files explicitly or use streaming comparison/copy. |
| PORT-002 | High | Windows | High | `src/root.c:95-108` | CVSROOT assumes slash-rooted paths and colon-separated host fields. Distinguish drive paths, UNC paths and remote host syntax; preserve bracketed IPv6 literals rather than splitting their colons. |
| PORT-003 | High | Windows | High | `src/client.c:207-276` | SSH launch directly depends on fork/pipe/dup2/exec and POSIX stdio defaults. Use wide Win32 process creation, bounded explicit handle inheritance, and binary protocol pipes. |
| SEC-006 | Medium | All | Medium | `src/rcsparse.c:1197-1207`, `src/buf.c:288-292` | Size additions precede checked `reallocarray`; multiplication checking cannot catch an addition that has already wrapped. Check both operations and exercise malformed/unbounded parser inputs under sanitizers. |
| SEC-007 | Medium | All | Low | `src/server.c:112-117` | The server temporary directory is PID-derived. Use exclusive randomized creation and ownership-scoped cleanup. |
| SEC-008 | Medium | Windows | High | `src/remote.c:298-315` | Validation rejects only literal `..` components separated by `/`. Account for backslashes, drive/UNC syntax, ADS, reserved names, trailing dots/spaces and reparse points through a centralized native path policy. |
| FUNC-001 | Medium | All | Medium | `src/cvs.c:49,408-410` | `-z` is parsed, but the compression level has no consumer and no compression implementation exists. Reject unsupported nonzero compression instead of implying a compressed connection. |
| FUNC-002 | Medium | All | High | `src/client.c:207-216` | `pserver`, `kserver`, `gserver`, and `fork` methods are explicitly rejected. Preserve explicit errors and prioritize SSH; do not claim these upstream methods work. |
| FUNC-003 | Low | All | High | `src/cmd.c:56-62` | `edit`, `editors`, `unedit`, `watch`, and `watchers` are disabled in the dispatch table. Retain their source without advertising operational support. |
| PORT-004 | Medium | OpenBSD | Low | `src/cvs.c:192-193` | The imported pledge includes `exec`, which the project policy excludes. Do not provide a success-returning pledge stub on other systems. Treat optional native restrictions honestly and separately from memory/path safety. |

`fgetln` callers index `len - 1`; a compatibility reader must preserve its
contract that a successful returned line is nonempty. EOF is not a successful
zero-length line. The original reader contract, rather than a guessed substitute,
controls these callers.

## Commands and protocol coverage

The dispatch table contains `init`, `import`, `checkout`, `add`, `remove`,
`commit`, `update`, `diff`, `log`, `status`, `tag`, `annotate`, `export`, `admin`,
`history`, remote variants, `release`, `version`, and `server`. Dispatch presence
is not a functional test. Each advertised command needs a temporary repository
integration test, including binary files, empty files, error recovery and
repository compatibility with GNU CVS.

The request and response tables deliberately leave several protocol extensions
without handlers, including compression-related or optional response forms.
The client advertises only selected supported responses. Interoperability tests
must exercise negotiation and explicit failure, not assume that every request
used by GNU CVS is available. The server has no authentication listener: SSH
provides transport authentication; repository `Root` selection and filesystem
permissions still need an explicit server deployment policy.

## Generator and build audit

`src/date.y` includes its own lexer. No Lex/Flex generator is required. Its
comment documents ten shift/reduce conflicts. Preserve the grammar and record
the chosen Yacc/Bison version and regeneration command. Distribute the generated
C parser so ordinary builds do not require a generator; preserve any generator
skeleton license and exception notices. A build using Bison does not import GNU
CVS implementation code.

`bsd.prog.mk` is an OpenBSD build dependency, not a suitable portable project
entrypoint. CMake must list actual translation units and discover feature
availability, without defining GNU or OpenBSD operating-system identity merely
to expose declarations. Strict warnings should be compiler-specific and must not
hide real incompatible prototypes, truncation, integer-size assumptions or
format-string defects.

## Licensing audit

Original notices are not replaced by a blanket project copyright. Many files
carry ISC or BSD-style conditions. The date grammar explicitly states that it
is public domain. `src/diff.h`, `src/diff_internals.c`, and `src/diff3.c` also carry
a Caldera International license with a separate advertising acknowledgment:

> This product includes software developed or owned by Caldera International, Inc.

Preserve the complete per-file Caldera, University of California, OpenBSD author,
and other notices. Include required notices in binary distributions and retain
the acknowledgment in project-facing documentation. An aggregate LICENSE must
explain the collection rather than incorrectly label every imported file ISC or
BSD-2-Clause. Auxiliary BSD libc, regex, and container imports require their own
provenance and notices as recorded in `upstream/compat-manifest.json`. The regex
COPYRIGHT also requires Henry Spencer origin/alteration credits in documentation;
its initial plain-text license block must not be lost by a C-comment-only notice
collector.

## Historical portable project

The [historical Portable OpenCVS page](https://chl.be/opencvs/) was retrieved
and its HTML inspected. It reports old Linux/Alpine, BSD and macOS port results,
but does not claim a Windows port. The current response ends at an empty download
table and exposes no source archive link. Therefore no old implementation was
copied or represented as audited. This provides historical context only;
modern glibc/musl and native Windows support must be established by this project.

## Initial verification boundary

This audit was a full-source static review. It did not itself execute an exploit,
prove absence of vulnerabilities, build every target, test Windows, run
sanitizers, or establish GNU CVS interoperability. Later build/test evidence
must state precisely which compiler, libc, operating system and command path
was actually exercised.
