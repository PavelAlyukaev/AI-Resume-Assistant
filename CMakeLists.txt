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
enable_testing()
qt_add_executable(resume_e2e_test tests/ResumeE2ETest.cpp)
target_link_libraries(resume_e2e_test PRIVATE resume_core Qt6::Test)
add_test(NAME resume_e2e COMMAND resume_e2e_test)
set_tests_properties(resume_e2e PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
