# Knowledge Base

Living repository for everything we learn while building NativeFrameUI.

## Boards

- [`problems/`](problems/) — development pitfalls, surprising behaviors, and fixes
  discovered during implementation. Format: `<YYYY-MM-DD>-<short-slug>.md`.
- [`polish/`](polish/) — backlog of UX/product-detail work needed to take the
  library from "works" to "feels professional". One entry per concern, with
  severity and effort estimates.
- [`competitive/`](competitive/) — comparison entries vs. BCGControlBar Pro,
  MFC, WinForms, WPF, Qt, Dear ImGui, etc. What they do that we don't, what
  we do that they don't, and what we're choosing not to clone.

## Conventions

- One file per entry. Filename: `YYYY-MM-DD-short-slug.md`.
- Each file has YAML frontmatter (`title`, `date`, `tags`, `status`).
- Cross-link liberally with relative paths.
- Entries never delete — when an issue is resolved, add a `## Resolution` section
  with the commit SHA that fixed it; the historical record is the value.
