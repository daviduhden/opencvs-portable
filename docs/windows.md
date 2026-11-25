# Native Windows implementation

Portable OpenCVS uses Win32 APIs and the Microsoft C runtime. It does not
implement `fork` and does not require WSL, Cygwin, or the MSYS POSIX runtime.
Build and execution results belong to the manual GitHub Actions matrix; a
backend implementation alone is not evidence that a compiler or platform
passed. Local compilation is prohibited for this development session.

## Process and stream boundary

`platform/windows/process.c` implements the shared `pc_process.h` interface.
`CreateProcessW` receives a resolved application path and a mutable UTF-16
command line. Every argument is individually quoted using the Microsoft CRT
backslash/quote rules. No `cmd.exe`, `sh`, or `ShellExecute` is invoked.
`STARTUPINFOEXW` restricts inherited handles to explicitly duplicated child
stdin, stdout, and stderr handles; other handles are noninheritable.
Anonymous pipes are binary CRT descriptors, and process handles are closed by
`pc_wait` or `pc_detach`. Native exception exit statuses map to failure 255.
This is process plumbing, not a security sandbox.

The actual remote transport inherited from OpenCVS is an SSH subprocess.
Install native Windows OpenSSH, and select an executable with `CVS_RSH` if
needed. OpenSSH performs DNS resolution, IPv4/IPv6 networking, encryption and
authentication. This project does not pretend that Windows sockets are CRT
file descriptors. The upstream pserver, kserver and gserver methods are not
implemented and must not be advertised as working transports.

## UTF-8, paths, and files

`pc_windows_startup` obtains arguments with `CommandLineToArgvW` and converts
UTF-16 to UTF-8 without using an ANSI code page. Environment access uses wide
Windows APIs and cached UTF-8 values; modifications update the CRT wide and
native process environments. Standard streams and all file streams are binary,
so CRLF, NUL bytes and protocol byte counts are preserved.

Filesystem operations use strict UTF-8 conversion, absolute extended-length
Win32 paths, `CreateFileW`, `FindFirstFileW`, `SetFileInformationByHandle`,
and related APIs.
The executable manifest requests `asInvoker`, long-path awareness and UTF-8
behavior; it does not request elevation. The application's inherited fixed
buffers impose a maximum of 4095 UTF-8 bytes per path. This supports paths
longer than 260 characters within that bound; it is not a claim to support
arbitrary NTFS path depth or the entire 32767-WCHAR namespace.

Device namespaces (`\\?\` and `\\.\`) supplied as input, DOS reserved names,
alternate-stream colons, forbidden characters, and trailing spaces/dots in
components fail explicitly. Drive-relative paths such as `C:module` fail;
use an absolute drive path such as `C:/repo`. Normal drive and UNC paths are
converted using native Unicode APIs. The handle check rejects case-only
final-component aliases and 8.3 file aliases. It accepts a directory's 8.3
spelling when the filesystem confirms it as that directory's short name.
Use the actual on-disk spelling of repository paths. Case-sensitive
directories and SMB/DFS shares still require dedicated verification.

Reparse-point traversal and file opens are rejected. Metadata is checked on
opened handles, including the final canonical path, before truncating a file.
These checks do not constitute a race-free filesystem sandbox against an
attacker who can concurrently replace ancestor directories. Keep repositories
and temporary directories owned by trusted accounts; do not expose a Windows
server with writable untrusted ancestors. Rename holds a handle to the source
and validates the destination directory by handle. The Win32 rename call still
resolves the destination by absolute path. Other operations such as named
deletion and directory creation also resolve a path after validation. A stronger
boundary would require handle-relative operations and a separate audit across the
supported filesystem and Windows versions.

Private creation modes 0600 (files) and 0700 (directories) use a protected
owner/SYSTEM DACL rather than assuming the POSIX mode exists. Other group and
owner mode bits do not provide POSIX ACL semantics on Windows. Read-only state
maps to the Windows read-only attribute. Replacement uses `FileRenameInfoEx`
with the read-only override flag and requires OS and filesystem support.
If that interface is unavailable, a non-replacing rename is possible when
the destination was absent. Deletion temporarily adjusts the read-only
attribute through an opened metadata handle and attempts to restore it after
failure.
Requests to set Unix executable bits on regular files return `ENOTSUP`:
checkout of Unix executable files is currently unsupported, rather than
silently losing mode information. No portable executable-bit metadata format
has been invented, and Windows DACLs must be administered separately.

Secure temporary files use `BCryptGenRandom`, exclusive `CREATE_NEW`, binary
handles and private DACLs. They never use predictable `rand()` filenames.
File sizes and offsets are 64-bit. File timestamps preserve the 100-nanosecond
resolution available from NTFS within the representable platform range.
The advisory numeric account ID is a SID-derived value and must never be used
as authorization; only the current account can be resolved through the narrow
`getpwuid` compatibility interface.

## Dates and resource controls

UTC conversion uses Gregorian calendar arithmetic, including pre-1970 values.
Local time uses the CRT and native dynamic time-zone APIs. The `strptime`
fallback is intentionally limited to the one ctime-style format used by
`CVS/Entries`, `%a %b %d %T %Y`; unsupported formats fail. Ctime/asctime output
uses English CVS-compatible formatting and rejects out-of-range years rather
than overflowing its 26-byte result.

The inherited `dlimit` interface maps to a Windows Job Object process committed
memory limit, not an exact Unix data-segment limit. Assignment or enforcement
setup errors are returned; no empty `pledge` or `unveil` substitute is used.
Readiness polling is implemented for ordinary files and read ends of pipes.
Unsupported readiness modes fail explicitly; normal application pipes remain
blocking.

## Validation and distribution

Use the MSVC, Clang/LLVM and MinGW-w64 jobs described in
[building.md](building.md) and [testing.md](testing.md). Inspect the actual job
results and executable import tables. Successful compilation does not establish
CVS interoperability, Unicode correctness, security isolation, or absence of
a POSIX runtime dependency. The native ZIP must include the executable,
licenses, version information and the actual compatibility matrix.

No stable Windows release should be published while critical build, data
integrity, or interoperability tests fail. This document describes implemented
interfaces and explicit limitations; verified results must be recorded
separately from pending tests.
