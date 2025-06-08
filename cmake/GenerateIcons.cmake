# Generate icons from master icon
function(generate_icons)
    set(MASTER_ICON "${CMAKE_SOURCE_DIR}/assets/icon-master.png")
    set(ICON_DIR "${CMAKE_SOURCE_DIR}/assets/generated")
    
    # Check if ImageMagick is available
    find_program(MAGICK_COMMAND magick)
    if(NOT MAGICK_COMMAND)
        find_program(CONVERT_COMMAND convert)
        if(NOT CONVERT_COMMAND)
            message(WARNING "ImageMagick not found. Skipping icon generation.")
            return()
        endif()
        set(MAGICK_COMMAND ${CONVERT_COMMAND})
    endif()
    
    # Create output directory
    file(MAKE_DIRECTORY ${ICON_DIR})
    
    # Generate icons if master exists and any icon is missing
    if(EXISTS ${MASTER_ICON})
        set(NEED_GENERATION FALSE)
        
        # Check if any icon is missing
        if(NOT EXISTS "${ICON_DIR}/icon_16x16.png" OR
           NOT EXISTS "${ICON_DIR}/icon_512x512.png" OR
           NOT EXISTS "${ICON_DIR}/menubar.png")
            set(NEED_GENERATION TRUE)
        endif()
        
        if(NEED_GENERATION)
            message(STATUS "Generating icons from master...")
            
            # Standard icons
            foreach(size 16 32 48 64 128 256 512 1024)
                execute_process(
                    COMMAND ${MAGICK_COMMAND} ${MASTER_ICON} -resize ${size}x${size} 
                            "${ICON_DIR}/icon_${size}x${size}.png"
                    RESULT_VARIABLE result
                )
                if(NOT result EQUAL 0)
                    message(WARNING "Failed to generate ${size}x${size} icon")
                endif()
            endforeach()
            
            # Retina icons
            foreach(size 16 32 64 128 256 512)
                math(EXPR double_size "${size} * 2")
                execute_process(
                    COMMAND ${MAGICK_COMMAND} ${MASTER_ICON} -resize ${double_size}x${double_size}
                            "${ICON_DIR}/icon_${size}x${size}@2x.png"
                    RESULT_VARIABLE result
                )
            endforeach()
            
            # Menubar icons
            execute_process(
                COMMAND ${MAGICK_COMMAND} ${MASTER_ICON} -resize 22x22 "${ICON_DIR}/menubar.png"
            )
            execute_process(
                COMMAND ${MAGICK_COMMAND} ${MASTER_ICON} -resize 44x44 "${ICON_DIR}/menubar@2x.png"
            )
            
            # Generate .icns for macOS
            if(APPLE)
                # Create iconset directory
                set(ICONSET_DIR "${CMAKE_BINARY_DIR}/yakety.iconset")
                file(MAKE_DIRECTORY ${ICONSET_DIR})
                
                # Copy icons to iconset with correct names
                file(COPY "${ICON_DIR}/icon_16x16.png" 
                     DESTINATION ${ICONSET_DIR}
                     FILE_RENAME "icon_16x16.png")
                file(COPY "${ICON_DIR}/icon_16x16@2x.png" 
                     DESTINATION ${ICONSET_DIR}
                     FILE_RENAME "icon_16x16@2x.png")
                file(COPY "${ICON_DIR}/icon_32x32.png" 
                     DESTINATION ${ICONSET_DIR}
                     FILE_RENAME "icon_32x32.png")
                file(COPY "${ICON_DIR}/icon_32x32@2x.png" 
                     DESTINATION ${ICONSET_DIR}
                     FILE_RENAME "icon_32x32@2x.png")
                file(COPY "${ICON_DIR}/icon_64x64.png" 
                     DESTINATION ${ICONSET_DIR}
                     FILE_RENAME "icon_64x64.png")
                file(COPY "${ICON_DIR}/icon_64x64@2x.png" 
                     DESTINATION ${ICONSET_DIR}
                     FILE_RENAME "icon_64x64@2x.png")
                file(COPY "${ICON_DIR}/icon_128x128.png" 
                     DESTINATION ${ICONSET_DIR}
                     FILE_RENAME "icon_128x128.png")
                file(COPY "${ICON_DIR}/icon_128x128@2x.png" 
                     DESTINATION ${ICONSET_DIR}
                     FILE_RENAME "icon_128x128@2x.png")
                file(COPY "${ICON_DIR}/icon_256x256.png" 
                     DESTINATION ${ICONSET_DIR}
                     FILE_RENAME "icon_256x256.png")
                file(COPY "${ICON_DIR}/icon_256x256@2x.png" 
                     DESTINATION ${ICONSET_DIR}
                     FILE_RENAME "icon_256x256@2x.png")
                file(COPY "${ICON_DIR}/icon_512x512.png" 
                     DESTINATION ${ICONSET_DIR}
                     FILE_RENAME "icon_512x512.png")
                file(COPY "${ICON_DIR}/icon_512x512@2x.png" 
                     DESTINATION ${ICONSET_DIR}
                     FILE_RENAME "icon_512x512@2x.png")
                
                # Generate .icns
                execute_process(
                    COMMAND iconutil -c icns -o "${CMAKE_SOURCE_DIR}/assets/yakety.icns" ${ICONSET_DIR}
                    RESULT_VARIABLE result
                )
                if(result EQUAL 0)
                    message(STATUS "Generated yakety.icns")
                else()
                    message(WARNING "Failed to generate .icns file")
                endif()
                
                # Clean up
                file(REMOVE_RECURSE ${ICONSET_DIR})
            endif()
            
            # Generate .ico for Windows
            if(WIN32 OR MINGW)
                execute_process(
                    COMMAND ${MAGICK_COMMAND} ${MASTER_ICON} 
                            -define icon:auto-resize=256,128,64,48,32,16
                            "${CMAKE_SOURCE_DIR}/assets/yakety.ico"
                    RESULT_VARIABLE result
                )
                if(result EQUAL 0)
                    message(STATUS "Generated yakety.ico")
                else()
                    message(WARNING "Failed to generate .ico file")
                endif()
            endif()
            
            message(STATUS "Icon generation complete")
        endif()
    else()
        message(WARNING "Master icon not found at ${MASTER_ICON}")
    endif()
endfunction()