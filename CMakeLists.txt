cmake_minimum_required(VERSION 3.25)
project(AIResumeAssistant VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Qt6 6.7 REQUIRED COMPONENTS Core Widgets Sql Network HttpServer Test)
find_package(libzip CONFIG REQUIRED)
qt_standard_project_setup()
add_library(resume_core STATIC
    src/domain/Resume.h
    src/application/ResumeJson.h
    src/application/ResumeJson.cpp
    src/application/DocumentImport.h
    src/application/DocumentImport.cpp
    src/storage/ResumeRepository.h
    src/storage/ResumeRepository.cpp
)
# resume_documents is linked here so the application layer can call the DOCX container reader.
target_link_libraries(resume_core PUBLIC Qt6::Core Qt6::Sql resume_documents)
target_include_directories(resume_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# DOCX container layer: libzip reads ZIP bytes held in memory (no path API, see ZipArchive.h).
add_library(resume_documents STATIC
    src/documents/DocxTypes.h
    src/documents/ZipArchive.h
    src/documents/ZipArchive.cpp
    src/documents/DocxContainer.h
    src/documents/DocxContainer.cpp
)
target_link_libraries(resume_documents PUBLIC Qt6::Core libzip::zip)
target_include_directories(resume_documents PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Local web UI: loopback-only HTTP server with the HTML front end embedded as a resource.
add_library(resume_web STATIC
    src/web/WebServer.h
    src/web/WebServer.cpp
)
target_link_libraries(resume_web PUBLIC resume_core Qt6::Network Qt6::HttpServer)
qt_add_resources(resume_web web_assets PREFIX "/web" BASE src/web FILES src/web/index.html)
qt_add_executable(AIResumeAssistant WIN32
    src/main.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
)
target_link_libraries(AIResumeAssistant PRIVATE resume_core resume_web Qt6::Widgets)

# Deployment: `cmake --install` lays out a self-contained app (exe, Qt and
# MinGW DLLs, plugins); the `installer` target packs it with Inno Setup.
if(WIN32)
    install(TARGETS AIResumeAssistant RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    # No executable imports a libzip symbol in Phase 1 (only resume_documents links it, and
    # the UI does not call it until Phase 3), so the linker records no import of libzip.dll.
    # Install it explicitly; DeployMinGWRuntime.cmake then resolves its transitive DLLs.
    install(IMPORTED_RUNTIME_ARTIFACTS libzip::zip RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    qt_generate_deploy_app_script(
        TARGET AIResumeAssistant
        OUTPUT_SCRIPT qt_deploy_script
        NO_TRANSLATIONS
    )
    install(SCRIPT ${qt_deploy_script})

    get_filename_component(compiler_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    install(CODE "set(AIRA_RUNTIME_DIRS \"${compiler_bin_dir};${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}\")")
    install(SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeployMinGWRuntime.cmake)

    set(program_files_x86 "ProgramFiles(x86)")
    find_program(ISCC_EXECUTABLE iscc
        PATHS
            "$ENV{${program_files_x86}}/Inno Setup 6"
            "$ENV{ProgramFiles}/Inno Setup 6"
            "$ENV{LOCALAPPDATA}/Programs/Inno Setup 6"
            "C:/Program Files (x86)/Inno Setup 6"
    )
    if(ISCC_EXECUTABLE)
        set(deploy_dir "${CMAKE_BINARY_DIR}/deploy")
        set(installer_dir "${CMAKE_BINARY_DIR}/installer")
        file(TO_NATIVE_PATH "${deploy_dir}" deploy_dir_native)
        file(TO_NATIVE_PATH "${installer_dir}" installer_dir_native)
        file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/installer/AIResumeAssistant.iss" iss_native)
        add_custom_target(installer
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${deploy_dir}"
            COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --prefix "${deploy_dir}"
            COMMAND "${ISCC_EXECUTABLE}" /Q "/DSourceDir=${deploy_dir_native}"
                    "/DAppVersion=${PROJECT_VERSION}" "/O${installer_dir_native}" "${iss_native}"
            DEPENDS AIResumeAssistant
            COMMENT "Building Inno Setup installer in ${installer_dir}"
            VERBATIM
        )
    else()
        message(STATUS "Inno Setup (iscc) not found: `installer` target is unavailable")
    endif()
endif()
enable_testing()
qt_add_executable(resume_e2e_test tests/ResumeE2ETest.cpp)
target_link_libraries(resume_e2e_test PRIVATE resume_core Qt6::Test)
add_test(NAME resume_e2e COMMAND resume_e2e_test)
set_tests_properties(resume_e2e PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(resume_json_test tests/ResumeJsonTest.cpp)
target_link_libraries(resume_json_test PRIVATE resume_core Qt6::Test)
add_test(NAME resume_json COMMAND resume_json_test)

qt_add_executable(zip_archive_test tests/ZipArchiveTest.cpp)
target_link_libraries(zip_archive_test PRIVATE resume_documents Qt6::Test)
add_test(NAME zip_archive COMMAND zip_archive_test)

qt_add_executable(document_import_test tests/DocumentImportTest.cpp)
target_link_libraries(document_import_test PRIVATE resume_core Qt6::Test)
add_test(NAME document_import COMMAND document_import_test)
qt_add_executable(web_api_test tests/WebApiTest.cpp)
target_link_libraries(web_api_test PRIVATE resume_web Qt6::Test)
add_test(NAME web_api COMMAND web_api_test)

# Browser e2e for the web UI: needs Node.js and `npm install` in tests/e2e (playwright-core + installed Chrome).
qt_add_executable(web_e2e_server tests/e2e/WebE2EServer.cpp)
target_link_libraries(web_e2e_server PRIVATE resume_web)
find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/node_modules/playwright-core")
    add_test(NAME web_e2e
        COMMAND "${NODE_EXECUTABLE}" tests/e2e/web_e2e.mjs "$<TARGET_FILE:web_e2e_server>"
                "${CMAKE_CURRENT_SOURCE_DIR}/screenshots/web-e2e-sprint1.png"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
else()
    message(STATUS "web_e2e skipped: node or tests/e2e/node_modules is missing (run `npm install` in tests/e2e)")
endif()
