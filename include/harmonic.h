#ifndef HARMONIC_H
#define HARMONIC_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum HarmonicStatus {
    HARMONIC_STATUS_OK = 0,
    HARMONIC_STATUS_INVALID_ARGUMENT = 1,
    HARMONIC_STATUS_INVALID_BACKEND = 2,
    HARMONIC_STATUS_INVALID_CONFIG = 3,
    HARMONIC_STATUS_SPAWN_FAILED = 4,
    HARMONIC_STATUS_BACKEND_FAILED = 5
} HarmonicStatus;

typedef struct HarmonicResult {
    int status;
    char *output;
    char *error;
} HarmonicResult;

/* Generate code from a natural language prompt using the configured backend.
 * Returns a structured result with status code, generated output, and error text. */
HarmonicResult harmonic_generate_result(const char *prompt, const char *context,
                                        const char *backend, const char *command,
                                        const char *model, const char *api_key,
                                        bool send_context);

/* Free any allocated strings in a HarmonicResult. */
void harmonic_free_result(HarmonicResult result);

/* Generate code from a natural language prompt using the configured backend.
 * prompt: the natural language description (required, non-null).
 * context: surrounding code for better results (may be NULL).
 * backend: "copilot", "opencode", "claude-code", or "custom" (may be NULL for default).
 * command: path to the backend command (may be NULL for default).
 * model: model name to use (may be NULL).
 * api_key: API key override (may be NULL to use environment).
 * send_context: whether to include context in the prompt.
 * Returns generated code on success, or an error string prefixed with "// Error: " on failure.
 * Prefer harmonic_generate_result() for structured status handling. */
char *harmonic_generate(const char *prompt, const char *context,
                        const char *backend, const char *command,
                        const char *model, const char *api_key,
                        bool send_context);

/* Free a string returned by harmonic functions. */
void harmonic_free_string(char *ptr);

/* Get the library version string.
 * Returns a newly allocated string that must be freed with harmonic_free_string(). */
char *harmonic_version();

#ifdef __cplusplus
}
#endif

#endif /* HARMONIC_H */
