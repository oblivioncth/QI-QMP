#================= Project Setup ==========================

# CMake
cmake_minimum_required(VERSION 3.23.0...3.25.0)

# Project
# NOTE: DON'T USE TRAILING ZEROS IN VERSIONS
project(QI-QMP
    VERSION 0.2.2
    LANGUAGES CXX
    DESCRIPTION "Qt-based Interface for QEMU Machine Protocol"
)

# Get helper scripts
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchOBCMake.cmake)
fetch_ob_cmake("v0.3.2")

# Initialize project according to standard rules
include(OB/Project)
ob_standard_project_setup()

# Standard setup overrides
set(PROJECT_NAMESPACE_LC "qi_qmp")
set(PROJECT_NAMESPACE_UC "QI_QMP")

# Configuration options
option(QI_QMP_DOCS "Build QI-QMP documentation" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries." OFF) # Redundant due to OB, but explicit

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
    REF "v0.5.1"
    COMPONENTS
        ${QI_QMP_QX_COMPONENTS}
)

# Process Targets
set(LIB_TARGET_NAME ${PROJECT_NAMESPACE_LC}_qmpi)
set(LIB_ALIAS_NAME Qmpi)
set(LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib")
add_subdirectory("${LIB_PATH}")

if(QI_QMP_DOCS)
    set(DOC_TARGET_NAME ${PROJECT_NAMESPACE_LC}_docs)
    add_subdirectory(doc)
endif()

#--------------------Package Config-----------------------

ob_standard_project_package_config(
    COMPATIBILITY "SameMinorVersion"
    CONFIG STANDARD
        TARGET_CONFIGS
            TARGET "${PROJECT_NAMESPACE}::${LIB_ALIAS_NAME}" COMPONENT "${LIB_ALIAS_NAME}" DEFAULT
        DEPENDS
            PACKAGE "Qt6" COMPONENTS ${QI_QMP_QT_COMPONENTS}
            PACKAGE "Qx" VERSION ${Qx_VERSION} COMPONENTS ${QI_QMP_QX_COMPONENTS}
)

#=================+ Top Level Install ========================

ob_standard_project_misc_install()

#====================== Package ==============================

include(OB/Packaging)
ob_standard_project_package(VENDOR "oblivioncth")
