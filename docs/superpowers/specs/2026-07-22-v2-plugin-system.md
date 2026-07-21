---
title: V2.0 plugin system
date: 2026-07-22
status: living
tags: [v2, plugin, extensibility]
---

## Goal
Allow third-party DLLs to extend a NativeFrameUI app at runtime:
register custom controls, contribute commands, add menu items, dock
panes, and react to app lifecycle events. Inspired by Visual Studio
extensions and Eclipse plug-ins.

## Why deferred from V1
- Plugin discovery requires file system scanning and metadata
  parsing.
- Sandboxing (which APIs are exposed to plugins) requires ABI
  discipline — what does V1 commit to?
- Lifecycle (load/unload/reload) requires careful resource management.

## Architecture
```
Application
  ├── PluginManager
  │     ├── Discovery (scan .nfplug files in /plugins)
  │     ├── Loader (LoadLibraryW + ABI version check)
  │     └── Registry (merged controls, commands, panes)
  └── PluginContext (passed to plugin entry points)
```

## Components
- `nfui::PluginManager` (singleton per app).
- `nfui::PluginContext` (per-plugin handle into app services).
- `nfui::PluginManifest` (.nfplug JSON or INI).
- `nfui::abi::PluginEntry` (extern "C" symbol every DLL exports).

## Open questions
- ABI stability: do we lock the ABI at V2.0 or allow breaking changes?
- Hot-reload: supported in dev mode, forbidden in release?
- Sandboxing: process-isolation via separate worker process, or
  in-process with API restrictions?
