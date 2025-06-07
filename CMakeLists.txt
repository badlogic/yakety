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

# Check if whisper.cpp directory exists
if(EXISTS "${WHISPER_DIR}/CMakeLists.txt")
    message(STATUS "Found whisper.cpp at ${WHISPER_DIR}")
    set(WHISPER_AVAILABLE TRUE)
else()
    message(WARNING "whisper.cpp not found. Clone it first or build without transcription support.")
    set(WHISPER_AVAILABLE FALSE)
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

# Ensure C files are compiled as C on Windows
if(WIN32)
    set_source_files_properties(
        src/windows/main.c
        src/windows/keylogger.c
        src/windows/overlay.c
        src/windows/clipboard.c
        src/audio.c
        PROPERTIES LANGUAGE C
    )
endif()

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

# Ensure C files are compiled as C on Windows for app target
if(WIN32)
    set_source_files_properties(
        src/windows/main_app.c
        src/windows/keylogger.c
        src/windows/overlay.c
        src/windows/clipboard.c
        src/windows/menubar.c
        src/audio.c
        PROPERTIES LANGUAGE C
    )
endif()

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
    # Use pre-built static libraries approach for all platforms
    # This avoids issues with whisper.cpp's CMake overriding our settings
    
    # Find the static libraries
    if(WIN32)
        # Windows uses Release/Debug subdirectories
        find_library(WHISPER_LIBRARY
            NAMES whisper
            PATHS ${WHISPER_BUILD_DIR}/src/Release ${WHISPER_BUILD_DIR}/src
            NO_DEFAULT_PATH
        )
        find_library(GGML_LIBRARY
            NAMES ggml
            PATHS ${WHISPER_BUILD_DIR}/ggml/src/Release ${WHISPER_BUILD_DIR}/ggml/src
            NO_DEFAULT_PATH
        )
        find_library(GGML_BASE_LIBRARY
            NAMES ggml-base
            PATHS ${WHISPER_BUILD_DIR}/ggml/src/Release ${WHISPER_BUILD_DIR}/ggml/src
            NO_DEFAULT_PATH
        )
        find_library(GGML_CPU_LIBRARY
            NAMES ggml-cpu
            PATHS ${WHISPER_BUILD_DIR}/ggml/src/Release ${WHISPER_BUILD_DIR}/ggml/src
            NO_DEFAULT_PATH
        )
    else()
        # macOS/Linux don't use Release subdirectory
        find_library(WHISPER_LIBRARY
            NAMES whisper
            PATHS ${WHISPER_BUILD_DIR}/src
            NO_DEFAULT_PATH
        )
        find_library(GGML_LIBRARY
            NAMES ggml
            PATHS ${WHISPER_BUILD_DIR}/ggml/src
            NO_DEFAULT_PATH
        )
        find_library(GGML_BASE_LIBRARY
            NAMES ggml-base
            PATHS ${WHISPER_BUILD_DIR}/ggml/src
            NO_DEFAULT_PATH
        )
        find_library(GGML_CPU_LIBRARY
            NAMES ggml-cpu
            PATHS ${WHISPER_BUILD_DIR}/ggml/src
            NO_DEFAULT_PATH
        )
        
        # On macOS, also look for Metal library
        if(APPLE)
            find_library(GGML_METAL_LIBRARY
                NAMES ggml-metal
                PATHS ${WHISPER_BUILD_DIR}/ggml/src/ggml-metal
                NO_DEFAULT_PATH
            )
        endif()
    endif()
    
    # Create imported targets
    add_library(whisper STATIC IMPORTED)
    set_target_properties(whisper PROPERTIES
        IMPORTED_LOCATION ${WHISPER_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES "${WHISPER_DIR};${WHISPER_DIR}/include"
    )
    
    # Link the libraries
    set(WHISPER_LIBS ${WHISPER_LIBRARY} ${GGML_LIBRARY} ${GGML_CPU_LIBRARY} ${GGML_BASE_LIBRARY})
    if(APPLE AND GGML_METAL_LIBRARY)
        list(APPEND WHISPER_LIBS ${GGML_METAL_LIBRARY})
    endif()
    
    target_link_libraries(yakety ${WHISPER_LIBS})
    target_link_libraries(yakety-app ${WHISPER_LIBS})
    target_include_directories(yakety PRIVATE ${WHISPER_DIR} ${WHISPER_DIR}/include ${WHISPER_DIR}/ggml/include)
    target_include_directories(yakety-app PRIVATE ${WHISPER_DIR} ${WHISPER_DIR}/include ${WHISPER_DIR}/ggml/include)
    
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
    target_link_libraries(test_transcription ${WHISPER_LIBS})
    target_include_directories(test_transcription PRIVATE ${WHISPER_DIR} ${WHISPER_DIR}/include ${WHISPER_DIR}/ggml/include)
    
    # Link Metal frameworks for test_transcription
    if(METAL_FRAMEWORK AND METALKIT_FRAMEWORK AND METALPERFORMANCE_FRAMEWORK)
        target_link_libraries(test_transcription
            ${METAL_FRAMEWORK}
            ${METALKIT_FRAMEWORK}
            ${METALPERFORMANCE_FRAMEWORK}
        )
    endif()
endif()

# Set output directory for all executables and DLLs to the same location
set_target_properties(yakety yakety-app recorder test_transcription PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
)

# Copy whisper model to output directory if it exists
set(MODEL_FILE "${WHISPER_DIR}/models/ggml-base.en.bin")
if(EXISTS ${MODEL_FILE})
    # Create models directory in output
    add_custom_command(TARGET yakety POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:yakety>/models"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MODEL_FILE}" "$<TARGET_FILE_DIR:yakety>/models/ggml-base.en.bin"
        COMMENT "Copying Whisper model to output directory"
    )
    add_custom_command(TARGET yakety-app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:yakety-app>/models"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MODEL_FILE}" "$<TARGET_FILE_DIR:yakety-app>/models/ggml-base.en.bin"
        COMMENT "Copying Whisper model to output directory"
    )
else()
    message(WARNING "Whisper model not found at ${MODEL_FILE}")
endif()

# Compiler flags
if(NOT WIN32)
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
endif()