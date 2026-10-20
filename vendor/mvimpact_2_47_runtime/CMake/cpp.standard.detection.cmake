# See https://en.cppreference.com/w/cpp/compiler_support#cpp11 for compiler version versus feature tables
#
# todo: There must be a better way to do this! But apparently currently there is NOT! See 
# - https://gitlab.kitware.com/cmake/cmake/issues/18874
# - https://stackoverflow.com/questions/54515364/how-to-detect-c11-support-of-a-compiler-with-cmake-and-react-without-faili
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
        set(CPP_STANDARD_VERSION 17)
    elseif(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 5.0)
        set(CPP_STANDARD_VERSION 14)
    elseif(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 4.8.1)
        # Actually 5.0 is the first one supporting the full feature set, but the stuff we are using so far is working. Only 'Money, Time, and hexfloat I/O manipulators' are missing in this version.
        set(CPP_STANDARD_VERSION 11)
    else()
        set(CPP_STANDARD_VERSION 03)
    endif()
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
        set(CPP_STANDARD_VERSION 17)
    elseif(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 3.8)
        set(CPP_STANDARD_VERSION 14)
    elseif(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 3.3)
        # Actually 3.8 is the first one supporting the full feature set, but the stuff we are using so far is working. Only 'Money, Time, and hexfloat I/O manipulators' are missing in this version.
        set(CPP_STANDARD_VERSION 11)
    else()
        set(CPP_STANDARD_VERSION 03)
    endif()
elseif(MSVC)
    if(MSVC_VERSION GREATER_EQUAL 1915)
        set(CPP_STANDARD_VERSION 17)
    elseif(MSVC_VERSION GREATER_EQUAL 1910)
        set(CPP_STANDARD_VERSION 14)
    elseif(MSVC_VERSION GREATER_EQUAL 1800)
        # todo: Update once we actually use a more modern Visual Studio! Actually 1900 (VS 2015) is the first one supporting the full feature set and even that has bugs
        set(CPP_STANDARD_VERSION 11)
    else()
        set(CPP_STANDARD_VERSION 03)
    endif()
elseif(BORLAND)
    # todo: Borland/Embarcadero claim to support C++11 however the 'thread' header is reported nowhere to be found...
    set(CPP_STANDARD_VERSION 03)
else()
    set(CPP_STANDARD_VERSION 03)
endif()

message(STATUS "C++ standard supported by this compiler: c++${CPP_STANDARD_VERSION}")

#message(STATUS "The following specific C++ features are supported by this compiler:")
#foreach(feature ${CMAKE_CXX_COMPILE_FEATURES})
#    message(STATUS "  ${feature}")
#endforeach()

if(CPP_STANDARD_VERSION GREATER_EQUAL 11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCPP_STANDARD_VERSION=${CPP_STANDARD_VERSION}")
endif()

function(mv_enable_and_enforce_cpp_standard_version TARGET VERSION)
    set_target_properties(${TARGET} PROPERTIES CXX_STANDARD ${VERSION})
    set_target_properties(${TARGET} PROPERTIES CXX_STANDARD_REQUIRED ON)
    message(STATUS "'${TARGET}' will be built using support for c++${VERSION} and would fail to compile with a smaller version of the c++ standard!")
endfunction()

function(mv_enable_and_enforce_cpp_standard_version_if_supported TARGET VERSION)
    if(CPP_STANDARD_VERSION GREATER_EQUAL ${VERSION})
        set_target_properties(${TARGET} PROPERTIES CXX_STANDARD ${VERSION})
        set_target_properties(${TARGET} PROPERTIES CXX_STANDARD_REQUIRED ON)
        message(STATUS "'${TARGET}' will be built using support for c++${VERSION}!")
    else()
        message(STATUS "'${TARGET}' will be built using support for c++${CPP_STANDARD_VERSION} but would use more advanced language features when c++${VERSION} would be supported by the used compiler!")
    endif()
endfunction()
