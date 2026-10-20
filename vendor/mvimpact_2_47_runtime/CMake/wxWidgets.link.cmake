if(WX_WIDGETS_DIR)
    target_link_libraries(${TARGET} PRIVATE
        ${WX_WIDGETS_LIB_DIR}/wxbase30u$<$<CONFIG:Debug>:d>.lib
        ${WX_WIDGETS_LIB_DIR}/wxbase30u$<$<CONFIG:Debug>:d>_xml.lib
        ${WX_WIDGETS_LIB_DIR}/wxmsw30u$<$<CONFIG:Debug>:d>_adv.lib
        ${WX_WIDGETS_LIB_DIR}/wxmsw30u$<$<CONFIG:Debug>:d>_core.lib
        ${WX_WIDGETS_LIB_DIR}/wxmsw30u$<$<CONFIG:Debug>:d>_webview.lib
        ${WX_WIDGETS_LIB_DIR}/wxpng$<$<CONFIG:Debug>:d>.lib
        ${WX_WIDGETS_LIB_DIR}/wxzlib$<$<CONFIG:Debug>:d>.lib
    )
elseif(wxWidgets_FOUND)
    target_link_libraries(${TARGET} PRIVATE ${wxWidgets_LIBRARIES})
else()
    message(FATAL_ERROR "Either 'WX_WIDGETS_DIR' must be defined in your environment OR the same compiler as used to compile the Windows version of wxWidgets in the 'Toolkits' must be used OR wxWidgets must be installed on this system in a way the cmake package finder can locate it!")
endif()