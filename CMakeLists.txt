#================= Project Setup ==========================

# CMake
cmake_minimum_required(VERSION 3.24.0)

# Project
# NOTE: DON'T USE TRAILING ZEROS IN VERSIONS
project(QI-QMP
    VERSION 0.2
    LANGUAGES CXX
    DESCRIPTION "Qt-based Interface for QEMU Machine Protocol"
)

# Get helper scripts
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchOBCMake.cmake)
fetch_ob_cmake("a98538b14c951fc8c8acc236c35c2cfa3be1821d")

# Initialize project according to standard rules
include(OB/Project)
ob_standard_project_setup()

# Standard setup overrides
set(PROJECT_NAMESPACE_LC "qi_qmp")
set(PROJECT_NAMESPACE_UC "QI_QMP")

# Configuration options
option(QI_QMP_DOCS "Build QI-QMP documentation" OFF)

# C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build augmentation
set(CMAKE_AUTOMOC ON)

#================= Build =========================

# Import Qt
set(QI_QMP_QT_COMPONENTS
    Core
    Network
)

add_compile_definitions(QT_DISABLE_DEPRECATED_BEFORE=0x060000)
include(OB/BetterFindQt6)
ob_find_qt6_package(REQUIRED COMPONENTS ${QI_QMP_QT_COMPONENTS})

# Fetch Qx (build and import from source)
set(QI_QMP_QX_COMPONENTS
    Core
)

include(OB/FetchQx)
ob_fetch_qx(
    REF "992f9171deea65aaecf30af6de94d3dddfdd9a71"
    COMPONENTS
        ${QI_QMP_QX_COMPONENTS}
)

# Process Targets
set(LIB_TARGET_NAME ${PROJECT_NAMESPACE_LC}_qmpi)
set(LIB_ALIAS_NAME Qmpi)
set(LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib")
add_subdirectory("${LIB_PATH}")

if(QI_QMP_DOCS)
    # Requires higher CMake version
    if("${CMAKE_VERSION}" VERSION_LESS "3.25.0")
        message(FATAL_ERROR "CMake 3.25.0 or greater is required to build the ${PROJECT_NAME} documentation!")
    endif()

    set(DOC_TARGET_NAME ${PROJECT_NAMESPACE_LC}_docs)
    add_subdirectory(doc)
endif()

#--------------------Package Config-----------------------

ob_standard_project_package_config(
    COMPATIBILITY "SameMinorVersion"
    CONFIG STANDARD
        TARGET_CONFIGS
            "${PROJECT_NAMESPACE}::${LIB_ALIAS_NAME}"
        DEPENDS
            PACKAGE "Qt6" COMPONENTS ${QI_QMP_QT_COMPONENTS}
            PACKAGE "Qx" VERSION ${Qx_VERSION} COMPONENTS ${QI_QMP_QX_COMPONENTS}
)

#=================+ Top Level Install ========================

ob_standard_project_misc_install()

#====================== Package ==============================

include(OB/Packaging)
ob_standard_project_package(
    VENDOR "oblivioncth"
    SUFFIX "_(Qt${Qt6_VERSION})"
)
