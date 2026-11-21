# Portability roadmap

1. Establish clean strict builds with GCC, Clang, MSVC, ClangCL, and MinGW.
2. Verify native OpenBSD Debug and Release and Linux ARM64 in manual CI.
3. Verify repository preservation under I/O failure, binary handling, and GNU CVS interoperability.
4. Close remaining parser resource limits and filesystem race gaps before release qualification.

OpenBSD Debug and Release have completed configuration and built the
compatibility archive in CI. Executable compilation and functional
verification remain open on both configurations.
