cmake_minimum_required(VERSION 3.25)
project(AIResumeAssistant VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Sql Test)
qt_standard_project_setup()
add_library(resume_core STATIC
    src/domain/Resume.h
    src/storage/ResumeRepository.h
    src/storage/ResumeRepository.cpp
)
target_link_libraries(resume_core PUBLIC Qt6::Core Qt6::Sql)
target_include_directories(resume_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
qt_add_executable(AIResumeAssistant WIN32
    src/main.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
)
target_link_libraries(AIResumeAssistant PRIVATE resume_core Qt6::Widgets)

# Deployment: `cmake --install` lays out a self-contained app (exe, Qt and
# MinGW DLLs, plugins); the `installer` target packs it with Inno Setup.
if(WIN32)
    install(TARGETS AIResumeAssistant RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
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
