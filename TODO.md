# Audit follow-up

- Validate the local changes in manual CI after authorized publication.
- Regenerate the date parser in CI and review the resulting artifact.
- Resolve remaining compiler diagnostics, especially Windows fallback regex conversions.
- Verify filesystem replacement and GNU CVS interoperability on the target matrix.
- Extend sanitizer coverage with bounded parser fuzzing and record any failures.

- Recheck the fd0ed04 blockers in the next published run, including OpenBSD
  headers, date generator sentinels, Windows regex and UCRT warnings, and
  the sticky-date preservation regression.
