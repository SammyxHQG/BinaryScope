cmake_minimum_required(VERSION 3.24)
project(BinaryScope VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(BINARYSCOPE_BUILD_UI "Build the Qt desktop application" ON)
option(BINARYSCOPE_WITH_CAPSTONE "Fetch pinned Capstone for x86/x64/ARM64 disassembly" ON)
include(CTest)
add_library(binaryscope_core src/core/BinaryFile.cpp src/pe/PEParser.cpp src/pe/PESymbolParser.cpp
  src/analysis/StringExtractor.cpp src/analysis/ByteSearch.cpp src/analysis/EntropyAnalyzer.cpp src/analysis/Disassembler.cpp)
target_include_directories(binaryscope_core PUBLIC src)
if(BINARYSCOPE_WITH_CAPSTONE)
  include(FetchContent)
  set(CAPSTONE_ARCHITECTURE_DEFAULT OFF CACHE BOOL "" FORCE)
  set(CAPSTONE_X86_SUPPORT ON CACHE BOOL "" FORCE)
  set(CAPSTONE_ARM64_SUPPORT ON CACHE BOOL "" FORCE)
  set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)
  set(CAPSTONE_INSTALL OFF CACHE BOOL "" FORCE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
  FetchContent_Declare(capstone
    URL https://github.com/capstone-engine/capstone/archive/refs/tags/5.0.6.tar.gz
    URL_HASH SHA256=240ebc834c51aae41ca9215d3190cc372fd132b9c5c8aa2d5f19ca0c325e28f9
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  function(binaryscope_fetch_capstone)
    set(PROJECT_VERSION 5.0.6)
    FetchContent_MakeAvailable(capstone)
    install(FILES "${capstone_SOURCE_DIR}/LICENSE.TXT" DESTINATION licenses/capstone)
  endfunction()
  binaryscope_fetch_capstone()
  target_link_libraries(binaryscope_core PRIVATE capstone_static)
  target_compile_definitions(binaryscope_core PRIVATE BINARYSCOPE_CAPSTONE)
endif()
if(MSVC)
  target_compile_options(binaryscope_core PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(binaryscope_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(BINARYSCOPE_BUILD_UI)
  find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Concurrent)
  qt_standard_project_setup()
  add_library(binaryscope_ui STATIC src/core/HashCalculator.cpp src/core/AnalysisService.cpp src/ui/Theme.cpp
    src/ui/MainWindow.cpp src/ui/MainWindowActions.cpp src/ui/OverviewWidget.cpp src/ui/TablePage.cpp src/ui/PEWidgets.cpp
    src/ui/StringsWidget.cpp src/ui/HexWidget.cpp src/ui/EntropyWidget.cpp src/ui/DisassemblyWidget.cpp)
  target_link_libraries(binaryscope_ui PUBLIC binaryscope_core Qt6::Widgets Qt6::Concurrent)
  qt_add_executable(BinaryScope WIN32 src/main.cpp)
  target_link_libraries(BinaryScope PRIVATE binaryscope_ui)
  if(WIN32)
    # IDE runs and direct launches use the build tree, not the install directory.
    # Deploy the matching configuration's Qt DLLs and platform plugin after linking.
    get_target_property(binaryscope_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(binaryscope_qt_bin "${binaryscope_qmake}" DIRECTORY)
    find_program(BINARYSCOPE_WINDEPLOYQT windeployqt
      HINTS "${binaryscope_qt_bin}" NO_DEFAULT_PATH REQUIRED)
    add_custom_command(TARGET BinaryScope POST_BUILD
      COMMAND "${BINARYSCOPE_WINDEPLOYQT}"
        "$<IF:$<CONFIG:Debug>,--debug,--release>"
        --no-translations --no-opengl-sw --no-compiler-runtime
        --dir "$<TARGET_FILE_DIR:BinaryScope>"
        "$<TARGET_FILE:BinaryScope>"
      COMMENT "Deploying Qt runtime for direct and IDE launches"
      VERBATIM)
  endif()
  if(MSVC)
    target_compile_options(BinaryScope PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(binaryscope_ui PRIVATE /W4 /permissive- /utf-8)
  endif()
  install(TARGETS BinaryScope RUNTIME DESTINATION bin)
  if(MSVC)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION bin)
    include(InstallRequiredSystemLibraries)
  endif()
  qt_generate_deploy_app_script(TARGET BinaryScope OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
  install(SCRIPT ${deploy_script})
  if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(ui_tests tests/UiTests.cpp)
    set_target_properties(ui_tests PROPERTIES WIN32_EXECUTABLE FALSE)
    target_link_libraries(ui_tests PRIVATE binaryscope_ui Qt6::Test)
    if(MSVC)
      target_compile_options(ui_tests PRIVATE /utf-8)
    endif()
    add_test(NAME ui_tests COMMAND ${CMAKE_COMMAND}
      "-DTEST_EXE=$<TARGET_FILE:ui_tests>" "-DLOG_PATH=${CMAKE_CURRENT_BINARY_DIR}/ui-tests.log"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunUiTests.cmake")
    get_target_property(qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(qt_bin_dir "${qmake_executable}" DIRECTORY)
    set_tests_properties(ui_tests PROPERTIES
      TIMEOUT 120
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${qt_bin_dir}"
      ENVIRONMENT "QT_PLUGIN_PATH=${qt_bin_dir}/../plugins")
  endif()
endif()
if(BUILD_TESTING)
  add_executable(core_tests tests/CoreTests.cpp)
  target_link_libraries(core_tests PRIVATE binaryscope_core)
  add_test(NAME core_tests COMMAND core_tests)
endif()
