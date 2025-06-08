# Build whisper.cpp as external project
include(ExternalProject)

function(build_whisper_cpp)
    set(WHISPER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/whisper.cpp")
    set(WHISPER_BUILD_DIR "${WHISPER_SOURCE_DIR}/build")
    
    # Clone whisper.cpp if it doesn't exist
    if(NOT EXISTS "${WHISPER_SOURCE_DIR}/CMakeLists.txt")
        message(STATUS "Cloning whisper.cpp...")
        execute_process(
            COMMAND git clone https://github.com/ggerganov/whisper.cpp.git "${WHISPER_SOURCE_DIR}"
            RESULT_VARIABLE result
        )
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "Failed to clone whisper.cpp")
        endif()
    endif()
    
    # Configure build options based on platform
    set(WHISPER_CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DBUILD_SHARED_LIBS=OFF
        -DWHISPER_BUILD_TESTS=OFF
        -DWHISPER_BUILD_EXAMPLES=OFF
        -DWHISPER_BUILD_SERVER=OFF
    )
    
    if(APPLE)
        list(APPEND WHISPER_CMAKE_ARGS
            -DGGML_METAL=ON
        )
        # Use the same OSX architectures as the main project
        if(CMAKE_OSX_ARCHITECTURES)
            string(REPLACE ";" "\\;" ESCAPED_ARCHITECTURES "${CMAKE_OSX_ARCHITECTURES}")
            list(APPEND WHISPER_CMAKE_ARGS
                "-DCMAKE_OSX_ARCHITECTURES=${ESCAPED_ARCHITECTURES}"
                -DGGML_NATIVE=OFF
            )
        endif()
    elseif(WIN32)
        # Check for Vulkan
        if(DEFINED ENV{VULKAN_SDK})
            list(APPEND WHISPER_CMAKE_ARGS -DGGML_VULKAN=ON)
            message(STATUS "Vulkan SDK found, enabling GPU acceleration for whisper.cpp")
        endif()
    endif()
    
    # Build whisper.cpp if libraries don't exist
    set(WHISPER_LIBS_EXIST TRUE)
    if(WIN32)
        set(LIB_SUFFIX "Release/")
    else()
        set(LIB_SUFFIX "")
    endif()
    
    if(NOT EXISTS "${WHISPER_BUILD_DIR}/src/${LIB_SUFFIX}libwhisper.a" AND
       NOT EXISTS "${WHISPER_BUILD_DIR}/src/${LIB_SUFFIX}whisper.lib")
        set(WHISPER_LIBS_EXIST FALSE)
    endif()
    
    if(NOT WHISPER_LIBS_EXIST)
        message(STATUS "Building whisper.cpp...")
        
        # Create build directory
        file(MAKE_DIRECTORY ${WHISPER_BUILD_DIR})
        
        # Configure
        execute_process(
            COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" ${WHISPER_CMAKE_ARGS} ..
            WORKING_DIRECTORY ${WHISPER_BUILD_DIR}
            RESULT_VARIABLE result
        )
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "Failed to configure whisper.cpp")
        endif()
        
        # Build
        execute_process(
            COMMAND ${CMAKE_COMMAND} --build . --config Release --parallel
            WORKING_DIRECTORY ${WHISPER_BUILD_DIR}
            RESULT_VARIABLE result
        )
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "Failed to build whisper.cpp")
        endif()
        
        message(STATUS "whisper.cpp built successfully")
    else()
        message(STATUS "whisper.cpp already built")
    endif()
endfunction()

# Download whisper model
function(download_whisper_model)
    set(MODEL_DIR "${CMAKE_SOURCE_DIR}/whisper.cpp/models")
    set(MODEL_FILE "${MODEL_DIR}/ggml-base.en.bin")
    set(MODEL_URL "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin")
    
    if(NOT EXISTS ${MODEL_FILE})
        message(STATUS "Downloading whisper base.en model (~150MB)...")
        file(MAKE_DIRECTORY ${MODEL_DIR})
        
        file(DOWNLOAD
            ${MODEL_URL}
            ${MODEL_FILE}
            SHOW_PROGRESS
            STATUS download_status
            TIMEOUT 300  # 5 minutes timeout
        )
        
        list(GET download_status 0 status_code)
        if(NOT status_code EQUAL 0)
            file(REMOVE ${MODEL_FILE})
            message(FATAL_ERROR "Failed to download whisper model")
        endif()
        
        message(STATUS "Whisper model downloaded successfully")
    else()
        message(STATUS "Whisper model already exists")
    endif()
endfunction()