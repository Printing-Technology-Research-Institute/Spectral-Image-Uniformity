if(WX_WIDGETS_DIR)
    target_include_directories(${TARGET} PRIVATE
        ${WX_WIDGETS_DIR}/include
        ${WX_WIDGETS_DIR}/lib/vc_lib/mswu$<$<CONFIG:Debug>:d>
    )
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WX_WIDGETS_LIB_DIR ${WX_WIDGETS_DIR}/lib/vc_x64_lib)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(WX_WIDGETS_LIB_DIR ${WX_WIDGETS_DIR}/lib/vc_lib)
    else()
        message(FATAL_ERROR "Help me if you can, I'm feeling down / And I do appreciate you being 'round / Help me get my feet back on the ground / Won't you please, please help me?")
    endif()
elseif(wxWidgets_FOUND)
    target_include_directories(${TARGET} PRIVATE ${wxWidgets_INCLUDE_DIRS})
    set(WX_WIDGETS_LIB_DIR ${wxWidgets_LIBRARY_DIRS})
    include(${wxWidgets_USE_FILE})
else()
    message(FATAL_ERROR "Either 'WX_WIDGETS_DIR' must be defined in your environment OR the same compiler as used to compile the Windows version of wxWidgets in the 'Toolkits' must be used OR wxWidgets must be installed on this system in a way the cmake package finder can locate it!")
endif()