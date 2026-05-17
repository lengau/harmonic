# Harmonic — Copilot agent instructions

Harmonic is an AI-assisted coding plugin for the [Kate](https://kate-editor.org) editor.
It combines a Rust core library with a C++/Qt Kate plugin.

## Quality bar

KDE conventions are the primary source of truth for plugin-side code.

- Keep changes surgical; do not refactor unrelated code.
- Match existing repository patterns and naming before introducing new helpers.
- Preserve user-visible behavior unless the task explicitly requires a behavior change.
- Wrap user-visible plugin strings in `i18n()` / `i18nc()`.
- Use Qt/KDE signal-slot idioms used in this repo (`Q_OBJECT`, `Q_SLOTS`, `Q_EMIT`).
- Keep plugin member variables with the `m_` prefix, consistent with current code.
- Follow current header style in this repository (include guards are used today).
- Avoid silent error handling and broad catches; surface actionable errors.
- Branch names for agent work should use `work/<description>`.

## Code review focus

When reviewing or generating code:

- Prioritize technical correctness (Qt/KF6 lifecycle correctness, async/process handling, FFI safety, and memory ownership).
- Enforce formatting and lint expectations from this repo (`cargo fmt`, `cargo clippy`, `clang-format` where applicable).
- Prefer the smallest correct change over larger rewrites.

## Tech stack

| Layer | Library / version |
|-------|-------------------|
| Rust core | Rust edition 2024 (`cdylib` + `rlib`) |
| Kate plugin | C++20, Qt 6, KDE Frameworks 6 |
| KDE libs used | KF6 TextEditor, I18n, XmlGui, Config |
| Build system | Cargo (Rust) + CMake 3.20+ (plugin) |
| CI runtime | Ubuntu 26.04 LXD container in GitHub Actions |

## Repository layout

Significant paths:

```text
harmonic/
├── .github/
│   ├── copilot-instructions.md   # This file
│   └── workflows/ci.yml          # CI pipeline (lint/test/build in LXD)
├── .clang-format                 # C++ formatting rules
├── Cargo.toml                    # Rust crate manifest
├── include/harmonic.h            # Public C FFI header
├── src/
│   ├── lib.rs                    # FFI exports: generate/free/version
│   └── engine.rs                 # Backend command execution logic
└── plugin/
    ├── CMakeLists.txt            # Plugin target + Rust build integration
    ├── harmonicplugin.*          # Kate plugin and view wiring
    ├── harmonicchatwidget.*      # Chat tool view and streaming behavior
    ├── harmonicconfigpage.*      # Settings page UI
    ├── harmonicacp.*             # ACP protocol integration
    ├── harmonicmarkdown.*        # Markdown rendering helpers
    ├── harmonicplugin.json       # Plugin metadata
    └── harmonicplugin.rc         # Kate XMLGUI actions
```

## Architecture

```text
Kate action/UI (C++/Qt plugin)
  ├─ Vibecode flow → FFI (`harmonic_generate`) → Rust engine
  │                   └─ invokes configured CLI backend (copilot/opencode/claude-code/custom)
  └─ Chat flow → process/ACP streaming in plugin UI
```

The Rust core is the execution layer for prompt-to-code generation.
The plugin manages user interaction, context extraction, and insertion into documents.

## Build, lint, and test

Use repo-native commands:

```bash
# Rust formatting + lint
cargo fmt --check --manifest-path Cargo.toml
cargo clippy --manifest-path Cargo.toml --all-targets --all-features -- -D warnings

# Rust tests
cargo test --locked --manifest-path Cargo.toml --all-targets --all-features

# Build Rust release artifact used by plugin
cargo build --release --manifest-path Cargo.toml

# Build plugin (invokes Rust release build through CMake custom command)
cmake -S plugin -B plugin/build -DCMAKE_BUILD_TYPE=Release
cmake --build plugin/build -j"$(nproc)"
```

For C++ formatting checks, follow `.clang-format` and CI logic (changed C/C++ files).

## CI

`.github/workflows/ci.yml` runs one `build-and-test` job:

1. Determines changed C/C++ files for `clang-format --dry-run --Werror`.
2. Launches an Ubuntu 26.04 LXD container.
3. Installs build/toolchain dependencies.
4. Runs Rust fmt/clippy, C++ format checks, Rust tests, and plugin build.

If local behavior differs from CI, treat CI workflow behavior as authoritative.

## Backend behavior notes

- `src/engine.rs` supports `opencode`, `claude-code`, `copilot`, and `custom` backends.
- Backend command failures must return explicit errors, not silent fallbacks.
- Preserve prompt/context behavior unless explicitly requested to change it.

## Internationalization

For plugin UI code, wrap visible strings in KDE i18n helpers:

```cpp
#include <KLocalizedString>
i18n("Harmonic");
i18nc("@action:button", "Send");
```

Do not concatenate translated fragments; use argument substitution.
