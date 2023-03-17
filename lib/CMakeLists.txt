# Add via ob standard library
include(OB/Library)
ob_add_standard_library(${LIB_TARGET_NAME}
    NAMESPACE "${PROJECT_NAMESPACE}"
    ALIAS "${LIB_ALIAS_NAME}"
    EXPORT_HEADER
        PATH "${PROJECT_NAME_LC}/qmpi_export.h"
    HEADERS_API
        COMMON "${PROJECT_NAME_LC}"
        FILES
            "qmpi.h"
    IMPLEMENTATION
        "qmpi.cpp"
    LINKS
        PRIVATE
            Qx::Core
        PUBLIC
            Qt6::Core
            Qt6::Network
    CONFIG STANDARD
 )
