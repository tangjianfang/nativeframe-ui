# NativeFrame UI Testing and Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete Phase 7 by documenting the automated/manual validation matrix and adding a no-BCG/no-MFC boundary check to CTest.

**Architecture:** Keep hardening checks lightweight and repository-local. CTest should continue to run the compiled smoke executable and also run a PowerShell boundary check that scans source content for forbidden dependency markers.

**Tech Stack:** CMake, CTest, PowerShell, Markdown.

---

## Tasks

1. Add `tools/verify_boundaries.ps1` that fails if implementation files contain BCG, MFC, ATL, or WTL dependency markers outside documentation/source planning text.
2. Register `NativeFrameUIBoundaryCheck` in CTest.
3. Add `docs/VALIDATION_MATRIX.md` covering automated tests and manual Windows 10/11, DPI, theme, Chinese/English resource, mouse/keyboard workflows.
4. Run full Debug/Release validation.

## Self-Review

- Spec coverage: covers Phase 7 hardening via CTest boundary checks and validation matrix documentation.
- Placeholder scan: no unresolved placeholders remain.
- Type consistency: new test uses existing CTest path.
