# Include standard documentation target creator
include(OB/Documentation)

# Setup documentation
ob_standard_documentation(${DOC_TARGET_NAME}
    DOXY_VER 1.9.4
    PROJ_NAME "${PROJECT_NAME}"
    QT_PREFIX "${Qt6_PREFIX_PATH}"
    QT_MODULES
        qtcore
        qtnetwork
    INPUT_LIST
        "${LIB_PATH}/src"
        "${LIB_PATH}/include"
    THEME_COLORS
        PRIMARY "#CC6428"
        NIGHT_PRIMARY "#E06B2C"
)
