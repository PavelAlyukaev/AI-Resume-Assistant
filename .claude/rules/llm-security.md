---
paths:
  - "src/llm/**"
---
# LLM module security rules
- Never include real API keys, resumes, emails, names or phone numbers in test fixtures, HTTP logs, or prompts.
- Use HTTPS and asynchronous Qt Network requests; do not block the UI thread.
- Model responses are untrusted: validate JSON and reject unconfirmed resume claims.
- Require a redacted payload and explicit user consent before any remote API call.
- Provide timeouts, cancellation, bounded retries, and redacted errors.
- For changes in `src/llm/**`, explicitly describe how the above constraints are enforced.
