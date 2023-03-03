# Library setup
set(header_common_path "qi-qmp")

# Add via ob standard library
include(OB/Library)
ob_add_standard_library(${LIB_TARGET_NAME}
    NAMESPACE "${PROJECT_NAMESPACE}"
    ALIAS "${LIB_ALIAS_NAME}"
    EXPORT_HEADER "${header_common_path}/qmpi_export.h"
    HEADERS_API
        COMMON "${header_common_path}"
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
