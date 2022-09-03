#================= Project Setup ==========================

# CMake
cmake_minimum_required(VERSION 3.21.1)

# Store minimum required version for later since it can be overwritten
# by find_package/macro calls in dependencies (stupid)
set(TRUE_CMAKE_MINIMUM_REQUIRED ${CMAKE_MINIMUM_REQUIRED_VERSION})

# Configuration options
option(QI_QMP_DOCS_TARGET "Build QI-QMP documentation" OFF)

# Project
# NOTE: DON'T USE TRAILING ZEROS IN VERSIONS
set(QI_QMP_BASE_VERSION 0.1.2.1) # Required for CI/CD
project(QI-QMP
    VERSION ${QI_QMP_BASE_VERSION}
    LANGUAGES CXX
    DESCRIPTION "Qt-based Interface for QEMU Machine Protocol"
)
set(FORMAL_NAME "QI-QMP")
set(INTERFACE_NAME "Qmpi")
string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LC)
string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UC)
string(TOLOWER ${INTERFACE_NAME} INTERFACE_NAME_LC)

# C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build augmentation
set(CMAKE_AUTOMOC ON)

# Add local modules
set(PROJ_SCRIPTS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${PROJ_SCRIPTS_PATH}/module")

# Get helper scripts
include(QI-QMP/FetchOBCMake)
fetch_ob_cmake("v0.1")

# Include utility functions
include(OB/Utility)

# General Variables
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(TARGET_ARCH x64)
else()
  set(TARGET_ARCH x86)
endif()
get_proper_system_name(PROPER_SYSTEM_NAME)

#--------------Top Level Project Setup-------------

if(${PROJECT_IS_TOP_LEVEL})
    message(STATUS "NOTE: ${PROJECT_NAME} is being configured as a top-level project")

    # Install (override the CMake default, but not a user set value)
    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/out/install"
               CACHE PATH "Project install path" FORCE
        )
    endif()

    # Clean install when clean target is ran
    set_directory_properties(PROPERTIES ADDITIONAL_CLEAN_FILES "${CMAKE_INSTALL_PREFIX}")
else()
    message(STATUS "NOTE: ${PROJECT_NAME} is being configured as a sub-project")

    # Keep install components out of 'all' target
    set(SUB_PROJ_EXCLUDE_FROM_ALL "EXCLUDE_FROM_ALL")
endif()

#--------------------Setup Paths-------------------

# Cmake scripts
set(FILE_TEMPLATES_PATH "${PROJ_SCRIPTS_PATH}/file_templates")
set(DOC_SCRIPTS_PATH "${PROJ_SCRIPTS_PATH}/doc")

# Source
set(DOC_SOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/doc")
set(DOC_GENERATED_PATH "${CMAKE_CURRENT_BINARY_DIR}/docin")

# Build
set(DOC_BUILD_PATH "${CMAKE_CURRENT_BINARY_DIR}/doc")

# Install
set(HEADER_INSTALL_SUFFIX "include")

# Package
set(PACKAGE_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/out/dist")

#------------Set Global Build Rules----------------

# Versioning
include(OB/VerboseVersioning)
setup_verbose_versioning(PROJECT_VERSION_VERBOSE)

#================= Build =========================

# Qt package components
set(QT_FIND_COMPONENTS
    Core
    Network
)

# Disable deprecated code
add_compile_definitions(QT_DISABLE_DEPRECATED_BEFORE=0x060000)

# Find Qt package
include(OB/BetterFindQt6)
find_qt6_package(REQUIRED COMPONENTS ${QT_FIND_COMPONENTS})

# Fetch Qx (build and import from source)
include(OB/FetchQx)
fetch_qx("v0.3.1")

# Set lib sources
set(INCLUDE_HEADERS
    qmpi.h
)

set(IMPLEMENTATION
    src/qmpi.cpp
)

# Name here needs to be as unique as possible for when this project is inlcuded
# in another via FetchContent or add_subdirectory (prevent target clashes)
set(TARGET_NAME ${PROJECT_NAME_LC}_${INTERFACE_NAME_LC})

# Add lib target
qt_add_library(${TARGET_NAME} STATIC)

# Make alias target so target can be referred to with its friendly
# export name both internally and when part of another build tree
add_library(${PROJECT_NAME}::${INTERFACE_NAME} ALIAS ${TARGET_NAME})

# Add sources to target
target_sources(${TARGET_NAME}
    PRIVATE
        ${IMPLEMENTATION}
)

# Add headers to target
target_include_directories(${TARGET_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/${PROJECT_NAME_LC}>
)

# Build pathed include file list
foreach(api_header ${INCLUDE_HEADERS})
    set(pathed_api_headers ${pathed_api_headers} "include/${PROJECT_NAME_LC}/${api_header}")
endforeach()

# Group include files with their parent directories stripped
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME_LC}"
    PREFIX "Include Files"
    FILES ${pathed_api_headers}
)

# Add include files as private target source so that they aren't built nor marked as a dependency,
# but are shown with the target in the IDE
target_sources(${TARGET_NAME} PRIVATE ${pathed_api_headers})

# Link libraries to target
target_link_libraries(${TARGET_NAME}
    PRIVATE
        Qt6::Core
        Qt6::Network
        Qx::Core
)
#---------- Configure Target Properties------------------
set_target_properties(${TARGET_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    OUTPUT_NAME "${PROJECT_NAME}"
    DEBUG_POSTFIX "d"
    EXPORT_NAME "${INTERFACE_NAME}"
)

#--------------------Package Config-----------------------

# Create config file
configure_file("${FILE_TEMPLATES_PATH}/${PROJECT_NAME}Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/${PROJECT_NAME}Config.cmake"
    @ONLY
)

# Create version config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)

#================ Build Documentation ======================

if(QI_QMP_DOCS_TARGET)
    # Find Doxygen package
    find_package(Doxygen 1.9.4 REQUIRED
                 COMPONENTS dot
    )

    # Configure files
    configure_file("${FILE_TEMPLATES_PATH}/doc_mainpage.md.in"
        "${DOC_GENERATED_PATH}/mainpage.md"
        @ONLY
    )

    # Doc Input
    set(DOC_INPUT_LIST
        "${DOC_GENERATED_PATH}/mainpage.md"
        "src"
        "include"
    )

    set(DOC_EXAMPLE_LIST
        "${DOC_SOURCE_PATH}/snippets"
    )

    set(DOC_IMAGE_LIST
        "${DOC_SOURCE_PATH}/images"
    )

    # Determine Qt related doc paths
    include(OB/QtDocsHelper)
    configure_qt_doc_link(${Qt6_PREFIX_PATH})

    # Set Doxygen parameters
    include(${DOC_SCRIPTS_PATH}/doxyconf.cmake)

    # Add Doxygen target
    set(DOC_TARGET_NAME "qi_qmp_docs")
    doxygen_add_docs(${DOC_TARGET_NAME}
        ${DOC_INPUT_LIST}
    )

    message(STATUS "Doxygen configured. Build target '${DOC_TARGET_NAME}' to build the documentation.")
endif()

#================= Install ==========================

set(TOP_LEVEL_INSTALL_COMPONENT ${PROJECT_NAME_LC})

# Install lib
install(TARGETS ${TARGET_NAME}
    COMPONENT ${TARGET_NAME}
    EXPORT ${PROJECT_NAME}Targets
    ${SUB_PROJ_EXCLUDE_FROM_ALL} # "EXCLUDE_FROM_ALL" if project is not top-level
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin # For potential future shared version
)

# Install public headers
install(DIRECTORY include/${PROJECT_NAME_LC}
    COMPONENT ${TARGET_NAME}
    DESTINATION "include"
    ${SUB_PROJ_EXCLUDE_FROM_ALL} # "EXCLUDE_FROM_ALL" if project is not top-level
)

# Install package target export
install(EXPORT ${PROJECT_NAME}Targets
    COMPONENT ${TARGET_NAME}
    FILE "${PROJECT_NAME}Targets.cmake"
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION cmake
    ${SUB_PROJ_EXCLUDE_FROM_ALL} # "EXCLUDE_FROM_ALL" if project is not top-level
)

# Install Package Config
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/${PROJECT_NAME}ConfigVersion.cmake"
    COMPONENT ${TOP_LEVEL_INSTALL_COMPONENT}
    DESTINATION cmake
    ${SUB_PROJ_EXCLUDE_FROM_ALL} # "EXCLUDE_FROM_ALL" if project is not top-level
)

# Install README and LICENSE
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    COMPONENT ${TOP_LEVEL_INSTALL_COMPONENT}
    DESTINATION .
    ${SUB_PROJ_EXCLUDE_FROM_ALL} # "EXCLUDE_FROM_ALL" if project is not top-level
)

# Install Docs if available
install(DIRECTORY ${DOC_BUILD_PATH}
    COMPONENT ${PROJECT_NAME_LC}_docs
    DESTINATION .
    CONFIGURATIONS Release
    OPTIONAL
    ${SUB_PROJ_EXCLUDE_FROM_ALL} # "EXCLUDE_FROM_ALL" if project is not top-level
)

#======== Export For In-tree Builds =================
export(EXPORT ${PROJECT_NAME}Targets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/${PROJECT_NAME}Targets.cmake"
    NAMESPACE ${PROJECT_NAME}::
)

#====================== CPack ==============================

set(CPACK_PACKAGE_VENDOR "oblivioncth")
set(CPACK_PACKAGE_DIRECTORY "${PACKAGE_PREFIX}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}_${PROJECT_VERSION_VERBOSE}_(Qt${Qt6_VERSION}-${Qt6_LINKAGE})_${PROPER_SYSTEM_NAME}_${TARGET_ARCH}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
include(CPack)
