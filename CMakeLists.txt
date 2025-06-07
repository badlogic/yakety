cmake_minimum_required(VERSION 3.20)
project(whisperer)

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
    # Windows libraries for miniaudio
    set(PLATFORM_LIBS winmm ole32)
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
set(WHISPER_BUILD_DIR "${WHISPER_DIR}/build")

if(EXISTS "${WHISPER_BUILD_DIR}/src/libwhisper.a")
    message(STATUS "Found whisper.cpp library at ${WHISPER_BUILD_DIR}/src/libwhisper.a")
    set(WHISPER_AVAILABLE TRUE)
else()
    message(WARNING "whisper.cpp library not found. Run ./build.sh to build whisper.cpp first.")
    set(WHISPER_AVAILABLE FALSE)
endif()

# Common audio sources
set(AUDIO_SOURCES src/audio_miniaudio.c)
if(APPLE)
    list(APPEND AUDIO_SOURCES src/audio_permissions_macos.m)
endif()

# Source files for whisperer CLI
set(WHISPERER_SOURCES
    src/main.m
    src/keylogger.c
    ${AUDIO_SOURCES}
    src/overlay.m
)

# Add transcription sources
list(APPEND WHISPERER_SOURCES src/transcription.cpp)

# Source files for whisperer app
set(WHISPERER_APP_SOURCES
    src/main_app.m
    src/keylogger.c
    ${AUDIO_SOURCES}
    src/overlay.m
    src/menubar.m
)

# Add transcription sources
list(APPEND WHISPERER_APP_SOURCES src/transcription.cpp)

# Source files for recorder
set(RECORDER_SOURCES
    src/recorder.c
    ${AUDIO_SOURCES}
)

# Create the whisperer CLI executable
add_executable(whisperer ${WHISPERER_SOURCES})

# Create the whisperer app executable
add_executable(whisperer-app ${WHISPERER_APP_SOURCES})

# Create the recorder executable
add_executable(recorder ${RECORDER_SOURCES})

# Create the test transcription executable
add_executable(test_transcription src/test_transcription.c)
if(WHISPER_AVAILABLE)
    target_sources(test_transcription PRIVATE src/transcription.cpp)
    target_include_directories(test_transcription PRIVATE ${WHISPER_DIR})
    target_compile_definitions(test_transcription PRIVATE WHISPER_AVAILABLE)
endif()

# Link frameworks for whisperer CLI
if(APPLE)
    target_link_libraries(whisperer
        ${APPLICATION_SERVICES_FRAMEWORK}
        ${APPKIT_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
        ${COREAUDIO_FRAMEWORK}
        ${AUDIOTOOLBOX_FRAMEWORK}
        ${AVFOUNDATION_FRAMEWORK}
    )
else()
    target_link_libraries(whisperer ${PLATFORM_LIBS})
endif()

# Link frameworks for whisperer app
if(APPLE)
    target_link_libraries(whisperer-app
        ${APPLICATION_SERVICES_FRAMEWORK}
        ${APPKIT_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
        ${COREAUDIO_FRAMEWORK}
        ${AUDIOTOOLBOX_FRAMEWORK}
        ${AVFOUNDATION_FRAMEWORK}
    )
else()
    target_link_libraries(whisperer-app ${PLATFORM_LIBS})
endif()

# Link whisper.cpp - required for app to work
if(NOT WHISPER_AVAILABLE)
    message(FATAL_ERROR "whisper.cpp library not found. Run ./build.sh to build whisper.cpp first.")
endif()

# Add whisper.cpp build directory to find the whisper target
add_subdirectory(${WHISPER_DIR} ${WHISPER_BUILD_DIR} EXCLUDE_FROM_ALL)

target_include_directories(whisperer PRIVATE ${WHISPER_DIR})
target_link_libraries(whisperer whisper)

target_include_directories(whisperer-app PRIVATE ${WHISPER_DIR})
target_link_libraries(whisperer-app whisper)

# Link Metal framework for GPU acceleration
find_library(METAL_FRAMEWORK Metal)
find_library(METALKIT_FRAMEWORK MetalKit)
find_library(METALPERFORMANCE_FRAMEWORK MetalPerformanceShaders)

if(METAL_FRAMEWORK AND METALKIT_FRAMEWORK AND METALPERFORMANCE_FRAMEWORK)
    target_link_libraries(whisperer
        ${METAL_FRAMEWORK}
        ${METALKIT_FRAMEWORK}
        ${METALPERFORMANCE_FRAMEWORK}
    )
    target_link_libraries(whisperer-app
        ${METAL_FRAMEWORK}
        ${METALKIT_FRAMEWORK}
        ${METALPERFORMANCE_FRAMEWORK}
    )
    message(STATUS "Linked Metal frameworks for GPU acceleration")
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
target_link_libraries(test_transcription
    ${COREFOUNDATION_FRAMEWORK}
)

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
set_target_properties(whisperer whisperer-app recorder test_transcription PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Compiler flags for macOS
target_compile_options(whisperer PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wno-error=unused-parameter
    -Wno-error=unused-function
)

target_compile_options(whisperer-app PRIVATE
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