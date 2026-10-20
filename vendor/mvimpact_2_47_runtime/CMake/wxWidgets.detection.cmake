find_package(wxWidgets 3.0 COMPONENTS net core base adv propgrid webview xml)
if(NOT wxWidgets_FOUND)
    message(STATUS "wxWidgets applications will not be compiled as no compatible wxWidgets version could be detected!")
endif()