# Testing and evidence

All product builds and runtime tests currently run in GitHub Actions only. Do not
execute a local opencvs build, local integration tests, or local parser generator
as part of this task. Static review and metadata/script syntax validation are
separate activities and must be labeled accurately.

CTest runs the native executable with the Perl black-box suite
`tests/integration.pl`. Its interfaces include `--opencvs`, `--suite core`,
`--suite security`, `--suite interop`, `--gnu-cvs`, `--ssh-shim`, `--fs-probe`,
`--timeout`, and `--report`. Tests create private disposable CVS repositories;
never point them at an existing repository. The native filesystem helper creates
Unicode/long paths when a Windows Perl API would otherwise confound product
behavior with interpreter limitations.

Core coverage targets init/import/checkout, add/remove/commit, update/diff,
log/status/annotate, tag/export, conflicting working copies, empty files, binary
bytes, CRLF/LF, nested paths, and error recovery. Security cases exercise protocol
lengths, unsafe names/traversal, malformed RCS input, exclusive locks and process
argument handling. A named test is not passed until its actual run succeeds.

GNU CVS interoperability runs only where the actual GNU executable is installed.
The local SSH-shaped shim starts real server processes and tests protocol bytes;
it does not authenticate or encrypt. Test both client/server directions when the
suite implements them and report unavailable peers as explicit skips. Do not
substitute GNU CVS implementation code or merely compare version strings.

The sanitizer matrix enables AddressSanitizer/UndefinedBehaviorSanitizer through
`PC_SANITIZERS`. Keep leak and UB diagnostics meaningful; investigate failures
rather than globally suppressing them. Future parser fuzzing should reuse small,
synthetic malformed RCS/protocol corpora and record time/resource limits.

Record commit SHA, runner image, architecture, compiler, libc, configuration,
passed/failed/skipped cases, and artifact links. Native Windows DLL inspection is
an additional gate. Runtime and sanitizer evidence must refer to the exact
artifact-producing revision; stale green badges do not certify a new commit.
