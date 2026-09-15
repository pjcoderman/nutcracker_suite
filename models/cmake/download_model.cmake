# download_model.cmake

function(download_model DEST_FILE URL)
    if(EXISTS "${DEST_FILE}")
        message(STATUS "File already exists: ${DEST_FILE}")
        return()
    endif()

    get_filename_component(DEST_DIR "${DEST_FILE}" DIRECTORY)
    file(MAKE_DIRECTORY "${DEST_DIR}")

    message(STATUS "Fetching ${DEST_FILE} from ${URL}...")

    file(DOWNLOAD 
        "${URL}" 
        "${DEST_FILE}"
        SHOW_PROGRESS
        STATUS DL_STATUS
        LOG DL_LOG
    )

    list(GET DL_STATUS 0 STATUS_CODE)
    list(GET DL_STATUS 1 ERROR_MSG)

    if(NOT STATUS_CODE EQUAL 0)
        file(REMOVE "${DEST_FILE}")
        message(FATAL_ERROR "Download failed: ${ERROR_MSG}\n${DL_LOG}")
    endif()
endfunction()