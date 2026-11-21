# Portable OpenCVS agent instructions

Portable OpenCVS derives from OpenBSD `usr.bin/cvs` at the revision in
`UPSTREAM`. Original authors retain credit; this is an independent project.
The existing import commit and `provenance/manifest.json` are the baseline.

## Architecture and ownership

- `src/`: CVS/RCS implementation. Preserve `$OpenBSD$` markers and notices.
- `compat/include/portable.h`: forced inclusion, feature-selected libc
  declarations, namespaced BSD getopt, and portable initialization.
- `compat/common.c`: per-stream fgetln lifetime, formatting, argv parsing.
- `compat/linux/process.c`: POSIX spawn/pipes/wait; no implicit shell.
- `platform/windows/process.c` and `compat/windows/`: native Win32 UTF-8/Wide
  adapters and explicit process handle inheritance. No POSIX runtime.
- `compat/imported/`: narrowly imported BSD helpers with separate provenance.
- `generated/date.c`: intentionally distributed, public-domain yacc output.
- `tests/`: native C helpers and Perl black-box tests, isolated temporary data.
- `scripts/`: non-destructive upstream staging, knowledge/import validators.

Consult the specialized agents under `.github/agents/` and matching skills
under `.agents/skills/` for upstream, Linux, Windows, architecture, security,
tests, CI/CD, and documentation. The execution environment has limited agent
slots: run roles as capacity permits, with disjoint file ownership. Root
integration owns shared interfaces and source changes; workers report before
changing another worker's files. Centralize commits and publishing.

## Session execution constraint

The user requires all compilation and product test execution in manual GitHub
Actions. Do not configure CMake locally: feature checks can compile probes.
Do not run native or cross compilers, parser generators, or product binaries
locally. Inspect and edit source, then use `gh workflow run ci.yml --ref main`,
`gh run view`, and `gh run download` for actual build/test evidence. Complex
test scripts must be Perl, not Python. Simple shell is acceptable.

## C, compatibility, and security

Use C99 and OpenBSD style; MSVC may use its supported C11 mode. Preserve
upstream algorithms unless an audited defect or platform contract requires a
change. Detect missing libc features; do not duplicate available APIs. Keep
Windows implementation details in its backend, not scattered source ifdefs.
Every new code file and every modified imported code file must contain the
complete notice from `LICENSES/ISC.txt`, including the exact 2026 David Uhden
Collado copyright and email. A short SPDX identifier alone is insufficient.
For imported files, scope this additional notice to Portable OpenCVS changes
and preserve every original notice. Do not relabel unmodified imported or
generated upstream code. Run `perl scripts/check-license-notices.pl` after edits.
Never stub security or process functions to return success. No fake fork,
pledge/unveil, sandbox, Unicode conversion, filesystem lock, or permission
semantics. The `exec` and `error` pledge promises are forbidden. Document
actual restrictions; reject unsupported data rather than corrupt it.

Treat RCS files and protocol input as hostile. Check lengths, integer
arithmetic, path components, link traversal, binary I/O and atomic replacement.
Spawn explicit argv without an implicit shell; append generated arguments as
separate elements. Never place user filenames into shell strings. Preserve
licenses and Caldera's advertising acknowledgment in documentation/packages.

## Validation, maintenance, and delivery

The CI commands are CMake configure/build, CTest, dependency audit, and CPack;
see `docs/building.md` and `docs/testing.md`. Report actual runner, compiler,
job, SHA, failures and skipped cases. A successful compile is not a functional
or interoperability pass. Do not call a platform supported before its native
job and relevant tests pass. Never publish stable packages with critical
failures. All workflows are owner-gated `workflow_dispatch`, least privilege,
with actions pinned to full commit IDs. Audit them with zizmor.

Before completion, review affected agents/skills, update only changed
contracts, validate their links and metadata in CI, and report **Agent & Skills
Synchronization** with actual checks and PASS/FAIL. Avoid cosmetic instruction
edits and maintenance loops. Use the user-configured Git identity and available
GPG key; never add agent authorship or coauthor trailers. Make focused commits.

For updates, use `scripts/upstream-stage.pl` to obtain an isolated candidate;
review the three-way differences against baseline and local changes. Never
automatically overwrite source. Update both provenance manifests, licenses,
parser output, relevant documentation and test evidence after a reviewed sync.

## Diagnostic and regression policy

For every compiler warning, build failure, crash, portability defect, test
failure, sanitizer report, or data integrity problem, record the originating
run and SHA (or mark the finding as static). Trace the root cause through
declarations, implementations, callers, and similar patterns across OpenBSD,
Linux, and Windows. Review the relevant inventories under `docs/ci/` before
changing a subsystem. Add a focused regression for important fixes and keep
upstream notices and provenance intact. A historical run provides evidence
only for its own SHA. Use manual CI for behavioral validation. Publication
requires permission when the task prohibits commits or push.

For a follow-up audit, compare the new SHA with the prior failing revision.
Record successful intermediate targets and regressions separately. Trace
warnings in generated parser sources back to the grammar or generator before
changing build flags.

When suites first reach execution, trace shared setup and cleanup failures
before attributing each failed assertion to a separate product defect. Record
sanitizer findings independently, including findings from successful paths.

Treat cancelled runs as incomplete. Record the last completed stage per job
and retain diagnostics already emitted before cancellation.

Record successful stages before failures. Keep dependency inspection separate
from test outcomes when a native executable was successfully built.

For resource lifetime failures, audit ownership across cwd changes and
cleanup. Keep temporary server state distinct from persistent working-copy
metadata. Use native failure stages to distinguish platform semantics from
test fixture assumptions.
