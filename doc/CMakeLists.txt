# Include standard documentation target creator
include(OB/Documentation)

# Set project specific doxygen overrides
set(DOXYGEN_HTML_COLORSTYLE_HUE 22)
set(DOXYGEN_HTML_COLORSTYLE_SAT 255)
set(DOXYGEN_HTML_COLORSTYLE_GAMMA 80)

# Setup documentation
ob_standard_documentation(
    TARGET_NAME "${DOC_TARGET_NAME}"
    DOXY_VER 1.9.4
    PROJ_NAME "${PROJECT_NAME}"
    QT_PREFIX "${Qt6_PREFIX_PATH}"
    QT_MODULES
        qtcore
        qtnetwork
    INPUT_LIST
        "${LIB_PATH}/src"
        "${LIB_PATH}/include"
)
