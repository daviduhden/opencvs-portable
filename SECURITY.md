# Security policy

This project is under development. No release is designated production-stable
merely because it compiles. The [initial audit](docs/initial-audit.md) records
inherited risks; [the security model](docs/security.md) describes remediation
requirements and residual limits.

Report a suspected vulnerability through GitHub's private vulnerability-reporting
feature when available. If that feature is unavailable, request a private contact
channel without posting exploit payloads, credentials, or private repository data
in a public issue. Do not assume a private reporting channel has been enabled
without checking the repository settings.

Useful reports include the exact revision, OS/compiler, access method, a minimal
synthetic repository or protocol transcript, expected/observed behavior, and
sanitizer diagnostics. Reproductions must use disposable directories. Do not run
destructive experiments on a real CVS repository or another user's system.

Treat remote CVS messages, RCS files, working-copy metadata, filenames, symlinks,
reparse points, environment variables, editors, and repository hooks as trust
boundaries. SSH authentication does not make those inputs safe. This port does
not add pserver authentication or promise that every CVS extension is supported.

Stable release claims require successful native Linux and Windows tests,
interoperability, and review of relevant high-severity findings. Runtime safety
must not depend on fake sandbox functions or disabled diagnostics.
