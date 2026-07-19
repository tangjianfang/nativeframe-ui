# Contributing

NativeFrame UI welcomes focused improvements to the pure Win32 C++20 library, samples, tests, and documentation.

## Before Opening a Pull Request

1. Check the roadmap and open issues for scope.
2. Keep dependencies within the documented Win32/Common Controls baseline.
3. Configure and build with a CMake preset.
4. Run CTest in every configuration affected by the change.
5. Update directly related documentation and validation notes.

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Use the same commands with `x64-release` for release-sensitive changes.

## Engineering Rules

- Keep ownership of `HWND`, resources, and callbacks explicit.
- Do not allow exceptions across Windows callback boundaries.
- Keep logical units separate from device pixels.
- Prefer small composable changes over framework-wide rewrites.
- Do not add copied or compatibility-branded code from third-party UI frameworks.

Pull requests should explain behavior changes, validation evidence, and any known limitations. The repository pull request template contains the required checklist.
