# Linux implementation and validation

Linux uses native POSIX filesystem, identity, descriptor, signal, and process
interfaces. CMake detects missing BSD libc functions and supplies narrow
compatibility implementations. The namespaced BSD getopt parser preserves
OpenBSD `optreset` behavior across glibc and musl; it must not accidentally bind
its private globals to GNU getopt.

The POSIX process backend uses `pipe2(O_CLOEXEC)` when the toolchain exposes it
and the kernel implements it. Older targets use `pipe` followed by
`fcntl(FD_CLOEXEC)`. The fallback is intended for the current single-threaded
engine. Other descriptors retain their normal POSIX inheritance behavior. Open
project-owned descriptors with close-on-exec semantics when they must stay
private across SSH, hook, or editor launches.

The manual CI matrix targets Ubuntu 24.04 with GCC and Clang, Clang sanitizers,
and Alpine 3.23 with musl inside a container on the hosted Ubuntu runner. The
container is test infrastructure, not a product runtime requirement. No local
container build or local compiler invocation is authorized in this task.

Support status is established by actual run results, not this planned matrix.
Debian, Fedora, ARM64, and other combinations need their own execution evidence.
Do not infer musl support from a glibc build or GCC support from a Clang build.

The executable uses `opencvs`; GNU CVS remains `cvs` when installed as the test
peer. SSH remote access needs an explicitly configured external OpenSSH client.
The engine has no direct socket or crypto-library dependency at this revision.

Linux packages built on Ubuntu use that runner's glibc ABI. Alpine packages use
musl and require the appropriate loader/runtime unless a separately verified
static distribution is produced. Inspect `ldd` artifacts and preserve the exact
compiler/libc record. TGZ infrastructure does not establish that DEB/RPM packages
have been generated or tested.

No seccomp sandbox is claimed. A future filter needs syscall tracing and explicit
coverage for local operations, SSH subprocesses, hooks, editors, and error cleanup.
Memory/path safety and robust error handling cannot depend on seccomp.
