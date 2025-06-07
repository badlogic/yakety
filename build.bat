@echo off
echo Building Yakety for Windows...

:: Check if cmake is available
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: CMake not found! Please install CMake and add it to PATH.
    echo Download from: https://cmake.org/download/
    exit /b 1
)

:: Check if we need to clone whisper.cpp
if not exist "whisper.cpp" (
    echo Cloning whisper.cpp...
    git clone https://github.com/ggerganov/whisper.cpp.git
    if %errorlevel% neq 0 (
        echo ERROR: Failed to clone whisper.cpp
        exit /b 1
    )
)

:: Build whisper.cpp if not already built
if not exist "whisper.cpp\build\Release\whisper.lib" (
    echo Building whisper.cpp...
    cd whisper.cpp
    mkdir build 2>nul
    cd build
    
    :: Configure for Windows with CUDA support if available
    cmake .. -DWHISPER_CUDA=ON -DCMAKE_BUILD_TYPE=Release
    if %errorlevel% neq 0 (
        echo WARNING: CUDA build failed, trying CPU-only build...
        cmake .. -DWHISPER_CUDA=OFF -DCMAKE_BUILD_TYPE=Release
    )
    
    :: Build whisper
    cmake --build . --config Release
    if %errorlevel% neq 0 (
        echo ERROR: Failed to build whisper.cpp
        exit /b 1
    )
    
    cd ..\..
)

:: Download Whisper model if not present
if not exist "whisper.cpp\models\ggml-base.en.bin" (
    echo Downloading Whisper base.en model...
    if not exist "whisper.cpp\models" mkdir "whisper.cpp\models"
    cd whisper.cpp\models
    echo   Downloading from Hugging Face...
    powershell -Command "Invoke-WebRequest -Uri 'https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin' -OutFile 'ggml-base.en.bin'"
    if %errorlevel% neq 0 (
        echo ERROR: Failed to download model
        exit /b 1
    )
    echo Model downloaded successfully
    cd ..\..
)

:: Create build directory
if not exist "build" mkdir build
cd build

:: Configure the project
echo Configuring Yakety...
cmake .. -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed
    exit /b 1
)

:: Build the project
echo Building Yakety...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo ERROR: Build failed
    exit /b 1
)

cd ..

echo.
echo Build complete!
echo.
echo Executables:
echo   - build\Release\yakety.exe     (CLI version - run as administrator)
echo   - build\Release\yakety-app.exe (GUI version with system tray - run as administrator)
echo   - build\Release\recorder.exe      (Audio recorder utility)
echo.
echo Note: Administrator privileges are required for keyboard monitoring.
echo.