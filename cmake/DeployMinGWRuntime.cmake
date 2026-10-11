# Install-time step that runs after Qt's deploy script (windeployqt).
# windeployqt copies Qt DLLs and plugins but not the third-party DLLs that
# MSYS2's Qt links against (ICU, zlib, zstd, freetype, harfbuzz, pcre2, ...).
# Expects AIRA_RUNTIME_DIRS to be set by a preceding install(CODE).

if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

set(prefix "${CMAKE_INSTALL_PREFIX}")

# Only QSQLITE is used; other SQL drivers pull in libpq, ODBC, MySQL client.
# windeployqt's plugin layout differs between Qt builds, so search the prefix.
file(GLOB_RECURSE sql_drivers "${prefix}/*.dll")
list(FILTER sql_drivers INCLUDE REGEX "/sqldrivers/[^/]*$")
foreach(driver IN LISTS sql_drivers)
    get_filename_component(driver_name "${driver}" NAME_WE)
    if(NOT driver_name MATCHES "^qsqlite")
        file(REMOVE "${driver}")
    endif()
endforeach()

file(GLOB executables "${prefix}/bin/*.exe")
# Plugins (MSYS2 Qt puts them under share/qt6/plugins); DLLs already in bin/
# are reached through the executable.
file(GLOB_RECURSE plugins "${prefix}/*.dll")
list(FILTER plugins EXCLUDE REGEX "/bin/[^/]*$")

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES ${executables}
    LIBRARIES ${plugins}
    DIRECTORIES "${prefix}/bin" ${AIRA_RUNTIME_DIRS}
    PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-"
    POST_EXCLUDE_REGEXES "^[A-Za-z]:[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\]"
    RESOLVED_DEPENDENCIES_VAR resolved
    UNRESOLVED_DEPENDENCIES_VAR unresolved
    # A DLL windeployqt already copied to bin/ also resolves from MSYS2;
    # either copy is fine, so keep the one in bin/.
    CONFLICTING_DEPENDENCIES_PREFIX conflict
)

foreach(dll_name IN LISTS conflict_FILENAMES)
    list(GET conflict_${dll_name} 0 dll)
    list(APPEND resolved "${dll}")
endforeach()

foreach(dll IN LISTS resolved)
    get_filename_component(dll_name "${dll}" NAME)
    if(NOT EXISTS "${prefix}/bin/${dll_name}")
        file(INSTALL "${dll}" DESTINATION "${prefix}/bin")
    endif()
endforeach()

if(unresolved)
    message(WARNING "Unresolved runtime dependencies: ${unresolved}")
endif()
