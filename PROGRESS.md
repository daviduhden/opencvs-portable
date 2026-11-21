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

The audit at 9d03e23 prompted additional regressions for separated diff hunks,
rejected conflict commits and replacement of read-only Windows files.

At ba0ad43 all Windows configurations reached CTest. Linux without
sanitizers and OpenBSD passed unit and security suites. Core and GNU CVS
interoperability remain open. Windows replacement and residual Entries
leaks have actionable follow-up in the CI inventories.

At 4646494, OpenBSD Debug and Release passed all three suites. Linux core
progressed, while GNU wire interoperability, Windows replacement, and
sanitizer leaks still require verification. The native run remains failed.

At `7ed85aa268b3507a19204ddda9b53e58ff91a3c7`, the Linux native jobs
reached CTest and failed GNU CVS interoperability. In GCC job `136917218913`,
a fresh GNU CVS checkout returned different bytes after a commit from the
OpenCVS client reported success. The reverse client/server direction passed.
[Native verification 44927597681](https://github.com/daviduhden/opencvs-portable/actions/runs/44927597681)

At `8801c7b29a208f45bed53d9d581ae8d99576e57e`, all 13 native jobs passed.
Four suites passed per Linux target, including GNU CVS interoperability
and the ASan/UBSan build. Both OpenBSD configurations and all Windows jobs
passed their three configured suites. The Windows PE import checks passed.
Agent Knowledge Validation passed 49 Perl contract tests and the range
checks. The [compatibility matrix](docs/compatibility.md) records configurations
and coverage limits. Subsequent parser, packaging and product changes require
new hosted verification.
[Native verification 44928779089](https://github.com/daviduhden/opencvs-portable/actions/runs/44928779089)
[OpenBSD verification 44928781955](https://github.com/daviduhden/opencvs-portable/actions/runs/44928781955)
[Agent Knowledge Validation 44928784505](https://github.com/daviduhden/opencvs-portable/actions/runs/44928784505)
