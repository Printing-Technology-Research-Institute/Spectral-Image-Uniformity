if(WIN32)
    if(wxWidgets_FOUND)
        find_file(WX_MANIFEST_FILE "wx.manifest" ${wxWidgets_INCLUDE_DIRS})
    endif()
    if(NOT WX_MANIFEST_FILE) # When using a pre-built version 'WX_MANIFEST_FILE' might already be defined!
        message(FATAL_ERROR "Failed to locate wx.manifest in ${wxWidgets_INCLUDE_DIRS}!")
    endif()
    add_custom_command(TARGET ${TARGET}
        POST_BUILD
        COMMAND "mt.exe" -manifest \"${WX_MANIFEST_FILE}\" -outputresource:\"$(TargetPath)\"\;\#1
        COMMENT "Adding wxWidgets manifest...")
    add_custom_command(TARGET ${TARGET}
        POST_BUILD
        COMMAND "mt.exe" -manifest \"${CMAKE_CURRENT_SOURCE_DIR}\\${TARGET}.exe.manifest\" -outputresource:\"$(TargetPath)\"\;\#1
        COMMENT "Adding application specific manifest...")
endif()