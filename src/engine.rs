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

/// Generate code using the configured AI backend.
pub fn generate(prompt: &str, context: Option<&str>, config: &EngineConfig) -> Result<String, EngineError> {
    let context_for_prompt = if config.send_context { context } else { None };

    let full_prompt = match context_for_prompt {
        Some(ctx) => format!(
            "Given the following code context:\n```\n{ctx}\n```\n\n{prompt}\n\nRespond with ONLY the generated code, no explanation."
        ),
        None => format!(
            "{prompt}\n\nRespond with ONLY the generated code, no explanation."
        ),
    };

    match config.backend.as_str() {
        "opencode" => run_opencode(&config.command, &full_prompt, &config.model, &config.api_key),
        "claude-code" => run_claude_code(&config.command, &full_prompt, &config.model, &config.api_key),
        "copilot" => run_copilot(&config.command, &full_prompt),
        "custom" => run_custom(&config.command, &full_prompt),
        _ => run_opencode(&config.command, &full_prompt, &config.model, &config.api_key),
    }
}

fn run_opencode(command: &str, prompt: &str, model: &str, api_key: &str) -> Result<String, EngineError> {
    let cmd = if command.is_empty() { "opencode" } else { command };

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

    let output = proc.output()
        .map_err(|e| EngineError::SpawnFailed(format!("{cmd}: {e}")))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(EngineError::ProcessFailed(stderr.to_string()));
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

fn run_claude_code(command: &str, prompt: &str, model: &str, api_key: &str) -> Result<String, EngineError> {
    let cmd = if command.is_empty() { "claude" } else { command };

    let mut proc = Command::new(cmd);
    proc.args(["--print", prompt]);
    if !model.is_empty() {
        proc.args(["--model", model]);
    }
    if !api_key.is_empty() {
        proc.env("ANTHROPIC_API_KEY", api_key);
    }

    let output = proc.output()
        .map_err(|e| EngineError::SpawnFailed(format!("{cmd}: {e}")))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(EngineError::ProcessFailed(stderr.to_string()));
    }

    let stdout = String::from_utf8_lossy(&output.stdout);
    Ok(strip_code_fences(stdout.trim()))
}

fn run_copilot(command: &str, prompt: &str) -> Result<String, EngineError> {
    let cmd = if command.is_empty() { "copilot" } else { command };

    let output = Command::new(cmd)
        .args(["-p", prompt, "--output-format", "text", "--silent"])
        .output()
        .map_err(|e| EngineError::SpawnFailed(format!("{cmd}: {e}")))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(EngineError::ProcessFailed(stderr.to_string()));
    }

    let stdout = String::from_utf8_lossy(&output.stdout);
    Ok(strip_code_fences(stdout.trim()))
}

fn run_custom(command: &str, prompt: &str) -> Result<String, EngineError> {
    if command.is_empty() {
        return Err(EngineError::SpawnFailed("No custom command configured".to_string()));
    }

    let output = Command::new("sh")
        .args(["-c", &format!("{command} {}", shell_escape(prompt))])
        .output()
        .map_err(|e| EngineError::SpawnFailed(e.to_string()))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(EngineError::ProcessFailed(stderr.to_string()));
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
        let without_opening = trimmed
            .strip_prefix("```")
            .unwrap_or(trimmed);
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

#[derive(Debug)]
pub enum EngineError {
    SpawnFailed(String),
    ProcessFailed(String),
}

impl std::fmt::Display for EngineError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::SpawnFailed(e) => write!(f, "Failed to launch backend: {e}"),
            Self::ProcessFailed(e) => write!(f, "Backend error: {e}"),
        }
    }
}

impl From<EngineError> for io::Error {
    fn from(e: EngineError) -> Self {
        io::Error::new(io::ErrorKind::Other, e.to_string())
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
}
