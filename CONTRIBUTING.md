# Contributing

Portable OpenCVS accepts narrowly scoped portability, interoperability, and
security improvements. Preserve C99 compatibility, OpenBSD style, original
licenses, and the CVS/RCS data model. Do not import GNU CVS implementation code.
GNU CVS is a test peer only.

Read [the architecture](docs/architecture.md) and [AGENTS.md](AGENTS.md), then
inspect the relevant source and its regression tests. Keep OS adaptations in
`compat/` and `platform/` where practical. Feature-detect missing interfaces;
do not replace working native APIs globally or supply success-returning stubs.

Product builds, parser generation, runtime tests, sanitizers, and interoperability
runs are currently authorized only on GitHub Actions. Submit changes to a branch,
then dispatch [the native portability workflow](.github/workflows/ci.yml) manually.
Do not turn on push, pull-request, or scheduled execution. Local static inspection,
Perl syntax checks, metadata validation, and workflow auditing are separate from
product compilation or execution and must not be reported as runtime evidence.

Report the exact commit, runner OS, architecture, compiler, libc, configuration,
test results, skipped cases, and artifact names. A Linux pass is not evidence for
Windows, musl, or ARM64. A compiler-only pass is not CVS interoperability evidence.
Do not remove failing tests, weaken diagnostics, or mark an unsupported operation
successful to obtain a green matrix.

Use small commits with the configured human identity. Sign them with the
configured existing OpenPGP key when available. Do not generate keys or add an
agent as author/coauthor. Imported source commits must identify their third-party
origin. Document behavior and maintain the affected agent/skill instructions when
a change alters their procedures; avoid cosmetic maintenance edits.

Security reports follow [SECURITY.md](SECURITY.md). Upstream changes follow
[the staging procedure](docs/upstream-sync.md), including a full license review.

New code uses the ISC license in [LICENSES/ISC.txt](LICENSES/ISC.txt). Copy its
complete copyright, permission, and warranty notice into every new code file
and every imported code file you modify, using that file format’s comment syntax.
For imported files, identify the additional notice as applying only to Portable
OpenCVS changes; keep all original authors’ notices. SPDX-only headers do not
satisfy this policy. Run `perl scripts/check-license-notices.pl` to audit the
complete notices and provenance inventory.
