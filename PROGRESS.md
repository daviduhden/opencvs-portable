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
