set(TARGET TimestampFeatures)

set(SOURCES
    ${TARGET}.cpp
)

add_executable(${TARGET}
    ${SOURCES}
)
set_target_properties(${TARGET} PROPERTIES FOLDER "Examples/C++${CPP_STANDARD_VERSION}")
mv_enable_and_enforce_cpp_standard_version(${TARGET} 11)

target_link_libraries(${TARGET} PRIVATE ${mvIMPACT_Acquire_LIBRARIES})

if(MSVC)
    target_compile_definitions(${TARGET} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# pthread and rt are part of the bionic in Android and don't exist as standalone libraries
if(UNIX AND NOT CMAKE_COMPILE_FOR_ANDROID)
    target_link_libraries(${TARGET} PRIVATE pthread)
endif()

