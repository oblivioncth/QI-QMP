# Add library target
qt_add_library(${LIB_TARGET_NAME} STATIC)

# Make alias target so target can be referred to with its friendly
# export name both internally and when part of another build tree
add_library(${LIB_ALIAS_NAME} ALIAS ${LIB_TARGET_NAME})

# Set lib sources
set(CXX_PUBLIC_HEADERS
    qmpi.h
)

set(CXX_IMPLEMENTATION
    qmpi.cpp
)

# Build pathed implementation file list
foreach(impl_file ${CXX_IMPLEMENTATION})
    list(APPEND pathed_impl_files "src/${impl_file}")
endforeach()

# Build pathed include file list
foreach(api_header ${CXX_PUBLIC_HEADERS})
    list(APPEND pathed_api_headers "include/${LIB_INCLUDE_ROOT_NAME}/${api_header}")
endforeach()

# Group include files with their parent directories stripped
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include/${LIB_INCLUDE_ROOT_NAME}"
    PREFIX "Include Files"
    FILES ${pathed_api_headers}
)

# Add sources to target. Add include files as private target source so that they aren't
# built nor marked as a dependency, but are shown with the target in the IDE
target_sources(${LIB_TARGET_NAME}
    PRIVATE
        ${pathed_impl_files}
        ${pathed_api_headers}
)

# Set target include directories so that public headers are propegated to consumers
target_include_directories(${LIB_TARGET_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/${LIB_INCLUDE_ROOT_NAME}>
)

# Link libraries to target
target_link_libraries(${LIB_TARGET_NAME}
    PRIVATE
        Qx::Core
    PUBLIC
        Qt6::Core
        Qt6::Network
)

#---------- Configure Target Properties------------------
set_target_properties(${LIB_TARGET_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    DEBUG_POSTFIX "d"
    EXPORT_NAME "${LIB_EXPORT_NAME}"
)

################# Windows Build #################
if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    set_target_properties(${LIB_TARGET_NAME} PROPERTIES
        OUTPUT_NAME "${PROJECT_NAME}"
    )
endif()


################# Linux Build #################
if(CMAKE_SYSTEM_NAME STREQUAL Linux)
    set_target_properties(${LIB_TARGET_NAME} PROPERTIES
        OUTPUT_NAME "${PROJECT_NAME_LC}"
    )
endif()

#================= Install ==========================

# Install lib
install(TARGETS ${LIB_TARGET_NAME}
    COMPONENT ${LIB_TARGET_NAME}
    EXPORT ${PROJECT_NAME}Targets
    ${SUB_PROJ_EXCLUDE_FROM_ALL} # "EXCLUDE_FROM_ALL" if project is not top-level
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin # For potential future shared version
)

# Install public headers
install(DIRECTORY include/${LIB_INCLUDE_ROOT_NAME}
    COMPONENT ${LIB_TARGET_NAME}
    DESTINATION "include"
    ${SUB_PROJ_EXCLUDE_FROM_ALL} # "EXCLUDE_FROM_ALL" if project is not top-level
)

# Install package target export
install(EXPORT ${PROJECT_NAME}Targets
    COMPONENT ${LIB_TARGET_NAME}
    FILE "${PROJECT_NAME}Targets.cmake"
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION cmake
    ${SUB_PROJ_EXCLUDE_FROM_ALL} # "EXCLUDE_FROM_ALL" if project is not top-level
)
