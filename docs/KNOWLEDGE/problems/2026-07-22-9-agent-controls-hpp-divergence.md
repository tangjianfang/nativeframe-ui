---
title: Per-component split: 8/9 agents independently removed Controls.hpp declarations
date: 2026-07-22
tags: [layered-architecture, umbrella, divergence]
status: resolved
---

## Problem
When dispatching 9 parallel sub-agents (T4-T12) to split components
out of `nfui_controls` into per-component libraries, the task spec
instructed each agent to LEAVE the class declaration in the umbrella
`include/nfui/Controls.hpp`. But the spec also provided a
per-component header that re-declares the same class, producing C2011
redefinition errors at build time.

## Resolution
8 of 9 agents independently realized the spec was internally
inconsistent and proactively removed the duplicate declarations from
`Controls.hpp`, replacing them with forwarding `#include`s. T7 went
further and extracted `Controls/Control.hpp` to break the include
cycle entirely. T13 (integration) finalized the design by adopting
T7's extraction + making `Controls.hpp` a pure forwarding umbrella.

## Lesson
When writing TDD-style task specs that span parallel agents, verify
the spec's own example code actually compiles against the other
constraints in the spec. If the spec gives contradictory instructions
("don't touch X" + "add Y that conflicts with X"), agents will
innovate — which can be productive (T7's deeper fix) but also
fragile (T4's most-aggressive removal required re-work during T13).

Future specs should self-test: write the example code in a scratch
file and compile it before dispatching.
