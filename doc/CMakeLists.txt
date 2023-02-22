#-----------Ensure Doxygen Availability------------

# Find Doxygen package
find_package(Doxygen 1.9.4 REQUIRED
             COMPONENTS dot
)

#--------------------Setup Paths-------------------

# Add local modules
set(DOC_SCRIPTS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Source
set(DOC_RESOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/res")
set(DOC_GENERAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/general")
set(DOC_GENERATED_PATH "${CMAKE_CURRENT_BINARY_DIR}/docin")

# Build
set(DOC_BUILD_PATH "${CMAKE_CURRENT_BINARY_DIR}/doc")

# Cmake scripts
set(DOC_TEMPLATES_PATH "${DOC_SCRIPTS_PATH}/file_templates")
set(DOC_SCRIPTS_PATH "${DOC_SCRIPTS_PATH}/config")

#--------------------Setup Input-------------------

# Configure files
configure_file("${DOC_TEMPLATES_PATH}/doc_mainpage.md.in"
    "${DOC_GENERATED_PATH}/mainpage.md"
    @ONLY
)

# Doc Input
set(DOC_INPUT_LIST
    "${DOC_GENERATED_PATH}/mainpage.md"
    "${LIB_TARGET_PATH}/src"
    "${LIB_TARGET_PATH}/include"
)

set(DOC_EXAMPLE_LIST
    "${DOC_RESOURCE_PATH}/snippets"
)

set(DOC_IMAGE_LIST
    "${DOC_RESOURCE_PATH}/images"
)

# Determine Qt related doc paths
include(OB/QtDocsHelper)
configure_qt_doc_link(${Qt6_PREFIX_PATH})

#--------------------Setup Doxygen-------------------

# Set Doxygen parameters
include(${DOC_SCRIPTS_PATH}/doxyconf.cmake)

# Add Doxygen target
doxygen_add_docs(${DOC_TARGET_NAME}
    ${DOC_INPUT_LIST}
    ${TOP_PROJ_INCLUDE_IN_ALL}
)

# Install Docs
install(DIRECTORY ${DOC_BUILD_PATH}
    COMPONENT ${DOC_TARGET_NAME}
    DESTINATION .
    CONFIGURATIONS Release
    ${SUB_PROJ_EXCLUDE_FROM_ALL} # "EXCLUDE_FROM_ALL" if project is not top-level
)

message(STATUS "Doxygen configured. Build target '${DOC_TARGET_NAME}' to build the documentation.")
