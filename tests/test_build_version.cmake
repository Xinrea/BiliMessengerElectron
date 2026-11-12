cmake_minimum_required(VERSION 3.16)
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/BMVersion.cmake")

if(CHILD)
    bm_resolve_version("2.0.0" "${INPUT}" actual)
    if(NOT "${actual}" STREQUAL "${EXPECTED}")
        message(FATAL_ERROR "Expected '${EXPECTED}', got '${actual}'")
    endif()
    return()
endif()

function(check_version input expected)
    bm_resolve_version("2.0.0" "${input}" actual)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR "Input '${input}': expected '${expected}', got '${actual}'")
    endif()
endfunction()

check_version("" "2.0.0")
check_version("2.0.0" "2.0.0")
check_version("v2.0.0" "2.0.0")
check_version("v2.0.0-beta.1" "2.0.0-beta.1")
check_version("2.0.0-rc.1+build.001" "2.0.0-rc.1+build.001")
check_version("v2.0.0+build-beta.001" "2.0.0+build-beta.001")
check_version("2.0.0-01a" "2.0.0-01a")

foreach(input IN ITEMS
        "2.0" "02.0.0" "2.00.0" "2.0.00" "2.0.0.1"
        "2.0.0-" "2.0.0-01" "2.0.0-beta.01" "2.0.0-beta..1"
        "2.0.0-beta." "2.0.0+" "2.0.0+build." "2.0.0+build..1"
        "2.0.0+build+other" "2.0.0-rc_1" "release-2.0.0" "vv2.0.0"
        "2.0.0\n" "2.0.0\"")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -DCHILD=ON "-DINPUT=${input}" "-DEXPECTED=${input}"
                -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
    if(result EQUAL 0 OR NOT stderr MATCHES "Invalid release SemVer")
        message(FATAL_ERROR "Expected invalid SemVer rejection for '${input}': ${stdout}${stderr}")
    endif()
endforeach()

foreach(input IN ITEMS "v2.1.0-beta.1" "v1.5.5" "3.0.0")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -DCHILD=ON "-DINPUT=${input}" "-DEXPECTED=${input}"
                -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
    if(result EQUAL 0 OR NOT stderr MATCHES "does not match PROJECT_VERSION")
        message(FATAL_ERROR "Expected numeric version mismatch for '${input}': ${stdout}${stderr}")
    endif()
endforeach()

message(STATUS "Build version tests passed")
