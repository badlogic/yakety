cmake_minimum_required(VERSION 3.20)

if(APPLE)
    project(yakety VERSION 1.0.0 LANGUAGES C CXX Swift)
else()
    project(yakety VERSION 1.0.0 LANGUAGES C CXX)
endif()

# C/C++ standards
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include helper modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(GenerateIcons)
include(BuildWhisper)
include(PlatformSetup)

# Platform-specific setup
if(APPLE)
    # Build for ARM64 only (Apple Silicon)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build for Apple Silicon" FORCE)
    # Set minimum macOS version for modern APIs and Swift 6 compatibility
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

# Generate icons if needed
generate_icons()

# Build whisper.cpp
build_whisper_cpp()

# Download whisper model
download_whisper_model()

# Setup platform libraries
setup_platform_libs()

# Check if whisper.cpp is available
set(WHISPER_DIR "${CMAKE_SOURCE_DIR}/whisper.cpp")
set(WHISPER_BUILD_DIR "${WHISPER_DIR}/build")

if(NOT EXISTS "${WHISPER_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "whisper.cpp not found. Build failed.")
endif()

# Create platform library
add_library(platform STATIC)

# Platform library sources
if(APPLE)
    target_sources(platform PRIVATE
        src/mac/logging.m
        src/mac/clipboard.m
        src/mac/overlay.m
        src/mac/dialog.m
        src/mac/dialogs/dialog_utils.swift
        src/mac/dialogs/hotkey_dialog.swift
        src/mac/dialogs/models_dialog.swift
        src/mac/dialogs/download_dialog.swift
        src/mac/menu.m
        src/mac/keylogger.c
        src/mac/app.m
        src/mac/utils.m
        src/mac/dispatch.m
        src/mac/http.m
        src/preferences.c
    )
    target_link_libraries(platform PUBLIC ${PLATFORM_FRAMEWORKS})
elseif(WIN32)
    target_sources(platform PRIVATE
        src/windows/logging.c
        src/windows/clipboard.c
        src/windows/overlay.c
        src/windows/dialog.c
        src/windows/dialog_keycapture.c
        src/windows/menu.c
        src/windows/keylogger.c
        src/windows/app.c
        src/windows/utils.c
        src/preferences.c
    )
    target_link_libraries(platform PUBLIC ${PLATFORM_LIBS})
    if(HAS_VULKAN)
        target_link_libraries(platform PUBLIC ${VULKAN_LIB})
    endif()
endif()

# Include directories for platform library
target_include_directories(platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Configure Swift compiler flags to disable incremental compilation warnings
if(APPLE)
    # Force override of default Swift debug flags to remove -incremental
    set(CMAKE_Swift_FLAGS_DEBUG "-Onone -g -disable-incremental-imports" CACHE STRING "Swift Debug flags" FORCE)
    set(CMAKE_Swift_FLAGS_RELEASE "${CMAKE_Swift_FLAGS_RELEASE} -disable-incremental-imports" CACHE STRING "Swift Release flags" FORCE)
endif()

# Add Swift generated header include path for Xcode builds
if(APPLE AND CMAKE_GENERATOR STREQUAL "Xcode")
    target_include_directories(platform PRIVATE 
        "$<$<CONFIG:Debug>:${CMAKE_BINARY_DIR}/platform.build/Debug/DerivedSources>"
        "$<$<CONFIG:Release>:${CMAKE_BINARY_DIR}/platform.build/Release/DerivedSources>"
    )
endif()

# Business logic sources
set(BUSINESS_SOURCES
    src/audio.c
    src/transcription.cpp
    src/menu.c
    src/models.c
)

# Create yakety CLI executable
add_executable(yakety-cli
    src/main.c
    ${BUSINESS_SOURCES}
)
target_link_libraries(yakety-cli PRIVATE platform)
target_include_directories(yakety-cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Add Windows resource file to CLI if it exists
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/windows/yakety.rc")
    target_sources(yakety-cli PRIVATE src/windows/yakety.rc)
endif()

# Create yakety-app executable
if(WIN32)
    # Windows GUI app (no console window)
    add_executable(yakety-app WIN32
        src/main.c
        ${BUSINESS_SOURCES}
    )

    # Set output name to Yakety
    set_target_properties(yakety-app PROPERTIES OUTPUT_NAME "Yakety")

    # Add Windows resource file if it exists
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/windows/yakety.rc")
        target_sources(yakety-app PRIVATE src/windows/yakety.rc)
    endif()
elseif(APPLE)
    # macOS app bundle
    add_executable(yakety-app MACOSX_BUNDLE
        src/main.c
        ${BUSINESS_SOURCES}
    )

    # Set bundle properties
    set_target_properties(yakety-app PROPERTIES
        OUTPUT_NAME "Yakety"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/src/mac/Info.plist
        MACOSX_BUNDLE_BUNDLE_NAME "Yakety"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.yakety.app"
    )

    # Copy resources to bundle
    set(ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/yakety.icns")
    set(MENUBAR_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/generated/menubar.png")
    set(MENUBAR_ICON_2X "${CMAKE_CURRENT_SOURCE_DIR}/assets/generated/menubar@2x.png")
    set(WHISPER_MODEL "${CMAKE_CURRENT_SOURCE_DIR}/whisper.cpp/models/ggml-base-q8_0.bin")

    if(EXISTS ${ICON_FILE})
        set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(yakety-app PRIVATE ${ICON_FILE})
    endif()

    if(EXISTS ${MENUBAR_ICON})
        set_source_files_properties(${MENUBAR_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(yakety-app PRIVATE ${MENUBAR_ICON})
    endif()

    if(EXISTS ${MENUBAR_ICON_2X})
        set_source_files_properties(${MENUBAR_ICON_2X} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(yakety-app PRIVATE ${MENUBAR_ICON_2X})
    endif()

    # Copy Whisper model to bundle
    if(EXISTS ${WHISPER_MODEL})
        set_source_files_properties(${WHISPER_MODEL} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/models")
        target_sources(yakety-app PRIVATE ${WHISPER_MODEL})
    endif()

    # Code sign the app
    setup_code_signing(yakety-app)
else()
    add_executable(yakety-app
        src/main.c
        ${BUSINESS_SOURCES}
    )
endif()

target_link_libraries(yakety-app PRIVATE platform)
target_include_directories(yakety-app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(yakety-app PRIVATE YAKETY_TRAY_APP)

# Create recorder executable
add_executable(recorder src/recorder.c src/audio.c)
target_include_directories(recorder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Create transcribe executable
add_executable(transcribe src/transcribe.c ${BUSINESS_SOURCES})
target_link_libraries(transcribe PRIVATE platform)
target_include_directories(transcribe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Link frameworks for recorder
if(APPLE)
    target_link_libraries(recorder platform ${PLATFORM_FRAMEWORKS})
elseif(WIN32)
    target_link_libraries(recorder platform ${PLATFORM_LIBS})
endif()

# Find and link whisper.cpp libraries
if(WIN32 AND NOT CMAKE_GENERATOR MATCHES "Ninja")
    # Visual Studio generator puts libraries in Release/Debug subdirs
    set(WHISPER_LIB_DIR "${WHISPER_BUILD_DIR}/src/Release")
    set(GGML_LIB_DIR "${WHISPER_BUILD_DIR}/ggml/src/Release")
else()
    set(WHISPER_LIB_DIR "${WHISPER_BUILD_DIR}/src")
    set(GGML_LIB_DIR "${WHISPER_BUILD_DIR}/ggml/src")
endif()

find_library(WHISPER_LIBRARY NAMES whisper PATHS ${WHISPER_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(GGML_LIBRARY NAMES ggml PATHS ${GGML_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(GGML_BASE_LIBRARY NAMES ggml-base PATHS ${GGML_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
find_library(GGML_CPU_LIBRARY NAMES ggml-cpu PATHS ${GGML_LIB_DIR} NO_DEFAULT_PATH REQUIRED)

set(WHISPER_LIBS ${WHISPER_LIBRARY} ${GGML_LIBRARY} ${GGML_CPU_LIBRARY} ${GGML_BASE_LIBRARY})

# Platform-specific whisper libraries
if(APPLE)
    find_library(GGML_METAL_LIBRARY NAMES ggml-metal PATHS "${GGML_LIB_DIR}/ggml-metal" NO_DEFAULT_PATH)
    if(GGML_METAL_LIBRARY)
        list(APPEND WHISPER_LIBS ${GGML_METAL_LIBRARY})
    endif()
    find_library(GGML_BLAS_LIBRARY NAMES ggml-blas PATHS "${GGML_LIB_DIR}/ggml-blas" NO_DEFAULT_PATH)
    if(GGML_BLAS_LIBRARY)
        list(APPEND WHISPER_LIBS ${GGML_BLAS_LIBRARY})
    endif()
elseif(WIN32 AND HAS_VULKAN)
    if(NOT CMAKE_GENERATOR MATCHES "Ninja")
        find_library(GGML_VULKAN_LIBRARY NAMES ggml-vulkan PATHS "${GGML_LIB_DIR}/../ggml-vulkan/Release" NO_DEFAULT_PATH)
    else()
        find_library(GGML_VULKAN_LIBRARY NAMES ggml-vulkan PATHS "${GGML_LIB_DIR}/ggml-vulkan" NO_DEFAULT_PATH)
    endif()
    if(GGML_VULKAN_LIBRARY)
        list(APPEND WHISPER_LIBS ${GGML_VULKAN_LIBRARY})
    endif()
endif()

# Link whisper to all targets that need it
foreach(target yakety-cli yakety-app transcribe)
    target_link_libraries(${target} PRIVATE ${WHISPER_LIBS})
    target_include_directories(${target} PRIVATE
        ${WHISPER_DIR}
        ${WHISPER_DIR}/include
        ${WHISPER_DIR}/ggml/include
    )
    target_compile_definitions(${target} PRIVATE WHISPER_AVAILABLE)
endforeach()

# Link Metal frameworks for macOS
if(APPLE AND METAL_FRAMEWORKS)
    foreach(target yakety-cli yakety-app transcribe)
        target_link_libraries(${target} PRIVATE ${METAL_FRAMEWORKS})
    endforeach()
endif()

# Set output directory
set_target_properties(yakety-cli yakety-app recorder transcribe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
)

# Copy whisper model to output directory for CLI tools
set(MODEL_FILE "${WHISPER_DIR}/models/ggml-base-q8_0.bin")
if(EXISTS ${MODEL_FILE})
    add_custom_command(TARGET yakety-cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:yakety-cli>/models"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MODEL_FILE}" "$<TARGET_FILE_DIR:yakety-cli>/models/ggml-base-q8_0.bin"
        COMMENT "Copying Whisper model to output directory"
    )
endif()

# Copy menubar icon for CLI tools
set(MENUBAR_ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/generated/menubar.png")
if(EXISTS ${MENUBAR_ICON_FILE})
    add_custom_command(TARGET yakety-cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MENUBAR_ICON_FILE}" "$<TARGET_FILE_DIR:yakety-cli>/menubar.png"
        COMMENT "Copying menubar icon for CLI"
    )
    add_custom_command(TARGET transcribe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MENUBAR_ICON_FILE}" "$<TARGET_FILE_DIR:transcribe>/menubar.png"
        COMMENT "Copying menubar icon for transcribe"
    )
endif()

# Compiler flags
if(WIN32)
    # For Windows Debug builds, use Release runtime library to match whisper.cpp
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        foreach(target yakety-cli yakety-app recorder transcribe platform)
            target_compile_options(${target} PRIVATE /MD)
            target_compile_definitions(${target} PRIVATE _ITERATOR_DEBUG_LEVEL=0)
        endforeach()
    endif()
elseif(NOT WIN32)
    # Base warning flags
    set(WARNING_FLAGS
        -Wall
        -Wextra
        -Werror
        -Wno-error=unused-parameter
        -Wno-error=unused-function
    )

    # Debug-specific flags
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(APPEND WARNING_FLAGS
            -g
            -O0
            -fno-omit-frame-pointer
            # Note: Sanitizers disabled due to Swift compatibility issues
            # -fsanitize=address
            # -fsanitize=undefined
        )
        # Note: Sanitizer linking disabled due to Swift compatibility
        # foreach(target yakety-cli yakety-app recorder transcribe)
        #     target_link_options(${target} PRIVATE -fsanitize=address -fsanitize=undefined)
        # endforeach()
    endif()

    # Apply flags to all targets for C/C++ only
    foreach(target yakety-cli yakety-app recorder transcribe)
        target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:C>:${WARNING_FLAGS}>)
        target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${WARNING_FLAGS}>)
    endforeach()
endif()

# Print build summary
message(STATUS "")
message(STATUS "=== Yakety Build Configuration ===")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Whisper.cpp: ${WHISPER_DIR}")
if(APPLE)
    message(STATUS "Metal acceleration: ${GGML_METAL_LIBRARY}")
elseif(WIN32 AND HAS_VULKAN)
    message(STATUS "Vulkan acceleration: ${GGML_VULKAN_LIBRARY}")
endif()
message(STATUS "Output directory: ${CMAKE_BINARY_DIR}/bin")
message(STATUS "=================================")
message(STATUS "")


# Distribution packaging targets
if(APPLE)
    # CLI distribution for macOS
    add_custom_target(package-cli-macos
        DEPENDS yakety-cli recorder transcribe
        COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/yakety-cli-macos.zip
        COMMAND ${CMAKE_COMMAND} -E tar c ${CMAKE_BINARY_DIR}/yakety-cli-macos.zip --format=zip
            ${CMAKE_BINARY_DIR}/bin/yakety-cli
            ${CMAKE_BINARY_DIR}/bin/models/
            ${CMAKE_BINARY_DIR}/bin/menubar.png
            ${CMAKE_BINARY_DIR}/bin/recorder
            ${CMAKE_BINARY_DIR}/bin/transcribe
        COMMAND ${CMAKE_COMMAND} -E echo "✅ Created yakety-cli-macos.zip"
        COMMENT "Creating yakety CLI distribution for macOS..."
        VERBATIM
    )

    # App distribution for macOS
    add_custom_target(package-app-macos
        DEPENDS yakety-app
        COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/Yakety-macos.dmg
        COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/Yakety-macos.zip
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/dmg_temp
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dmg_temp

        # Copy app bundle to temp directory
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            $<TARGET_BUNDLE_DIR:yakety-app>
            ${CMAKE_BINARY_DIR}/dmg_temp/Yakety.app

        # Create Applications symlink
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            /Applications
            ${CMAKE_BINARY_DIR}/dmg_temp/Applications

        # Create DMG
        COMMAND hdiutil create
            -volname "Yakety"
            -srcfolder ${CMAKE_BINARY_DIR}/dmg_temp
            -ov
            -format UDZO
            ${CMAKE_BINARY_DIR}/Yakety-macos.dmg

        # Create ZIP of the DMG for additional compression
        COMMAND ${CMAKE_COMMAND} -E tar c ${CMAKE_BINARY_DIR}/Yakety-macos.zip --format=zip
            ${CMAKE_BINARY_DIR}/Yakety-macos.dmg

        # Clean up temp directory
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/dmg_temp

        COMMAND ${CMAKE_COMMAND} -E echo "✅ Created Yakety-macos.dmg and Yakety-macos.zip"
        COMMENT "Creating Yakety app distribution for macOS..."
        VERBATIM
    )

    # Combined package target for macOS
    add_custom_target(package-macos
        DEPENDS package-cli-macos package-app-macos
        COMMENT "Creating all macOS distribution packages..."
    )

elseif(WIN32)
    # CLI distribution for Windows
    add_custom_target(package-cli-windows
        DEPENDS yakety-cli recorder transcribe
        COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/yakety-cli-windows.zip
        COMMAND ${CMAKE_COMMAND} -E tar c ${CMAKE_BINARY_DIR}/yakety-cli-windows.zip --format=zip
            ${CMAKE_BINARY_DIR}/bin/yakety-cli.exe
            ${CMAKE_BINARY_DIR}/bin/models/
            ${CMAKE_BINARY_DIR}/bin/menubar.png
            ${CMAKE_BINARY_DIR}/bin/recorder.exe
            ${CMAKE_BINARY_DIR}/bin/transcribe.exe
        COMMAND ${CMAKE_COMMAND} -E echo "✅ Created yakety-cli-windows.zip"
        COMMENT "Creating yakety CLI distribution for Windows..."
        VERBATIM
    )

    # App distribution for Windows
    add_custom_target(package-app-windows
        DEPENDS yakety-app
        COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/Yakety-windows.zip
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/Yakety
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/Yakety
        
        # Copy files to Yakety directory
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/bin/Yakety.exe ${CMAKE_BINARY_DIR}/Yakety/
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/bin/models ${CMAKE_BINARY_DIR}/Yakety/models
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/bin/menubar.png ${CMAKE_BINARY_DIR}/Yakety/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/bin/recorder.exe ${CMAKE_BINARY_DIR}/Yakety/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/bin/transcribe.exe ${CMAKE_BINARY_DIR}/Yakety/
        
        # Create zip with Yakety folder
        COMMAND ${CMAKE_COMMAND} -E tar c ${CMAKE_BINARY_DIR}/Yakety-windows.zip --format=zip Yakety
        
        # Clean up temp directory
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/Yakety
        
        COMMAND ${CMAKE_COMMAND} -E echo "✅ Created Yakety-windows.zip"
        COMMENT "Creating Yakety app distribution for Windows..."
        VERBATIM
    )

    # Combined package target for Windows
    add_custom_target(package-windows
        DEPENDS package-cli-windows package-app-windows
        COMMENT "Creating all Windows distribution packages..."
    )
endif()

# Universal package target
add_custom_target(package
    DEPENDS $<IF:$<PLATFORM_ID:Darwin>,package-macos,package-windows>
    COMMENT "Creating all distribution packages for current platform..."
)

# Upload target
if(WIN32)
    # Create a batch file for the upload command to avoid escaping issues
    file(WRITE ${CMAKE_BINARY_DIR}/upload.bat
        "@echo off\n"
        "for %%f in (\"${CMAKE_BINARY_DIR}\\*.zip\") do (\n"
        "    echo Uploading %%f...\n"
        "    scp \"%%f\" badlogic@slayer.marioslab.io:/home/badlogic/mariozechner.at/html/uploads/\n"
        ")\n"
    )
    
    add_custom_target(upload
        DEPENDS package
        COMMAND cmd /c ${CMAKE_BINARY_DIR}/upload.bat
        COMMAND ${CMAKE_COMMAND} -E echo "✅ Uploaded distribution packages to server"
        COMMENT "Uploading distribution packages to server using scp..."
        VERBATIM
    )
else()
    add_custom_target(upload
        DEPENDS package
        COMMAND sh -c "rsync -avz --progress ${CMAKE_BINARY_DIR}/*.zip badlogic@slayer.marioslab.io:/home/badlogic/mariozechner.at/html/uploads/"
        COMMAND ${CMAKE_COMMAND} -E echo "✅ Uploaded distribution packages to server"
        COMMENT "Uploading distribution packages to server using rsync..."
        VERBATIM
    )
endif()

# Test programs
if(APPLE)
    add_executable(test-model-dialog src/tests/test_model_dialog.c)
    target_link_libraries(test-model-dialog platform)
    target_link_libraries(test-model-dialog "-framework Cocoa")
    set_target_properties(test-model-dialog PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
    )
    
    
    # Key combination dialog test
    add_executable(test-keycombination-dialog src/tests/test_keycombination_dialog.c)
    target_link_libraries(test-keycombination-dialog platform)
    target_link_libraries(test-keycombination-dialog "-framework Cocoa" "-framework SwiftUI")
    set_target_properties(test-keycombination-dialog PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
    )
    
    # Download dialog test
    add_executable(test-download-dialog src/tests/test_download_dialog.c)
    target_link_libraries(test-download-dialog platform)
    target_link_libraries(test-download-dialog "-framework Cocoa" "-framework SwiftUI")
    set_target_properties(test-download-dialog PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
    )
elseif(WIN32 AND HAS_WINUI3)
    # WinUI3 Hello World test (only if WinUI3 is available)
    add_executable(test-winui3-hello WIN32 src/tests/test_winui3_hello.cpp)
    
    # Set C++20 standard for WinRT/WinUI3 support
    target_compile_features(test-winui3-hello PRIVATE cxx_std_20)
    
    # Add WinUI3 and WinRT includes
    target_include_directories(test-winui3-hello PRIVATE 
        ${WINUI3_INCLUDE_DIR}
        "$ENV{WindowsSdkDir}Include/$ENV{WindowsSDKVersion}um"
        "$ENV{WindowsSdkDir}Include/$ENV{WindowsSDKVersion}shared"
    )
    
    # Link WinUI3 and WinRT libraries
    target_link_libraries(test-winui3-hello 
        platform
        windowsapp
    )
    
    # Set subsystem to Windows for GUI app
    set_target_properties(test-winui3-hello PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
        WIN32_EXECUTABLE TRUE
    )
    
    # Add preprocessor definitions for WinUI3
    target_compile_definitions(test-winui3-hello PRIVATE
        WINRT_LEAN_AND_MEAN
        NOMINMAX
        _UNICODE
        UNICODE
        WIN32_LEAN_AND_MEAN
    )
    
    message(STATUS "WinUI3 test target created: test-winui3-hello")
elseif(WIN32)
    message(STATUS "WinUI3 not available - skipping WinUI3 test target")
endif()

message(STATUS "Package targets available: package, package-cli-${CMAKE_SYSTEM_NAME}, package-app-${CMAKE_SYSTEM_NAME}")
message(STATUS "Upload target available: upload (packages and uploads to server)")