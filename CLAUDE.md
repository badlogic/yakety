- YOU, CLAUDE, CAN NOT TEST THE PROGRAMS, ASK THE USER TO TEST AND REPORT RESULTS

# Code Style Guidelines

- Use **C-style C++**: Write C++ code but with C-style conventions
- Prefer C-style casts, function declarations, and naming conventions
- Use C++ features (std::vector, extern "C") only when required for whisper.cpp integration
- Keep the core application logic in C-style for simplicity and readability
- File extensions: .cpp for files that need C++ features, .c for pure C files

- ALWAYS USE rg NOT GREP

# CMake Build Presets

- **Normal development**: Use `cmake --preset release` or `cmake --preset debug` 
  - Uses Ninja generator on all platforms for fast builds
- **Windows debugging**: Use `cmake --preset vs-debug` when you need Visual Studio for debugging
  - Uses Visual Studio 2022 generator with proper debugging support
  - Only available on Windows