---
name: cvs-protocol
description: Develop and validate Portable OpenCVS command, RCS, working-copy, and client-server interoperability behavior.
---

# Cvs Protocol

Read [compatibility boundaries](../../../docs/compatibility.md), [test evidence rules](../../../docs/testing.md), and the actual request/response tables in `src/remote.c`, `src/client.c`, and `src/server.c`. Preserve byte counts, binary mode, Entries/Root/Repository data and RCS revisions. Never infer support from a method name or dispatch entry.

Exercise `tests/integration.pl` through hosted CTest using `--suite core`, `security`, or `interop`, an explicit GNU CVS peer, SSH-shaped transport shim and native filesystem helper. Keep repositories disposable; compare bytes and revision/tag behavior in both peer directions. The shim does not establish SSH authentication or encryption. Retain explicit errors for unsupported pserver/kserver/gserver/fork, compression and disabled watch/edit commands. Do not use GNU CVS implementation code to replace OpenCVS or drop a failing interoperability case.
