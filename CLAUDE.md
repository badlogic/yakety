- YOU, CLAUDE, CAN NOT TEST THE PROGRAMS, ASK THE USER TO TEST AND REPORT RESULTS

# Code Style Guidelines

- Use **C-style C++**: Write C++ code but with C-style conventions
- Prefer C-style casts, function declarations, and naming conventions
- Use C++ features (std::vector, extern "C") only when required for whisper.cpp integration
- Keep the core application logic in C-style for simplicity and readability
- File extensions: .cpp for files that need C++ features, .c for pure C files

- ALWAYS USE rg NOT GREP

- Always use the CMake debug preset to build

- On Windows, CMake now automatically detects Visual Studio 2022 using the VS generator
  No need to set up environment variables!