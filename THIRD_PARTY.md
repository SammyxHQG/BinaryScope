# Third-party components

## Qt

BinaryScope links dynamically to Qt 6 Core, Gui, Widgets and Concurrent. Qt Test is
used only by the UI tests. Qt is available under commercial and open-source terms;
use the terms applicable to your SDK and distribution. The local development SDK
is Qt 6.8.3, downloaded from the official Qt repository.

When distributing binaries, include the relevant Qt license notices and fulfill
the source/relinking requirements of the license you use. This repository does not
relicense Qt. See <https://www.qt.io/licensing/> and the SDK's `licenses/` directory.

## Capstone

The optional disassembly backend uses Capstone 5.0.6, under its BSD license and
the notices in its upstream `LICENSE.TXT` and source files. CMake installs the
license into `licenses/capstone/`. The dependency is fetched from the upstream
release archive and verified with a pinned SHA-256 hash.

Source: <https://github.com/capstone-engine/capstone/tree/5.0.6>.

## Fonts

The UI requests system-installed Segoe UI and Consolas. Fonts are not bundled.

## Microsoft C++ runtime

Windows MSVC builds deploy the required runtime DLLs through CMake's
`InstallRequiredSystemLibraries` module. Those files retain Microsoft's licensing
terms and are not part of BinaryScope's own source code.
