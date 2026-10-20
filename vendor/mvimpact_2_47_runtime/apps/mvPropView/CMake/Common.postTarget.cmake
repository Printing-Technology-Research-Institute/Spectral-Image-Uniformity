set_target_properties(${TARGET} PROPERTIES FOLDER "Applications(GUI)/wxPropView")

include(../../../../CMake/wxWidgets.includes.cmake)
include(../../../../CMake/wxWidgets.compile.cmake)

if(WIN32)
    target_link_libraries(${TARGET} PRIVATE comctl32.lib rpcrt4.lib winhttp.lib Ws2_32.lib)
    if(NOT wxWidgets_FOUND)
        target_link_libraries(${TARGET} PRIVATE
            ${WX_WIDGETS_LIB_DIR}/wxbase30u$<$<CONFIG:Debug>:d>_net.lib
            ${WX_WIDGETS_LIB_DIR}/wxjpeg$<$<CONFIG:Debug>:d>.lib
            ${WX_WIDGETS_LIB_DIR}/wxmsw30u$<$<CONFIG:Debug>:d>_propgrid.lib
            ${WX_WIDGETS_LIB_DIR}/wxregexu$<$<CONFIG:Debug>:d>.lib
            ${WX_WIDGETS_LIB_DIR}/wxtiff$<$<CONFIG:Debug>:d>.lib
        )
    endif()
endif()
include(../../../../CMake/wxWidgets.link.cmake)
target_link_libraries(${TARGET} PRIVATE mvIMPACT_Acquire::mvDeviceManager)