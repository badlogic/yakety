cmake_minimum_required(VERSION 3.20)
project(yakety VERSION 1.0.0)

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

# Generate icons if needed
generate_icons()

# Build whisper.cpp
build_whisper_cpp()

# Download whisper model
download_whisper_model()

# Platform-specific setup
if(APPLE)
    # Build universal binary for both Intel and Apple Silicon
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build for both architectures" FORCE)
endif()

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
        src/mac/menu.m
        src/mac/keylogger.c
        src/mac/app.m
        src/mac/utils.m
    )
    target_link_libraries(platform PUBLIC ${PLATFORM_FRAMEWORKS})
elseif(WIN32)
    target_sources(platform PRIVATE
        src/windows/logging.c
        src/windows/clipboard.c
        src/windows/overlay.c
        src/windows/dialog.c
        src/windows/menu.c
        src/windows/keylogger.c
        src/windows/app.c
        src/windows/utils.c
    )
    target_link_libraries(platform PUBLIC ${PLATFORM_LIBS})
    if(HAS_VULKAN)
        target_link_libraries(platform PUBLIC ${VULKAN_LIB})
    endif()
endif()

# Include directories for platform library
target_include_directories(platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Business logic sources
set(BUSINESS_SOURCES
    src/audio.c
    src/transcription.cpp
)

# Create yakety CLI executable
add_executable(yakety
    src/main.c
    ${BUSINESS_SOURCES}
)
target_link_libraries(yakety PRIVATE platform)
target_include_directories(yakety PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Create yakety-app executable
if(WIN32)
    # Windows GUI app (no console window)
    add_executable(yakety-app WIN32
        src/main.c
        ${BUSINESS_SOURCES}
    )
    
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

# Create test_transcription executable
add_executable(test_transcription src/test_transcription.c ${BUSINESS_SOURCES})
target_link_libraries(test_transcription PRIVATE platform)
target_include_directories(test_transcription PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Link frameworks for recorder
if(APPLE)
    target_link_libraries(recorder ${PLATFORM_FRAMEWORKS})
elseif(WIN32)
    target_link_libraries(recorder ${PLATFORM_LIBS})
endif()

# Find and link whisper.cpp libraries
if(WIN32)
    # Windows uses Release subdirectory
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
    find_library(GGML_VULKAN_LIBRARY NAMES ggml-vulkan PATHS "${GGML_LIB_DIR}/ggml-vulkan" NO_DEFAULT_PATH)
    if(GGML_VULKAN_LIBRARY)
        list(APPEND WHISPER_LIBS ${GGML_VULKAN_LIBRARY})
    endif()
endif()

# Link whisper to all targets that need it
foreach(target yakety yakety-app test_transcription)
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
    foreach(target yakety yakety-app test_transcription)
        target_link_libraries(${target} PRIVATE ${METAL_FRAMEWORKS})
    endforeach()
endif()

# Set output directory
set_target_properties(yakety yakety-app recorder test_transcription PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
)

# Copy whisper model to output directory for CLI tools
set(MODEL_FILE "${WHISPER_DIR}/models/ggml-base.en.bin")
if(EXISTS ${MODEL_FILE})
    add_custom_command(TARGET yakety POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:yakety>/models"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MODEL_FILE}" "$<TARGET_FILE_DIR:yakety>/models/ggml-base.en.bin"
        COMMENT "Copying Whisper model to output directory"
    )
endif()

# Compiler flags
if(NOT WIN32)
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
            -fsanitize=address
            -fsanitize=undefined
        )
        # Add sanitizer libraries for linking
        foreach(target yakety yakety-app recorder test_transcription)
            target_link_options(${target} PRIVATE -fsanitize=address -fsanitize=undefined)
        endforeach()
    endif()
    
    # Apply flags to all targets
    foreach(target yakety yakety-app recorder test_transcription)
        target_compile_options(${target} PRIVATE ${WARNING_FLAGS})
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