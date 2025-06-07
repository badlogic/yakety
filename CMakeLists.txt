cmake_minimum_required(VERSION 3.20)
project(yakety)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform-specific settings
if(APPLE)
    # Find required frameworks for macOS
    find_library(APPLICATION_SERVICES_FRAMEWORK ApplicationServices)
    find_library(APPKIT_FRAMEWORK AppKit)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    find_library(COREAUDIO_FRAMEWORK CoreAudio)
    find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox)
    find_library(AVFOUNDATION_FRAMEWORK AVFoundation)
    find_library(ACCELERATE_FRAMEWORK Accelerate)
    find_library(FOUNDATION_FRAMEWORK Foundation)
elseif(WIN32)
    # Windows libraries for miniaudio and system features
    set(PLATFORM_LIBS winmm ole32 user32 shell32 gdi32)
elseif(UNIX)
    # Linux libraries for miniaudio
    find_package(Threads REQUIRED)
    set(PLATFORM_LIBS ${CMAKE_THREAD_LIBS_INIT} dl m)
    
    # Check for ALSA
    find_package(ALSA)
    if(ALSA_FOUND)
        list(APPEND PLATFORM_LIBS ${ALSA_LIBRARIES})
    endif()
    
    # Check for PulseAudio
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(PULSEAUDIO libpulse)
        if(PULSEAUDIO_FOUND)
            list(APPEND PLATFORM_LIBS ${PULSEAUDIO_LIBRARIES})
        endif()
    endif()
endif()

if(APPLE)
    if(NOT APPLICATION_SERVICES_FRAMEWORK)
        message(FATAL_ERROR "ApplicationServices framework not found")
    endif()

    if(NOT APPKIT_FRAMEWORK)
        message(FATAL_ERROR "AppKit framework not found")
    endif()

    if(NOT COREFOUNDATION_FRAMEWORK)
        message(FATAL_ERROR "CoreFoundation framework not found")
    endif()
    
    if(NOT COREAUDIO_FRAMEWORK)
        message(FATAL_ERROR "CoreAudio framework not found")
    endif()
endif()

# Check if whisper.cpp exists
set(WHISPER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/whisper.cpp")
if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # For cross-compilation to Windows, use a different build directory
    set(WHISPER_BUILD_DIR "${WHISPER_DIR}/build-windows")
else()
    set(WHISPER_BUILD_DIR "${WHISPER_DIR}/build")
endif()

# Check for appropriate library file based on platform
if(WIN32 OR (CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_NAME STREQUAL "Windows"))
    set(WHISPER_LIB_FILE "${WHISPER_BUILD_DIR}/src/whisper.lib")
    if(NOT EXISTS "${WHISPER_LIB_FILE}")
        set(WHISPER_LIB_FILE "${WHISPER_BUILD_DIR}/src/libwhisper.a")
    endif()
else()
    set(WHISPER_LIB_FILE "${WHISPER_BUILD_DIR}/src/libwhisper.a")
endif()

if(EXISTS "${WHISPER_LIB_FILE}")
    message(STATUS "Found whisper.cpp library at ${WHISPER_LIB_FILE}")
    set(WHISPER_AVAILABLE TRUE)
else()
    if(CMAKE_CROSSCOMPILING)
        message(WARNING "whisper.cpp library not found for cross-compilation. Building without transcription support.")
        set(WHISPER_AVAILABLE FALSE)
    else()
        message(WARNING "whisper.cpp library not found. Run ./build.sh to build whisper.cpp first.")
        set(WHISPER_AVAILABLE FALSE)
    endif()
endif()

# Common audio sources
set(AUDIO_SOURCES src/audio.c)
if(APPLE)
    list(APPEND AUDIO_SOURCES src/mac/audio_permissions.m)
elseif(WIN32)
    # Windows doesn't need special audio permissions
endif()

# Source files for yakety CLI
if(APPLE)
    set(YAKETY_SOURCES
        src/mac/main.m
        src/keylogger.c
        ${AUDIO_SOURCES}
        src/mac/overlay.m
    )
elseif(WIN32)
    set(YAKETY_SOURCES
        src/windows/main.c
        src/windows/keylogger.c
        ${AUDIO_SOURCES}
        src/windows/overlay.c
        src/windows/clipboard.c
    )
endif()

# Add transcription sources
list(APPEND YAKETY_SOURCES src/transcription.cpp)

# Source files for yakety app
if(APPLE)
    set(YAKETY_APP_SOURCES
        src/mac/main_app.m
        src/keylogger.c
        ${AUDIO_SOURCES}
        src/mac/overlay.m
        src/mac/menubar.m
    )
elseif(WIN32)
    set(YAKETY_APP_SOURCES
        src/windows/main_app.c
        src/windows/keylogger.c
        ${AUDIO_SOURCES}
        src/windows/overlay.c
        src/windows/clipboard.c
        src/windows/menubar.c
    )
endif()

# Add transcription sources
list(APPEND YAKETY_APP_SOURCES src/transcription.cpp)

# Source files for recorder
set(RECORDER_SOURCES
    src/recorder.c
    ${AUDIO_SOURCES}
)

# Create the yakety CLI executable
add_executable(yakety ${YAKETY_SOURCES})
target_include_directories(yakety PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Create the yakety app executable
if(WIN32)
    # Windows GUI app (no console window)
    add_executable(yakety-app WIN32 ${YAKETY_APP_SOURCES})
elseif(APPLE)
    # macOS app bundle
    add_executable(yakety-app MACOSX_BUNDLE ${YAKETY_APP_SOURCES})
    
    # Set bundle properties
    set_target_properties(yakety-app PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
        MACOSX_BUNDLE_BUNDLE_NAME "Yakety"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    )
    
    # Copy resources to bundle
    set(ICON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/yakety.icns")
    set(MENUBAR_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/generated/menubar.png")
    set(MENUBAR_ICON_2X "${CMAKE_CURRENT_SOURCE_DIR}/assets/generated/menubar@2x.png")
    set(WHISPER_MODEL "${CMAKE_CURRENT_SOURCE_DIR}/whisper.cpp/models/ggml-base.en.bin")
    
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
        message(STATUS "Including Whisper model in app bundle")
    else()
        message(WARNING "Whisper model not found at ${WHISPER_MODEL}")
    endif()
else()
    add_executable(yakety-app ${YAKETY_APP_SOURCES})
endif()
target_include_directories(yakety-app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Create the recorder executable
add_executable(recorder ${RECORDER_SOURCES})
target_include_directories(recorder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Create the test transcription executable
add_executable(test_transcription src/test_transcription.c)
target_include_directories(test_transcription PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(WHISPER_AVAILABLE)
    target_sources(test_transcription PRIVATE src/transcription.cpp)
    target_include_directories(test_transcription PRIVATE ${WHISPER_DIR})
    target_compile_definitions(test_transcription PRIVATE WHISPER_AVAILABLE)
endif()

# Link frameworks for yakety CLI
if(APPLE)
    target_link_libraries(yakety
        ${APPLICATION_SERVICES_FRAMEWORK}
        ${APPKIT_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
        ${COREAUDIO_FRAMEWORK}
        ${AUDIOTOOLBOX_FRAMEWORK}
        ${AVFOUNDATION_FRAMEWORK}
    )
else()
    target_link_libraries(yakety ${PLATFORM_LIBS})
endif()

# Link frameworks for yakety app
if(APPLE)
    target_link_libraries(yakety-app
        ${APPLICATION_SERVICES_FRAMEWORK}
        ${APPKIT_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
        ${COREAUDIO_FRAMEWORK}
        ${AUDIOTOOLBOX_FRAMEWORK}
        ${AVFOUNDATION_FRAMEWORK}
    )
else()
    target_link_libraries(yakety-app ${PLATFORM_LIBS})
endif()

# Link whisper.cpp - required for app to work (except when cross-compiling)
if(NOT WHISPER_AVAILABLE AND NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "whisper.cpp library not found. Run ./build.sh to build whisper.cpp first.")
endif()

# Add whisper.cpp only if available
if(WHISPER_AVAILABLE)
    # Force static libraries for whisper.cpp
    set(BUILD_SHARED_LIBS OFF)
    set(WHISPER_BUILD_TESTS OFF)
    set(WHISPER_BUILD_EXAMPLES OFF)
    
    # Add whisper.cpp build directory to find the whisper target
    add_subdirectory(${WHISPER_DIR} ${WHISPER_BUILD_DIR} EXCLUDE_FROM_ALL)
    
    target_include_directories(yakety PRIVATE ${WHISPER_DIR})
    target_link_libraries(yakety whisper)
    
    target_include_directories(yakety-app PRIVATE ${WHISPER_DIR})
    target_link_libraries(yakety-app whisper)
    
    # Add compile definition to enable whisper code
    target_compile_definitions(yakety PRIVATE WHISPER_AVAILABLE)
    target_compile_definitions(yakety-app PRIVATE WHISPER_AVAILABLE)
else()
    message(WARNING "Building without whisper.cpp support")
endif()

# Link Metal framework for GPU acceleration (macOS only)
if(APPLE AND NOT CMAKE_CROSSCOMPILING)
    find_library(METAL_FRAMEWORK Metal)
    find_library(METALKIT_FRAMEWORK MetalKit)
    find_library(METALPERFORMANCE_FRAMEWORK MetalPerformanceShaders)
    
    if(METAL_FRAMEWORK AND METALKIT_FRAMEWORK AND METALPERFORMANCE_FRAMEWORK AND WHISPER_AVAILABLE)
        target_link_libraries(yakety
            ${METAL_FRAMEWORK}
            ${METALKIT_FRAMEWORK}
            ${METALPERFORMANCE_FRAMEWORK}
        )
        target_link_libraries(yakety-app
            ${METAL_FRAMEWORK}
            ${METALKIT_FRAMEWORK}
            ${METALPERFORMANCE_FRAMEWORK}
        )
        message(STATUS "Linked Metal frameworks for GPU acceleration")
    endif()
endif()

# Link frameworks for recorder
if(APPLE)
    target_link_libraries(recorder
        ${APPKIT_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
        ${COREAUDIO_FRAMEWORK}
        ${AUDIOTOOLBOX_FRAMEWORK}
        ${AVFOUNDATION_FRAMEWORK}
    )
else()
    target_link_libraries(recorder ${PLATFORM_LIBS})
endif()

# Link frameworks for test_transcription
if(APPLE)
    target_link_libraries(test_transcription
        ${COREFOUNDATION_FRAMEWORK}
    )
endif()

# Link whisper.cpp to test_transcription if available
if(WHISPER_AVAILABLE)
    target_link_libraries(test_transcription whisper)
    
    # Link Metal frameworks for test_transcription
    if(METAL_FRAMEWORK AND METALKIT_FRAMEWORK AND METALPERFORMANCE_FRAMEWORK)
        target_link_libraries(test_transcription
            ${METAL_FRAMEWORK}
            ${METALKIT_FRAMEWORK}
            ${METALPERFORMANCE_FRAMEWORK}
        )
    endif()
endif()

# Set output directory for all executables
set_target_properties(yakety yakety-app recorder test_transcription PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Compiler flags for macOS
target_compile_options(yakety PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wno-error=unused-parameter
    -Wno-error=unused-function
)

target_compile_options(yakety-app PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wno-error=unused-parameter
    -Wno-error=unused-function
)

target_compile_options(recorder PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wno-error=unused-parameter
    -Wno-error=unused-function
)