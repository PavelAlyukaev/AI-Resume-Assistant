# Install-time step that runs after Qt's deploy script (windeployqt).
# windeployqt copies Qt DLLs and plugins but not the third-party DLLs that
# MSYS2's Qt links against (ICU, zlib, zstd, freetype, harfbuzz, pcre2, ...).
# Expects AIRA_RUNTIME_DIRS to be set by a preceding install(CODE).

set(prefix "${CMAKE_INSTALL_PREFIX}")

# Only QSQLITE is used; other SQL drivers pull in libpq, ODBC, MySQL client.
file(GLOB sql_drivers "${prefix}/plugins/sqldrivers/*.dll")
foreach(driver IN LISTS sql_drivers)
    get_filename_component(driver_name "${driver}" NAME_WE)
    if(NOT driver_name MATCHES "^qsqlite")
        file(REMOVE "${driver}")
    endif()
endforeach()

file(GLOB executables "${prefix}/bin/*.exe")
file(GLOB_RECURSE plugins "${prefix}/plugins/*.dll")

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES ${executables}
    LIBRARIES ${plugins}
    DIRECTORIES ${AIRA_RUNTIME_DIRS}
    PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-"
    POST_EXCLUDE_REGEXES "^[A-Za-z]:[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\]"
    RESOLVED_DEPENDENCIES_VAR resolved
    UNRESOLVED_DEPENDENCIES_VAR unresolved
)

foreach(dll IN LISTS resolved)
    get_filename_component(dll_name "${dll}" NAME)
    if(NOT EXISTS "${prefix}/bin/${dll_name}")
        file(INSTALL "${dll}" DESTINATION "${prefix}/bin")
    endif()
endforeach()

if(unresolved)
    message(WARNING "Unresolved runtime dependencies: ${unresolved}")
endif()
