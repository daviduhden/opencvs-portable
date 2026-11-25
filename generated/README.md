# Distributed date parser

`date.c` is the Berkeley byacc output from `src/date.y`, retrieved from
job `136920607513` of [verification run 44928779089](https://github.com/daviduhden/opencvs-portable/actions/runs/44928779089)
on Ubuntu 24.04 at source revision `8801c7b29a208f45bed53d9d581ae8d99576e57e`.
`GENERATOR.txt` records the generator version, package, command and grammar
SHA-256. The portability inventory records the generated file's SHA-256.

The generated file retains the notices copied from the grammar, including
the ISC notice for portability changes. Berkeley yacc's parser skeleton
is public domain. Users build the checked-in parser without a generator.

The manual CI parser job regenerates in a temporary directory, uploads
`generated-date-parser` and compares it with the checked-in files. After
a grammar change, retrieve the artifact even if the comparison fails.
Review and incorporate both files together with the provenance hash before
requesting verification of the new revision.

## Date expression semantics

`date_parse` in `src/date.y` parses user-supplied dates and revision
selectors. Missing calendar fields default to the current local date.
The clock fields default to midnight and the time zone defaults to local
time. A relative-only expression starts from the current instant.
The parser applies relative seconds, then calendar months, then a weekday
adjustment when there was no explicit date.
It uses shared parser state for the current expression, so calls must be
serialized. Callers treat a return value of `(time_t)-1` as an error.

The Entries and Tag readers in `src/entries.c` use a separate fixed UTC
timestamp format for on-disk sticky dates.
