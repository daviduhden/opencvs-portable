# Portability roadmap

1. Establish clean strict builds with GCC, Clang, MSVC, ClangCL, and MinGW.
2. Verify native OpenBSD Debug and Release and Linux ARM64 in manual CI.
3. Verify repository preservation under I/O failure, binary handling, and GNU CVS interoperability.
4. Close remaining parser resource limits and filesystem race gaps before release qualification.

OpenBSD Debug and Release have completed configuration and built the
compatibility archive in CI. Executable compilation and functional
verification remain open on both configurations.

The b79f865 runs reached CTest on Linux, OpenBSD, and MinGW. Prioritize
repository setup and cleanup before interpreting downstream CVS results.

Both OpenBSD unit configurations passed at 9d03e23. The next gate is a clean
functional run with conflict handling, diff status, and archive interchange.

The ba0ad43 matrix compiled through CTest on all current targets. Complete
functional and interoperability verification before expanding support claims.

OpenBSD's two configurations passed at 4646494. Keep that regression
coverage while pursuing a clean native matrix and GNU interoperability.
