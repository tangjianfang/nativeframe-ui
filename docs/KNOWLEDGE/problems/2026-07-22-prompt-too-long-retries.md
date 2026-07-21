---
title: Sub-agent prompt size limits — 2 retries in one cycle
date: 2026-07-22
tags: [subagent, prompt-size, retry]
status: mitigated
---

## Problem
Two sub-agent dispatches failed with "Prompt is too long":
- T10 (TreeView split) — original prompt was ~9KB
- P1.1 (Button disabled) — original prompt was ~7KB

The model API has an undocumented but enforced upper limit on prompt
size; complex multi-file tasks with full code templates exceed it.

## Resolution
Retried both with leaner prompts (~2-3KB) that pointed at the
specific files rather than inlining all reference content. The
shorter prompts succeeded in both cases.

## Mitigation
- For sub-agent tasks that exceed ~5KB of instructions, split into
  multiple agents OR pre-write the changes to a scratch file the
  agent can reference rather than receiving them inline.
- Always prefer "read these files yourself, then make these specific
  edits" over "here is the full content of every file you need".
- Pre-flight check: if the prompt is longer than ~6KB, condense.
