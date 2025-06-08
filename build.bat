@echo off
echo Building Yakety for Windows...

:: Generate Windows icon from SVG if ImageMagick is available
where magick >nul 2>nul
if %errorlevel%==0 (
    echo Generating Windows icon from SVG...
    if not exist "assets\yakety.ico" (
        magick convert assets\yakety.svg -define icon:auto-resize=256,128,64,48,32,16 assets\yakety.ico
        echo Icon generated: assets\yakety.ico
    )
) else (
    echo Note: ImageMagick not found. Using default icon.
)

:: Check if cmake is available
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: CMake not found! Please install CMake and add it to PATH.
    echo Download from: https://cmake.org/download/
    exit /b 1
)

:: Check for Vulkan SDK (optional for GPU acceleration)
set VULKAN_AVAILABLE=0
set ENABLE_VULKAN=0
if defined VULKAN_SDK (
    echo Vulkan SDK found at: %VULKAN_SDK%
    set VULKAN_AVAILABLE=1
    :: Ask user if they want to enable Vulkan
    choice /C YN /T 5 /D N /M "Enable Vulkan GPU acceleration"
    if errorlevel 2 (
        echo Continuing with CPU-only build...
    ) else (
        echo Vulkan acceleration will be enabled.
        set ENABLE_VULKAN=1
    )
) else (
    echo Note: Vulkan SDK not found. Building CPU-only version.
    echo       For GPU acceleration, install Vulkan SDK from:
    echo       https://vulkan.lunarg.com/sdk/home#windows
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
set WHISPER_LIB_EXISTS=0
if exist "whisper.cpp\build\bin\Release\whisper.lib" set WHISPER_LIB_EXISTS=1
if exist "whisper.cpp\build\src\Release\whisper.lib" set WHISPER_LIB_EXISTS=1

if %WHISPER_LIB_EXISTS%==0 (
    echo Building whisper.cpp...
    cd whisper.cpp
    mkdir build 2>nul
    cd build
    
    :: Configure for Windows - with optional Vulkan support
    if %ENABLE_VULKAN%==1 (
        echo Configuring whisper.cpp with Vulkan GPU acceleration (static libraries^)...
        cmake .. -DWHISPER_BUILD_TESTS=OFF -DWHISPER_BUILD_EXAMPLES=OFF -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release -DGGML_VULKAN=ON
    ) else (
        echo Configuring whisper.cpp for CPU-only build (static libraries^)...
        cmake .. -DWHISPER_BUILD_TESTS=OFF -DWHISPER_BUILD_EXAMPLES=OFF -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release
    )
    if %errorlevel% neq 0 (
        echo ERROR: Failed to configure whisper.cpp
        exit /b 1
    )
    
    :: Build whisper
    echo Building whisper.cpp...
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
    echo   Downloading from Hugging Face using curl for better speed...
    
    :: Try curl first (much faster)
    where curl >nul 2>nul
    if %errorlevel%==0 (
        curl -L -o ggml-base.en.bin --progress-bar https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin
    ) else (
        :: Fallback to PowerShell with progress preference disabled for speed
        echo   curl not found, using PowerShell (this may be slower)...
        powershell -Command "$ProgressPreference = 'SilentlyContinue'; Invoke-WebRequest -Uri 'https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin' -OutFile 'ggml-base.en.bin'"
    )
    
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
echo Executables (statically linked, no DLLs required):
echo   - build\bin\yakety.exe     (CLI version - run as administrator)
echo   - build\bin\yakety-app.exe (GUI version with system tray - run as administrator)
echo   - build\bin\recorder.exe   (Audio recorder utility)
echo.
echo Note: Administrator privileges are required for keyboard monitoring.
echo.