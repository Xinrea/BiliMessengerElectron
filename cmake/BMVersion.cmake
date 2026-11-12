# Keep platform bundle/file versions numeric, but retain the full release SemVer
# for the UI, diagnostics and update checks.
function(bm_resolve_version project_version release_version output)
    if("${release_version}" STREQUAL "")
        set(${output} "${project_version}" PARENT_SCOPE)
        return()
    endif()

    set(version "${release_version}")
    if(version MATCHES "^v")
        string(SUBSTRING "${version}" 1 -1 version)
    endif()
    set(number "(0|[1-9][0-9]*)")
    set(pre_id "(0|[1-9][0-9]*|[0-9A-Za-z-]*[A-Za-z-][0-9A-Za-z-]*)")
    set(build_id "[0-9A-Za-z-]+")
    if(NOT "${version}" MATCHES "^${number}\\.${number}\\.${number}(-${pre_id}(\\.${pre_id})*)?(\\+${build_id}(\\.${build_id})*)?$")
        message(FATAL_ERROR "Invalid release SemVer: ${release_version}")
    endif()

    string(REGEX REPLACE "[-+].*$" "" numeric_version "${version}")
    if(NOT "${numeric_version}" STREQUAL "${project_version}")
        message(FATAL_ERROR
            "Release version ${release_version} does not match PROJECT_VERSION ${project_version}")
    endif()
    set(${output} "${version}" PARENT_SCOPE)
endfunction()
