# Development notes

## Milestone 1: application and safe parsing

The first build introduces an independent C++ core, an owned file snapshot, checked
little-endian reads, PE32/PE32+ parsing, and a Qt window with an asynchronous overview.
`core_tests` validates both optional header layouts, raw-backed RVA translation,
invalid signatures, corrupt offsets and truncation at every byte of a fixture.

Qt owns UI objects through its parent-child tree. Raw UI pointers are non-owning
handles; no manual `delete` is needed. Analysis results use shared immutable
ownership so views and workers can safely retain a snapshot during reload.

## Parser policy

No OS image loading APIs, structure casts, or analyzed-code execution are used.
File offsets and RVAs are translated explicitly. Virtual zero-fill does not map to
file bytes. Section overlaps are rejected because they make mapping ambiguous.
This is deliberately stricter than tools which attempt to repair damaged images.

Reference: [Microsoft PE/COFF specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
Deployment follows [Qt's CMake deployment API](https://doc.qt.io/qt-6/cmake-deployment.html).

## Subsequent milestones

- Header and section tables were followed by separately bounded import/export
  parsing. Named and ordinal imports, export aliases, holes and forwarders are
  covered by synthetic fixtures. Structural inspection was also checked against
  the installed Windows `kernel32.dll` and an x86 image.
- String extraction stores offsets instead of duplicate strings. ASCII EOF and
  both wide-string alignments are tested. Filtering uses overlapping bounded
  decoding windows so a very long string does not require a huge temporary QString.
- The hex view paints visible rows through `QAbstractScrollArea`; background byte
  searches retain immutable snapshots and discard stale results. Explicit jumps
  cancel outstanding searches.
- Entropy has known-distribution tests (constant and uniform bytes). Disassembly
  is behind a format-independent interface; Capstone handles the default build,
  with instruction boundary tests for x64 and ARM64. The no-dependency backend
  stops conservatively at unsupported bytes.
- Global search, settings and the worker pipeline are kept outside the view
  implementation. Worker errors are returned as values, preserving the original
  message through QtConcurrent.

Qt tests operate the complete window, filters, clipboard, search dialog and reload.
One test appends 64 MiB of text to a valid executable snapshot and checks that the
UI timer still runs while analysis proceeds. Screenshot captures use the same
theme as the shipped application; only the offscreen test font registration differs.

The application, parser-only configuration and deployed Windows executable are
validated separately. All analyzed binaries are read as bytes, including test
executables; they are never launched by an analysis operation.
