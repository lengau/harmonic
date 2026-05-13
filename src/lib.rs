//! Harmonic – AI-assisted vibecoding core library.
//!
//! This crate exposes a C-compatible FFI for the Kate plugin (C++/Qt) to call into.
//! It uses a configurable AI backend (opencode, claude-code, or custom).

pub mod engine;

use std::ffi::{CStr, CString};
use std::os::raw::c_char;

use engine::EngineConfig;

/// Helper to convert a C string pointer to an Option<&str>.
unsafe fn cstr_to_option(ptr: *const c_char) -> Option<&'static str> {
    if ptr.is_null() {
        None
    } else {
        unsafe { CStr::from_ptr(ptr) }.to_str().ok()
    }
}

/// Generate code from a natural language prompt with full configuration.
///
/// # Safety
/// All pointer parameters must be valid null-terminated C strings or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn harmonic_generate(
    prompt: *const c_char,
    context: *const c_char,
    backend: *const c_char,
    command: *const c_char,
    model: *const c_char,
    api_key: *const c_char,
    send_context: bool,
) -> *mut c_char {
    let prompt_str = unsafe { CStr::from_ptr(prompt) }.to_str().unwrap_or("");

    let config = EngineConfig {
        backend: unsafe { cstr_to_option(backend) }.unwrap_or("opencode").to_string(),
        command: unsafe { cstr_to_option(command) }.unwrap_or("opencode").to_string(),
        model: unsafe { cstr_to_option(model) }.unwrap_or("").to_string(),
        api_key: unsafe { cstr_to_option(api_key) }.unwrap_or("").to_string(),
        send_context,
    };

    let context_str = unsafe { cstr_to_option(context) };

    let result = match engine::generate(prompt_str, context_str, &config) {
        Ok(code) => code,
        Err(e) => format!("// Error: {e}"),
    };

    CString::new(result).unwrap_or_default().into_raw()
}

/// Free a string previously returned by harmonic functions.
///
/// # Safety
/// `ptr` must have been returned by a harmonic FFI function, or be null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn harmonic_free_string(ptr: *mut c_char) {
    if !ptr.is_null() {
        unsafe {
            drop(CString::from_raw(ptr));
        }
    }
}

/// Return the library version as a C string. Caller must free with `harmonic_free_string`.
#[unsafe(no_mangle)]
pub extern "C" fn harmonic_version() -> *mut c_char {
    CString::new(env!("CARGO_PKG_VERSION"))
        .unwrap_or_default()
        .into_raw()
}
