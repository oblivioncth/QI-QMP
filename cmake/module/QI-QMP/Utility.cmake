function(get_proper_system_name return)
    if(CMAKE_SYSTEM_NAME STREQUAL Windows)
        set(${return} Windows PARENT_SCOPE)
    elseif(CMAKE_SYSTEM_NAME STREQUAL Linux)
        # Get distro name
        execute_process(
            COMMAND sh -c "( awk -F= '\$1==\"NAME\" { print \$2 ;}' /etc/os-release || lsb_release -is ) 2>/dev/null"
            ERROR_QUIET
            RESULT_VARIABLE res
            OUTPUT_VARIABLE distro_name
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        # Handle cleanup and fallback
        if(("${distro_name}" STREQUAL ""))
            message(WARNING "Could not determine distro name. Falling back to 'Linux'")
            set(distro_name "Linux")
        else()
            string(REPLACE "\"" "" distro_name ${distro_name}) # Remove possible quotation
        endif()

        set(${return} "${distro_name}" PARENT_SCOPE)
    endif()
endfunction()
