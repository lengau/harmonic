//! Core generation engine for Harmonic.
//!
//! Calls a configurable AI coding tool in non-interactive mode.

use std::io;
use std::process::Command;

use serde::Deserialize;

/// Response from opencode in JSON mode.
#[derive(Debug, Deserialize)]
struct OpencodeResponse {
    #[serde(default)]
    content: String,
    #[serde(default)]
    response: String,
}

/// Configuration for the generation backend.
pub struct EngineConfig {
    pub backend: String,
    pub command: String,
    pub model: String,
    pub api_key: String,
    pub send_context: bool,
}

impl Default for EngineConfig {
    fn default() -> Self {
        Self {
            backend: "opencode".to_string(),
            command: "opencode".to_string(),
            model: String::new(),
            api_key: String::new(),
            send_context: true,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Backend {
    Opencode,
    ClaudeCode,
    Copilot,
    Custom,
}

impl Backend {
    fn from_name(name: &str) -> Result<Self, EngineError> {
        match name {
            "opencode" => Ok(Self::Opencode),
            "claude-code" => Ok(Self::ClaudeCode),
            "copilot" => Ok(Self::Copilot),
            "custom" => Ok(Self::Custom),
            _ => Err(EngineError::invalid_backend(format!(
                "Unknown backend: {name}. Expected one of: copilot, opencode, claude-code, custom."
            ))),
        }
    }
}

/// Generate code using the configured AI backend.
pub fn generate(
    prompt: &str,
    context: Option<&str>,
    config: &EngineConfig,
) -> Result<String, EngineError> {
    let backend = Backend::from_name(&config.backend)?;
    let context_for_prompt = if config.send_context { context } else { None };

    let full_prompt = match context_for_prompt {
        Some(ctx) => format!(
            "Given the following code context:\n```\n{ctx}\n```\n\n{prompt}\n\nRespond with ONLY the generated code, no explanation."
        ),
        None => format!("{prompt}\n\nRespond with ONLY the generated code, no explanation."),
    };

    match backend {
        Backend::Opencode => run_opencode(
            &config.command,
            &full_prompt,
            &config.model,
            &config.api_key,
        ),
        Backend::ClaudeCode => run_claude_code(
            &config.command,
            &full_prompt,
            &config.model,
            &config.api_key,
        ),
        Backend::Copilot => run_copilot(&config.command, &full_prompt),
        Backend::Custom => run_custom(&config.command, &full_prompt),
    }
}

fn run_opencode(
    command: &str,
    prompt: &str,
    model: &str,
    api_key: &str,
) -> Result<String, EngineError> {
    let cmd = if command.is_empty() {
        "opencode"
    } else {
        command
    };

    let mut args = vec!["-p", prompt, "-f", "json", "-q"];
    if !model.is_empty() {
        args.push("-m");
        args.push(model);
    }

    let mut proc = Command::new(cmd);
    proc.args(&args);
    if !api_key.is_empty() {
        proc.env("ANTHROPIC_API_KEY", api_key);
        proc.env("OPENAI_API_KEY", api_key);
    }

    let output = proc
        .output()
        .map_err(|e| EngineError::spawn_failed(format!("{cmd}: {e}")))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(EngineError::process_failed(stderr.to_string()));
    }

    let stdout = String::from_utf8_lossy(&output.stdout);

    if let Ok(resp) = serde_json::from_str::<OpencodeResponse>(&stdout) {
        let text = if resp.content.is_empty() {
            resp.response
        } else {
            resp.content
        };
        Ok(strip_code_fences(&text))
    } else {
        Ok(strip_code_fences(stdout.trim()))
    }
}

fn run_claude_code(
    command: &str,
    prompt: &str,
    model: &str,
    api_key: &str,
) -> Result<String, EngineError> {
    let cmd = if command.is_empty() {
        "claude"
    } else {
        command
    };

    let mut proc = Command::new(cmd);
    proc.args(["--print", prompt]);
    if !model.is_empty() {
        proc.args(["--model", model]);
    }
    if !api_key.is_empty() {
        proc.env("ANTHROPIC_API_KEY", api_key);
    }

    let output = proc
        .output()
        .map_err(|e| EngineError::spawn_failed(format!("{cmd}: {e}")))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(EngineError::process_failed(stderr.to_string()));
    }

    let stdout = String::from_utf8_lossy(&output.stdout);
    Ok(strip_code_fences(stdout.trim()))
}

fn run_copilot(command: &str, prompt: &str) -> Result<String, EngineError> {
    let cmd = if command.is_empty() {
        "copilot"
    } else {
        command
    };

    let output = Command::new(cmd)
        .args(["-p", prompt, "--output-format", "text", "--silent"])
        .output()
        .map_err(|e| EngineError::spawn_failed(format!("{cmd}: {e}")))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(EngineError::process_failed(stderr.to_string()));
    }

    let stdout = String::from_utf8_lossy(&output.stdout);
    Ok(strip_code_fences(stdout.trim()))
}

fn run_custom(command: &str, prompt: &str) -> Result<String, EngineError> {
    if command.is_empty() {
        return Err(EngineError::invalid_config(
            "No custom command configured".to_string(),
        ));
    }

    let output = Command::new("sh")
        .args(["-c", &format!("{command} {}", shell_escape(prompt))])
        .output()
        .map_err(|e| EngineError::spawn_failed(e.to_string()))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(EngineError::process_failed(stderr.to_string()));
    }

    let stdout = String::from_utf8_lossy(&output.stdout);
    Ok(strip_code_fences(stdout.trim()))
}

fn shell_escape(s: &str) -> String {
    format!("'{}'", s.replace('\'', "'\\''"))
}

/// Strip markdown code fences if present.
fn strip_code_fences(s: &str) -> String {
    let trimmed = s.trim();
    if trimmed.starts_with("```") {
        let without_opening = trimmed.strip_prefix("```").unwrap_or(trimmed);
        let after_lang = without_opening
            .find('\n')
            .map(|i| &without_opening[i + 1..])
            .unwrap_or(without_opening);
        after_lang
            .strip_suffix("```")
            .unwrap_or(after_lang)
            .trim()
            .to_string()
    } else {
        trimmed.to_string()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EngineErrorKind {
    InvalidBackend,
    InvalidConfig,
    SpawnFailed,
    ProcessFailed,
}

#[derive(Debug)]
pub struct EngineError {
    kind: EngineErrorKind,
    message: String,
}

impl EngineError {
    fn invalid_backend(message: String) -> Self {
        Self {
            kind: EngineErrorKind::InvalidBackend,
            message,
        }
    }

    fn invalid_config(message: String) -> Self {
        Self {
            kind: EngineErrorKind::InvalidConfig,
            message,
        }
    }

    fn spawn_failed(message: String) -> Self {
        Self {
            kind: EngineErrorKind::SpawnFailed,
            message,
        }
    }

    fn process_failed(message: String) -> Self {
        Self {
            kind: EngineErrorKind::ProcessFailed,
            message,
        }
    }

    pub fn kind(&self) -> EngineErrorKind {
        self.kind
    }

    pub fn message(&self) -> &str {
        &self.message
    }
}

impl std::fmt::Display for EngineError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self.kind {
            EngineErrorKind::InvalidBackend => write!(f, "Invalid backend: {}", self.message),
            EngineErrorKind::InvalidConfig => write!(f, "Invalid configuration: {}", self.message),
            EngineErrorKind::SpawnFailed => write!(f, "Failed to launch backend: {}", self.message),
            EngineErrorKind::ProcessFailed => write!(f, "Backend error: {}", self.message),
        }
    }
}

impl From<EngineError> for io::Error {
    fn from(e: EngineError) -> Self {
        io::Error::other(e.to_string())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_strip_code_fences_with_lang() {
        let input = "```rust\nfn main() {}\n```";
        assert_eq!(strip_code_fences(input), "fn main() {}");
    }

    #[test]
    fn test_strip_code_fences_without_lang() {
        let input = "```\nhello\n```";
        assert_eq!(strip_code_fences(input), "hello");
    }

    #[test]
    fn test_strip_code_fences_no_fences() {
        let input = "fn main() {}";
        assert_eq!(strip_code_fences(input), "fn main() {}");
    }

    #[test]
    fn test_shell_escape() {
        assert_eq!(shell_escape("hello world"), "'hello world'");
        assert_eq!(shell_escape("it's"), "'it'\\''s'");
    }

    #[test]
    fn test_generate_rejects_unknown_backend() {
        let config = EngineConfig {
            backend: "unknown".to_string(),
            command: String::new(),
            model: String::new(),
            api_key: String::new(),
            send_context: true,
        };
        let error = generate("hello", None, &config).unwrap_err();
        assert_eq!(error.kind(), EngineErrorKind::InvalidBackend);
    }

    #[test]
    fn test_generate_custom_requires_command() {
        let config = EngineConfig {
            backend: "custom".to_string(),
            command: String::new(),
            model: String::new(),
            api_key: String::new(),
            send_context: true,
        };
        let error = generate("hello", None, &config).unwrap_err();
        assert_eq!(error.kind(), EngineErrorKind::InvalidConfig);
    }
}
