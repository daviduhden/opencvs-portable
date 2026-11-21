# Audit follow-up

- Validate the local changes in manual CI after authorized publication.
- Regenerate the date parser in CI and review the resulting artifact.
- Resolve remaining compiler diagnostics, especially Windows fallback regex conversions.
- Verify filesystem replacement and GNU CVS interoperability on the target matrix.
- Extend sanitizer coverage with bounded parser fuzzing and record any failures.

- Recheck the fd0ed04 blockers in the next published run, including OpenBSD
  headers, date generator sentinels, Windows regex and UCRT warnings, and
  the sticky-date preservation regression.

- Recheck repeated init, import cleanup, RCS commit-ID ownership, and the
  Windows drive-path and linker fixes on an authorized published revision.
- Investigate GNU server response path semantics while preserving containment.
- Reassess remaining individual cases after shared setup succeeds.

- Verify Windows archive replacement and short directory names.
- Recheck sanitizer diagnostics after diff changes, then trace residual leaks.
- Verify GNU CVS interoperability as a non-root user and isolate wire failures.

- Validate date arithmetic on both generated parsers and the Windows and
  POSIX time interfaces. Include normal relative dates and out-of-range input.
- Verify the Windows read-only replacement contract with native handles.

- Recheck export -d, unresolved conflicts, and Entries ownership in CI.
- Identify the exact Windows archive replacement failure stage.
- Verify both GNU CVS wire directions with the corrected transport fixture.
- Diagnose the Windows Debug server timeouts with native process evidence.
  Keep the current time limits and separate this from the import failures.
