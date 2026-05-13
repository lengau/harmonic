# Harmonic

Harmonic is an AI-assisted vibecoding plugin for the Kate editor. It combines a Rust core library with a C++/Qt Kate plugin so you can generate code from natural-language prompts directly inside Kate.

## What it does

Harmonic adds AI-powered coding workflows to Kate, including:

- **Vibecode generation** from a prompt, selection, or comment near the cursor
- **In-editor chat** with an AI coding assistant in a Kate tool view
- **Configurable backends** such as GitHub Copilot CLI, OpenCode, Claude Code, or a custom command
- **Optional file-context sending** so the active document can be included in prompts

The plugin calls into a Rust library through a small C FFI layer declared in [`include/harmonic.h`](include/harmonic.h).

## Architecture

The repository is split into two main parts:

### Rust core (`src/`)

- [`src/lib.rs`](src/lib.rs) exposes a C-compatible API:
  - `harmonic_generate(...)`
  - `harmonic_free_string(...)`
  - `harmonic_version()`
- [`src/engine.rs`](src/engine.rs) builds prompts and invokes the configured AI backend
- The crate is built as both:
  - `cdylib` for the Kate plugin / FFI integration
  - `rlib` for native Rust linking and reuse

### Kate plugin (`plugin/`)

- C++ plugin built with **Qt 6**, **KDE Frameworks 6**, and **CMake**
- Adds a Kate action for code generation and a chat sidebar/tool view
- Reads user configuration for backend, command, model, API key, and whether to send file context
- Uses the Rust FFI library for the **Vibecode** action and links against the Rust-produced shared library (`libharmonic.so`)
- Streams chat responses directly from supported CLI backends inside the plugin UI

## Build requirements

### Rust core

- Rust toolchain with `cargo`

### Kate plugin

- CMake 3.20+
- Rust toolchain with `cargo`
- Qt 6 (`Core`, `Widgets`)
- KDE Frameworks 6:
  - `KF6TextEditor`
  - `KF6I18n`
  - `KF6XmlGui`
  - `KF6Config`
- Extra CMake Modules (ECM)
- Kate development environment / headers appropriate for your distro

## Building

### Build the Rust library

From the repository root:

```sh
cargo build --release
```

This produces the Rust artifacts under `target/release/`, including the shared library used by the plugin (`libharmonic.so`).

### Build the Kate plugin

From the repository root:

```sh
cmake -S plugin -B plugin/build -DCMAKE_BUILD_TYPE=Release
cmake --build plugin/build -j
```

The plugin build is configured to invoke Cargo automatically and build the Rust core in release mode before linking the Kate plugin.

### Install the Kate plugin

A typical local install looks like:

```sh
cmake --install plugin/build --prefix "$HOME/.local"
```

Install paths are controlled by KDE CMake install macros, so exact destinations depend on your platform and prefix.

## Usage overview

- Trigger **Harmonic: Vibecode** in Kate to generate code from the current selection, a comment on the current line, or a typed prompt.
- Use the **Harmonic Chat** sidebar to have a running conversation with the configured AI backend.
- Configure the backend command, model, API key, and context behavior from the plugin configuration page.

## Development notes

- The public FFI header is [`include/harmonic.h`](include/harmonic.h).
- The Rust core currently depends on `serde` and `serde_json`.
- Backend execution is handled by spawning external CLI tools from the Rust engine or the plugin chat UI.

## License

Harmonic is licensed under the **MIT License**.
