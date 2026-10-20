if(MSVC AND (NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")) # todo clang and openmp?
    target_compile_options(${TARGET} PRIVATE $<$<NOT:$<CONFIG:Debug>>:/openmp>)
elseif(OPENMP_FOUND)
    if(NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"))
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
    endif()
endif()
