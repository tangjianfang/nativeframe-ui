#include <nfui/design_tokens.hpp>

// CP-A1: design tokens are declared as `inline constexpr` in the header so
// each translation unit that includes the header gets a definition and the
// linker folds the duplicates. This .cpp exists so the cmake module target
// has a source file to build and so a future addition that does need a TU
// (e.g. an annotated default-resolution for an optional user-supplied
// override) can land here without rewiring CMakeLists.
