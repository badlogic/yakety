cmake_minimum_required(VERSION 3.20)
project(whisperer)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required frameworks
find_library(APPLICATION_SERVICES_FRAMEWORK ApplicationServices)
find_library(AVFOUNDATION_FRAMEWORK AVFoundation)
find_library(APPKIT_FRAMEWORK AppKit)
find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
find_library(ACCELERATE_FRAMEWORK Accelerate)
find_library(FOUNDATION_FRAMEWORK Foundation)

if(NOT APPLICATION_SERVICES_FRAMEWORK)
    message(FATAL_ERROR "ApplicationServices framework not found")
endif()

if(NOT AVFOUNDATION_FRAMEWORK)
    message(FATAL_ERROR "AVFoundation framework not found")
endif()

if(NOT APPKIT_FRAMEWORK)
    message(FATAL_ERROR "AppKit framework not found")
endif()

if(NOT COREFOUNDATION_FRAMEWORK)
    message(FATAL_ERROR "CoreFoundation framework not found")
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

# Source files for whisperer
set(WHISPERER_SOURCES
    src/main.m
    src/keylogger.c
    src/audio.m
    src/overlay.m
    src/menubar.m
)

# Add transcription sources
list(APPEND WHISPERER_SOURCES src/transcription.cpp)

# Source files for recorder
set(RECORDER_SOURCES
    src/recorder.c
    src/audio.m
)

# Create the whisperer executable
add_executable(whisperer ${WHISPERER_SOURCES})

# Create the recorder executable
add_executable(recorder ${RECORDER_SOURCES})

# Create the test transcription executable
add_executable(test_transcription src/test_transcription.c)
if(WHISPER_AVAILABLE)
    target_sources(test_transcription PRIVATE src/transcription.cpp)
    target_include_directories(test_transcription PRIVATE ${WHISPER_DIR})
    target_compile_definitions(test_transcription PRIVATE WHISPER_AVAILABLE)
endif()

# Link frameworks for whisperer
target_link_libraries(whisperer
    ${APPLICATION_SERVICES_FRAMEWORK}
    ${AVFOUNDATION_FRAMEWORK}
    ${APPKIT_FRAMEWORK}
    ${COREFOUNDATION_FRAMEWORK}
)

# Link whisper.cpp - required for app to work
if(NOT WHISPER_AVAILABLE)
    message(FATAL_ERROR "whisper.cpp library not found. Run ./build.sh to build whisper.cpp first.")
endif()

# Add whisper.cpp build directory to find the whisper target
add_subdirectory(${WHISPER_DIR} ${WHISPER_BUILD_DIR} EXCLUDE_FROM_ALL)

target_include_directories(whisperer PRIVATE ${WHISPER_DIR})
target_link_libraries(whisperer whisper)

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
    message(STATUS "Linked Metal frameworks for GPU acceleration")
endif()

# Link frameworks for recorder
target_link_libraries(recorder
    ${AVFOUNDATION_FRAMEWORK}
    ${APPKIT_FRAMEWORK}
    ${COREFOUNDATION_FRAMEWORK}
)

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
set_target_properties(whisperer recorder test_transcription PROPERTIES
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

target_compile_options(recorder PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wno-error=unused-parameter
    -Wno-error=unused-function
)