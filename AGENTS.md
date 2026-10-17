# AGENTS.md

`chiaki-ng` — cross-platform PlayStation Remote Play client. C core + Qt6/QML GUI, built with CMake (Ninja in CI).

## Read first

- `CONTRIBUTOR_GUIDE.md` — architecture map: directory ownership, video/audio/input paths, where to start per bug type.
- `.github/copilot-instructions.md` — code conventions: `*_init`/`*_fini` pairing, propagate `ChiakiErrorCode`, feature-gate rules, editing boundaries.
- `.agents/skills/` — repo-local skills (e.g. `cpp-coding-standards`, `github-actions-templates`).
- Trust executable sources (workflows, CMake, scripts) over prose: `doc/platform-build.md` is Qt5-era and references a removed `scripts/appveyor.sh`.

## Commands

```sh
git submodule update --init --recursive    # always first: test/munit + third-party/* needed to even configure
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target chiaki        # desktop GUI app
cmake --build build --target chiaki-unit   # unit tests (CHIAKI_ENABLE_TESTS defaults to ON)
build/test/chiaki-unit                     # run all tests (add .exe on Windows)
build/test/chiaki-unit /chiaki/aia/        # run one suite; suites are /chiaki/<name>, list in test/main.c
ctest --test-dir build --output-on-failure # equivalent; tests register as ctest name "unit"
```

Verification reality:

- There is no linter, formatter, or typecheck. Configure + build + `chiaki-unit` is the entire local verification.
- GitHub PR CI (`.github/workflows/build-pr.yaml`) only builds packages (Windows MSYS2, macOS arm64, AppImages) — it runs **no tests**, so nothing runs them for your PR. Tests execute only inside packaging scripts (`scripts/build-common.sh`, `scripts/build-appimage.sh`), sourcehut `.builds/`, and legacy AppVeyor (`master`/`develop` only). Run `chiaki-unit` yourself.
- Unit coverage is small; renderer/audio/timing changes need manual live validation.

## Build and codegen quirks

- `protoc` must be on PATH at configure time (hard `FATAL_ERROR`) and Python 3 is required (nanopb generator). Codegen: `lib/protobuf/takion.proto` → `takion.pb.c/.h` in the **build dir only**; never commit generated protobuf output.
- After a protobuf upgrade, stale `third-party/nanopb/generator/proto/nanopb_pb2.py` breaks the build; delete it (`scripts/build-common.sh` and `scripts/build-appimage.sh` do this automatically).
- Most CMake options are tri-state `AUTO|ON|OFF` (`tri_option` macro) and silently auto-disable when deps are missing (FFmpeg, SpeexDSP, setsu, Steam Deck native, system jerasure/nanopb/curl). Check configure messages before assuming a feature is enabled.
- On Windows with bundled curl (`CHIAKI_USE_SYSTEM_CURL=OFF`, the default and the official build path): configure copies the curl submodule and applies `cmake/curl-schannel-aia.patch`, **failing hard if it no longer applies** — rebase the patch when bumping the curl submodule. Not applied on non-Windows or system-curl builds. Details: `cmake/curl-schannel-aia.md`.
- GUI hard-requires Qt6, SDL2, FFmpeg, and libplacebo >= 7.349.0 via pkg-config.
- Dependency pins live in three places — root `CMakeLists.txt`, `scripts/`, `.github/workflows/` — update all of them together (libplacebo, builder image tags, FFmpeg URLs, …). Version comes from `CHIAKI_VERSION_*` in root `CMakeLists.txt`; CI greps it for package names.
- Packaged WebEngine builds need `scripts/qtwebengine_import.qml` copied into `gui/src/qml/` (CI does this; the file is not tracked there).
- `gui/CMakeFiles/*_autogen.dir/*.json` are tracked but stale generated files (Flatpak `/app` paths baked in); they are not source — don't edit or "fix" them.
- Keep LF line endings in shell/build scripts; Windows CI explicitly sets `core.autocrlf=false` before checkout.

## Platform builds

- **Windows (official)**: MSYS2 mingw64 + Ninja. `.github/workflows/build-pr.yaml` is the working template: pacboy deps, `scripts/build-libplacebo-windows.sh`, FFmpeg from `streetpea/FFmpeg-Builds`, `scripts/build-sdl2-compat.sh`. Legacy vcpkg/clang-cl kit config: `meson.ini` + `vcpkg.json`.
- **Linux packaging**: `scripts/build-appimage.sh` runs inside `docker.io/streetpea/chiaki-ng-builder` (see workflow for exact invocation); Flatpak manifests in `scripts/flatpak/`.
- **Android**: builds the root `CMakeLists.txt` via Gradle with GUI/CLI/FFmpeg/tests disabled (`android/app/build.gradle`).
- **Docs site**: `docs/` is an mkdocs-material site; `mkdocs serve` after `pip3 install mkdocs mkdocs-material mkdocs-git-revision-date-localized-plugin mkdocs-print-site-plugin`.

## Editing boundaries

- First-party: `lib/`, `gui/`, `cli/`, `test/`, `cmake/`, `scripts/`.
- Avoid edits in vendored trees: `third-party/`, `switch/borealis/`, `android/app/src/main/cpp/oboe/`, `test/munit/`.
- If you change build options or dependencies, update the workflows/scripts in the same change.
