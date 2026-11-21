# Audit progress

Both accessible historical runs, 44752958590 and 44754537542, stopped before
product tests. The second reached native Windows compilation. The inventories
under [docs/ci](docs/ci/) retain their diagnostics and revision identities.

Local changes cover feature detection, directory iteration, Win32 interfaces,
descriptor ownership, checked parsing, and staged file replacement. Regressions
and manual workflow changes are prepared. Compilation and behavioral
verification of this worktree await authorized publication. No product test,
sanitizer, or interoperability result is available for these changes.

The next two runs tested published revision
`fd0ed043ee5521637c2c9c738f583b8b4a17f2da`. OpenBSD Debug and Release configured
and built the compatibility archive. The native matrix completed the parser
and audit jobs. No product job reached tests. The follow-up entries in
[the CI inventories](docs/ci/COMPILATION-ERRORS.md) record the additional
diagnostics and distinguish them from the local fixes.

At b79f865, Linux, MinGW, and OpenBSD linked and reached CTest. Core and
security tests failed, as did GNU interoperability where enabled. MSVC had
compile failures and ClangCL hit a duplicate manifest at link. See the
b79f865 follow-up in [CI diagnostics](docs/ci/COMPILATION-ERRORS.md).

At 9d03e23, both OpenBSD unit jobs passed. Core and security failures
persisted. ClangCL reached CTest, MSVC still failed compilation, and the
native matrix was cancelled overall. Linux sanitizers found diff undefined
behavior and additional leaks. See the 9d03e23 entries in the CI inventories.

The subsequent read-only GitHub inventory found no run newer than 9d03e23.
Local follow-up expands regression coverage for separated diff hunks,
rejected conflict commits, and replacement of read-only Windows files.
These cases have not executed in CI.
