---
name: cvs-protocol
description: Develop and validate Portable OpenCVS command, RCS, working-copy, and client-server interoperability behavior.
---

# Cvs Protocol

Read [compatibility boundaries](../../../docs/compatibility.md), [test evidence rules](../../../docs/testing.md), and the actual request/response tables in `src/remote.c`, `src/client.c`, and `src/server.c`. Preserve byte counts, binary mode, Entries/Root/Repository data and RCS revisions. Never infer support from a method name or dispatch entry.

Exercise `tests/integration.pl` through hosted CTest using `--suite core`, `security`, or `interop`, an explicit GNU CVS peer, SSH-shaped transport shim and native filesystem helper. Keep repositories disposable; compare bytes and revision/tag behavior in both peer directions. The shim does not establish SSH authentication or encryption. Retain explicit errors for unsupported pserver/kserver/gserver/fork, compression and disabled watch/edit commands. Do not use GNU CVS implementation code to replace OpenCVS or drop a failing interoperability case.

## Maintenance & Continuous Learning

Update this skill when changes invalidate guidance within its scope.

For an ordinary modified file upload, the Entry timestamp field is empty.
The following Modified request carries the state and bytes. A checkout
timestamp on the wire can match the peer's temporary file timestamp and hide
a real modification. Preserve conflict markers independently.

Treat a terminal protocol error and a later transport failure as distinct
observations. An acknowledged command may have published changes before an SSH
exit failure. Check the repository before retrying a write. Use the loopback SSH
job to verify real authentication and transport, and keep the process fixture
for repeatable framing and failure tests.
