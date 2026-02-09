# Platform-specific setup and checks

function(setup_platform_libs)
    if(APPLE)
        # Find required frameworks for macOS
        find_library(APPLICATION_SERVICES_FRAMEWORK ApplicationServices REQUIRED)
        find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
        find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
        find_library(COREAUDIO_FRAMEWORK CoreAudio REQUIRED)
        find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox REQUIRED)
        find_library(AVFOUNDATION_FRAMEWORK AVFoundation REQUIRED)
        find_library(ACCELERATE_FRAMEWORK Accelerate REQUIRED)
        find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
        find_library(SERVICEMANAGEMENT_FRAMEWORK ServiceManagement REQUIRED)
        
        # Metal frameworks for GPU acceleration
        find_library(METAL_FRAMEWORK Metal)
        find_library(METALKIT_FRAMEWORK MetalKit)
        find_library(METALPERFORMANCE_FRAMEWORK MetalPerformanceShaders)
        
        set(PLATFORM_FRAMEWORKS
            ${APPLICATION_SERVICES_FRAMEWORK}
            ${APPKIT_FRAMEWORK}
            ${COREFOUNDATION_FRAMEWORK}
            ${FOUNDATION_FRAMEWORK}
            ${SERVICEMANAGEMENT_FRAMEWORK}
            ${COREAUDIO_FRAMEWORK}
            ${AUDIOTOOLBOX_FRAMEWORK}
            ${AVFOUNDATION_FRAMEWORK}
            ${ACCELERATE_FRAMEWORK}
            PARENT_SCOPE
        )
        
        if(METAL_FRAMEWORK AND METALKIT_FRAMEWORK AND METALPERFORMANCE_FRAMEWORK)
            set(METAL_FRAMEWORKS
                ${METAL_FRAMEWORK}
                ${METALKIT_FRAMEWORK}
                ${METALPERFORMANCE_FRAMEWORK}
                PARENT_SCOPE
            )
            message(STATUS "Metal frameworks found for GPU acceleration")
        endif()
        
    elseif(WIN32)
        # Windows libraries
        set(PLATFORM_LIBS winmm ole32 user32 shell32 gdi32 PARENT_SCOPE)
        
        # Check for Vulkan SDK
        if(DEFINED ENV{VULKAN_SDK})
            set(VULKAN_SDK_PATH $ENV{VULKAN_SDK})
            find_library(VULKAN_LIBRARY 
                NAMES vulkan-1 vulkan
                PATHS ${VULKAN_SDK_PATH}/Lib
                PATH_SUFFIXES "" "x64"
            )
            if(VULKAN_LIBRARY)
                message(STATUS "Found Vulkan library: ${VULKAN_LIBRARY}")
                set(VULKAN_LIB ${VULKAN_LIBRARY} PARENT_SCOPE)
                set(HAS_VULKAN TRUE PARENT_SCOPE)
            endif()
        endif()
        
    elseif(UNIX)
        # Linux libraries
        find_package(Threads REQUIRED)
        set(_PLATFORM_LIBS ${CMAKE_THREAD_LIBS_INIT} dl m)
        
        # Check for ALSA
        find_package(ALSA)
        if(ALSA_FOUND)
            list(APPEND _PLATFORM_LIBS ${ALSA_LIBRARIES})
        endif()
        
        # Check for PulseAudio and libevdev
        find_package(PkgConfig)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(PULSEAUDIO libpulse)
            if(PULSEAUDIO_FOUND)
                list(APPEND _PLATFORM_LIBS ${PULSEAUDIO_LIBRARIES})
            endif()
            
            pkg_check_modules(LIBEVDEV libevdev)
            if(LIBEVDEV_FOUND)
                list(APPEND _PLATFORM_LIBS ${LIBEVDEV_LIBRARIES})
                set(LIBEVDEV_INCLUDE_DIRS ${LIBEVDEV_INCLUDE_DIRS} PARENT_SCOPE)
                message(STATUS "Found libevdev: ${LIBEVDEV_LIBRARIES}")
            else()
                message(WARNING "libevdev not found - keylogger will not work")
            endif()
        endif()
        
        # Check for Vulkan
        find_package(Vulkan)
        if(Vulkan_FOUND)
            message(STATUS "Found Vulkan: ${Vulkan_LIBRARIES}")
            set(VULKAN_LIB ${Vulkan_LIBRARIES} PARENT_SCOPE)
            set(HAS_VULKAN TRUE PARENT_SCOPE)
        endif()
        
        # Export platform libs to parent scope
        set(PLATFORM_LIBS ${_PLATFORM_LIBS} PARENT_SCOPE)
    endif()
endfunction()

function(setup_code_signing target)
    if(APPLE)
        # Only sign with ad-hoc signature if not building for distribution
        # Distribution builds will be properly signed and notarized by build.sh
        if(NOT SKIP_ADHOC_SIGNING)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:${target}>"
                COMMAND find "$<TARGET_BUNDLE_DIR:${target}>" -exec xattr -c {} \\\;
                COMMENT "Signing ${target} with ad-hoc signature"
            )
        else()
            # Just remove quarantine attribute without re-signing
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND find "$<TARGET_BUNDLE_DIR:${target}>" -exec xattr -c {} \\\;
                COMMENT "Removing quarantine from ${target}"
            )
        endif()
    endif()
endfunction()
