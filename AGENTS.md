# Repository Guidelines

## Project Structure & Module Organization
This repository is a native Qt desktop app for the Octra wallet and contract tools. The active app lives under `native/` (`qt_main.cpp`, `qt_app_bridge.cpp`, `qml/Main.qml`). Core wallet and crypto code is split across `lib/`, `pvac/`, `wallet.hpp`, and `crypto_utils.hpp`. Static web assets and templates live in `static/`, while Windows packaging files are under `packaging/windows/`. `main.cpp` is legacy and should not be treated as the primary desktop entry point.

## Build, Test, and Development Commands
- `./setup.sh` - install dependencies and build on Unix-like systems.
- `./setup.sh --deps-only` - install toolchain and libraries without building.
- `cmake -S . -B build-native && cmake --build build-native -j2` - configure and build the Qt app manually.
- `./build-native/octra_wallet_native` - run the native desktop app after a CMake build.
- `make` - build the standalone target used by the legacy flow.
- `make run` - build and start the local app on port `8420`.
- `setup.bat` - Windows dependency install and build helper from `cmd.exe`.

## Coding Style & Naming Conventions
Target C++17 and keep changes consistent with the surrounding file. Match existing Qt/C++ style rather than introducing new formatting rules. Prefer small, focused functions, descriptive names, and lower-case resource paths for assets and templates. Keep QML files and UI resources organized by feature, and avoid renaming paths unless necessary.

## Testing Guidelines
There is no formal automated test suite in the repo. Validate changes by building the relevant target on the affected platform and exercising the UI or wallet flow manually. For desktop UI changes, verify the native app launches, the modified screen renders correctly, and the affected workflow still works end to end.

## Commit & Pull Request Guidelines
Recent commits use short, imperative subject lines such as `Fix Linux CI native app build target`. Keep commit summaries concise and action-oriented. Pull requests should describe the user-facing change, list the platforms tested, and include screenshots or short screen recordings for UI updates. Link the related issue or task when available.

## Security & Configuration Tips
Do not commit build outputs such as `build-native/`, `octra_wallet`, or packaged archives. Windows signed releases rely on `WIN_SIGN_PFX_BASE64` and `WIN_SIGN_PFX_PASSWORD`; keep those secrets out of the repo. If you touch packaging or release logic, verify the unsigned and signed paths separately.
