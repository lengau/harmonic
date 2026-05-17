//! Harmonic – AI-assisted vibecoding core library.
//!
//! This crate exposes a C-compatible FFI for the Kate plugin (C++/Qt) to call into.
//! It uses a configurable AI backend (opencode, claude-code, or custom).

pub mod engine;

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;

use engine::{EngineConfig, EngineErrorKind};

pub const HARMONIC_STATUS_OK: i32 = 0;
pub const HARMONIC_STATUS_INVALID_ARGUMENT: i32 = 1;
pub const HARMONIC_STATUS_INVALID_BACKEND: i32 = 2;
pub const HARMONIC_STATUS_INVALID_CONFIG: i32 = 3;
pub const HARMONIC_STATUS_SPAWN_FAILED: i32 = 4;
pub const HARMONIC_STATUS_BACKEND_FAILED: i32 = 5;

#[repr(C)]
pub struct HarmonicResult {
    pub status: i32,
    pub output: *mut c_char,
    pub error: *mut c_char,
}

fn into_c_string_ptr(value: &str) -> *mut c_char {
    let sanitized = value.replace('\0', "\\0");
    CString::new(sanitized).unwrap_or_default().into_raw()
}

/// Helper to convert a C string pointer to an Option<&str>.
unsafe fn cstr_to_string(ptr: *const c_char) -> Option<String> {
    if ptr.is_null() {
        None
    } else {
        unsafe { CStr::from_ptr(ptr) }
            .to_str()
            .ok()
            .map(str::to_string)
    }
}

unsafe fn cstr_required(ptr: *const c_char, field: &str) -> Result<String, HarmonicResult> {
    if ptr.is_null() {
        return Err(HarmonicResult {
            status: HARMONIC_STATUS_INVALID_ARGUMENT,
            output: ptr::null_mut(),
            error: into_c_string_ptr(&format!("{field} is required")),
        });
    }

    unsafe { CStr::from_ptr(ptr) }
        .to_str()
        .map(str::to_string)
        .map_err(|_| HarmonicResult {
            status: HARMONIC_STATUS_INVALID_ARGUMENT,
            output: ptr::null_mut(),
            error: into_c_string_ptr(&format!("{field} must be valid UTF-8")),
        })
}

fn status_from_engine_error_kind(kind: EngineErrorKind) -> i32 {
    match kind {
        EngineErrorKind::InvalidBackend => HARMONIC_STATUS_INVALID_BACKEND,
        EngineErrorKind::InvalidConfig => HARMONIC_STATUS_INVALID_CONFIG,
        EngineErrorKind::SpawnFailed => HARMONIC_STATUS_SPAWN_FAILED,
        EngineErrorKind::ProcessFailed => HARMONIC_STATUS_BACKEND_FAILED,
    }
}

/// Generate code from a natural language prompt with full configuration.
///
/// # Safety
/// All pointer parameters must be valid null-terminated C strings or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn harmonic_generate_result(
    prompt: *const c_char,
    context: *const c_char,
    backend: *const c_char,
    command: *const c_char,
    model: *const c_char,
    api_key: *const c_char,
    send_context: bool,
) -> HarmonicResult {
    let prompt_str = match unsafe { cstr_required(prompt, "prompt") } {
        Ok(prompt) => prompt,
        Err(result) => return result,
    };

    let config = EngineConfig {
        backend: unsafe { cstr_to_string(backend) }.unwrap_or_else(|| "opencode".to_string()),
        command: unsafe { cstr_to_string(command) }.unwrap_or_else(|| "opencode".to_string()),
        model: unsafe { cstr_to_string(model) }.unwrap_or_default(),
        api_key: unsafe { cstr_to_string(api_key) }.unwrap_or_default(),
        send_context,
    };

    let context_str = unsafe { cstr_to_string(context) };

    match engine::generate(&prompt_str, context_str.as_deref(), &config) {
        Ok(code) => HarmonicResult {
            status: HARMONIC_STATUS_OK,
            output: into_c_string_ptr(&code),
            error: ptr::null_mut(),
        },
        Err(e) => HarmonicResult {
            status: status_from_engine_error_kind(e.kind()),
            output: ptr::null_mut(),
            error: into_c_string_ptr(e.message()),
        },
    }
}

/// Generate code from a natural language prompt with a legacy string-only response.
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
    let result = unsafe {
        harmonic_generate_result(
            prompt,
            context,
            backend,
            command,
            model,
            api_key,
            send_context,
        )
    };

    if result.status == HARMONIC_STATUS_OK {
        if !result.error.is_null() {
            unsafe { harmonic_free_string(result.error) };
        }
        return result.output;
    }

    let error_text = if result.error.is_null() {
        "Harmonic generation failed".to_string()
    } else {
        unsafe { CStr::from_ptr(result.error) }
            .to_string_lossy()
            .into_owned()
    };
    unsafe { harmonic_free_result(result) };

    into_c_string_ptr(&format!("// Error: {error_text}"))
}

/// Free strings inside a HarmonicResult previously returned by harmonic functions.
///
/// # Safety
/// pointers inside result must have been returned by a harmonic FFI function, or be null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn harmonic_free_result(result: HarmonicResult) {
    unsafe { harmonic_free_string(result.output) };
    unsafe { harmonic_free_string(result.error) };
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

#[cfg(test)]
mod tests {
    use super::*;
    use std::ptr;

    #[test]
    fn ffi_rejects_null_prompt() {
        let result = unsafe {
            harmonic_generate_result(
                ptr::null(),
                ptr::null(),
                ptr::null(),
                ptr::null(),
                ptr::null(),
                ptr::null(),
                true,
            )
        };
        assert_eq!(result.status, HARMONIC_STATUS_INVALID_ARGUMENT);
        assert!(!result.error.is_null());
        unsafe { harmonic_free_result(result) };
    }

    #[test]
    fn ffi_reports_invalid_backend() {
        let prompt = CString::new("hello").unwrap();
        let backend = CString::new("not-a-backend").unwrap();
        let result = unsafe {
            harmonic_generate_result(
                prompt.as_ptr(),
                ptr::null(),
                backend.as_ptr(),
                ptr::null(),
                ptr::null(),
                ptr::null(),
                true,
            )
        };
        assert_eq!(result.status, HARMONIC_STATUS_INVALID_BACKEND);
        assert!(!result.error.is_null());
        unsafe { harmonic_free_result(result) };
    }

    #[test]
    fn ffi_classifies_spawn_failures() {
        let prompt = CString::new("hello").unwrap();
        let backend = CString::new("opencode").unwrap();
        let command = CString::new("__harmonic_nonexistent_backend__").unwrap();
        let result = unsafe {
            harmonic_generate_result(
                prompt.as_ptr(),
                ptr::null(),
                backend.as_ptr(),
                command.as_ptr(),
                ptr::null(),
                ptr::null(),
                true,
            )
        };
        assert_eq!(result.status, HARMONIC_STATUS_SPAWN_FAILED);
        assert!(!result.error.is_null());
        unsafe { harmonic_free_result(result) };
    }

    #[test]
    fn ffi_classifies_backend_failures() {
        let prompt = CString::new("hello").unwrap();
        let backend = CString::new("custom").unwrap();
        let command = CString::new("false").unwrap();
        let result = unsafe {
            harmonic_generate_result(
                prompt.as_ptr(),
                ptr::null(),
                backend.as_ptr(),
                command.as_ptr(),
                ptr::null(),
                ptr::null(),
                true,
            )
        };
        assert_eq!(result.status, HARMONIC_STATUS_BACKEND_FAILED);
        assert!(!result.error.is_null());
        unsafe { harmonic_free_result(result) };
    }
}
