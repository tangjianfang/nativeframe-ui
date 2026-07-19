# Release Checklist

## Scope and API

- [ ] Public headers and namespace remain documented and stable.
- [ ] Breaking changes have migration notes and roadmap approval.
- [ ] No MFC, ATL/WTL, BCG, copied resources, or incompatible branding was added.

## Build and Validation

- [ ] x64 Debug configure, build, and CTest pass.
- [ ] x64 Release configure, build, and CTest pass.
- [ ] Boundary scan passes.
- [ ] Workbench, Showcase, DarkStudio, SettingsDemo, and ResourceGallery start successfully.
- [ ] Resource integration is explicit and verified.
- [ ] Windows 10/11, DPI, theme, keyboard, resource, and screenshot checks are recorded.

## Documentation and Distribution

- [ ] README, product growth roadmap, known limitations, validation matrix, and demo matrix are current.
- [ ] Showcase and per-demo walkthroughs match the released binaries.
- [ ] Any pending manual Windows checks are marked as pending rather than implied complete.
- [ ] License and third-party notices are present.
- [ ] Release notes include known issues and upgrade guidance.
