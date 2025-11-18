find_package(Git REQUIRED)

execute_process(COMMAND
        "${GIT_EXECUTABLE}"
        describe
        WORKING_DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE GIT_RESULT
        OUTPUT_VARIABLE GIT_VERSION
        ERROR_VARIABLE GIT_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE)

if(NOT GIT_RESULT EQUAL 0)
    file(STRINGS "VERSION" GIT_VERSION)
endif()

string(REGEX REPLACE "^([0-9]+)\\..*" "\\1" VERSION_MAJOR "${GIT_VERSION}")
string(REGEX REPLACE "^[0-9]+\\.([0-9]+).*" "\\1" VERSION_MINOR "${GIT_VERSION}")
string(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" VERSION_PATCH "${GIT_VERSION}")

string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+(-[0-9]+)-.*" VERSION_EXTRA_MATCH "${GIT_VERSION}")
if (VERSION_EXTRA_MATCH)
    string(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.[0-9]+(-.*)" "\\1" VERSION_EXTRA "${GIT_VERSION}")
endif()
