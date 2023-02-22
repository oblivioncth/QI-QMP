#================= Project Setup ==========================

# CMake
cmake_minimum_required(VERSION 3.24.0)

# Store minimum required version for later since it can be overwritten
# by find_package/macro calls in dependencies (stupid)
set(TRUE_CMAKE_MINIMUM_REQUIRED ${CMAKE_MINIMUM_REQUIRED_VERSION})

# Project
# NOTE: DON'T USE TRAILING ZEROS IN VERSIONS
project(QI-QMP
    VERSION 0.2
    LANGUAGES CXX
    DESCRIPTION "Qt-based Interface for QEMU Machine Protocol"
)
string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LC)
string(REPLACE "-" "_" PROJECT_NAME_LC "${PROJECT_NAME_LC}")
string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UC)
string(REPLACE "-" "_" PROJECT_NAME_UC "${PROJECT_NAME_UC}")

# Configuration options
option(QI_QMP_DOCS "Build QI-QMP documentation" OFF)

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
fetch_ob_cmake("8e20cc55aefba251d3436b6e16db71caadd0393e")

# Include utility functions
include(OB/Utility)

# Architecture
get_system_architecture(TARGET_ARCH)

#--------------Top Level Project Setup-------------
include(OB/ProjectSetup)
ob_top_level_project_setup()

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
ob_setup_verbose_versioning(PROJECT_VERSION_VERBOSE)

#================= Build =========================

# Qt package components
set(QT_COMPONENTS
    Core
    Network
)

# Disable deprecated code
add_compile_definitions(QT_DISABLE_DEPRECATED_BEFORE=0x060000)

# Find Qt package
include(OB/BetterFindQt6)
ob_find_qt6_package(REQUIRED COMPONENTS ${QT_COMPONENTS})

# Fetch Qx (build and import from source)
include(OB/FetchQx)
ob_fetch_qx(
    REF "504b1c29e67523fcc8077f3000c73686c0401450"
    COMPONENTS
        Core
)

# Process Targets
set(LIB_TARGET_NAME ${PROJECT_NAME_LC}_qmpi)
set(LIB_ALIAS_NAME ${PROJECT_NAME}::Qmpi)
set(LIB_EXPORT_NAME Qmpi)
set(LIB_INCLUDE_ROOT_NAME qi-qmp)
set(LIB_TARGET_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib)
add_subdirectory(lib)

if(QI_QMP_DOCS)
    # Requires higher CMake version
    if("${CMAKE_VERSION}" VERSION_LESS "3.25.0")
        message(FATAL_ERROR "CMake 3.25.0 or greater is required to build the ${PROJECT_NAME} documentation!")
    endif()

    set(DOC_TARGET_NAME ${PROJECT_NAME_LC}_docs)
    add_subdirectory(doc)
endif()

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

#================= Install ==========================

# Install README and LICENSE
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    COMPONENT ${PROJECT_NAME_LC}
    DESTINATION .
    ${SUB_PROJ_EXCLUDE_FROM_ALL} # "EXCLUDE_FROM_ALL" if project is not top-level
)

# Install Package Config
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/${PROJECT_NAME}Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/${PROJECT_NAME}ConfigVersion.cmake"
    COMPONENT ${PROJECT_NAME_LC}
    DESTINATION cmake
    ${SUB_PROJ_EXCLUDE_FROM_ALL} # "EXCLUDE_FROM_ALL" if project is not top-level
)

#====================== CPack ==============================

set(CPACK_PACKAGE_VENDOR "oblivioncth")
set(CPACK_PACKAGE_DIRECTORY "${PACKAGE_PREFIX}")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}_${PROJECT_VERSION_VERBOSE}_(Qt${Qt6_VERSION}-${Qt6_LINKAGE})_${CMAKE_SYSTEM_NAME}_${TARGET_ARCH}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
include(CPack)
