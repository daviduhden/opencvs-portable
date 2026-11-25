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

On Windows, `--fs-probe` selects the native C filesystem helper for UTF-8 and
long-path reads, writes, and cleanup; this avoids treating Perl's ANSI filesystem
interfaces as product behavior. Its guarded cleanup operation is restricted to
private generated test sandboxes and must never target a real working copy.
# Hosted transport and performance checks

The manual native workflow can use a dedicated loopback SSH daemon with ephemeral
keys to exercise checkout and commit. The test compares the resulting bytes in a
fresh checkout. Ordinary CTest runs retain the process transport fixture for
deterministic protocol and exit-status coverage.

The `benchmark` suite in `tests/integration.pl` is intended for hosted runners.
It reports elapsed times for fixed history depths and tree sizes. Linux can also
record peak RSS and file syscalls through `time` and `strace`. Compare samples
within the same runner and compiler configuration. These observations establish a
baseline for any index or path optimization and have no fixed timing threshold.
The Linux transport benchmark uses the native process shim and checks binary
checkout bytes. It records elapsed time and RSS, with separate read/write
syscall counts when strace is available. Compare those results before changing
the existing protocol buffering or framing.

CTest writes bounded JSON summaries for the Perl integration suites under
`test-reports/`. They identify passed, failed and skipped cases. Native CI
retains these summaries with the runner and compiler metadata.

## Windows manual acceptance

The manual CI and release workflows validate the generated topics, test
the CHM container, and compare the packaged bytes with the validated input.
For desktop acceptance, open the packaged `opencvs.chm` in Windows HTML Help
on Windows 10 and Windows 11 with network access unavailable. Check that
the start page shows the package version and that Contents opens
`cvs(1)`, `cvs(5)` and `cvsintro(7)` under sections 1, 5 and 7. Follow
a cross-reference from the introduction to the command manual, use Index
to open a heading, and search for `CVSROOT` across topics. Check the command
examples, lists and tables in the topic pane. Record the Windows build,
package SHA and outcome for each desktop test.
