cmake_minimum_required(VERSION 3.22)

set(CREATION_REMOTE_RECEIVER_VERSION "0.0.1" CACHE STRING "Creation Remote Receiver product version")
project(CreationRemoteReceiver VERSION ${CREATION_REMOTE_RECEIVER_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " /Zi")
    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " /DEBUG /OPT:REF /OPT:ICF")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE " /DEBUG /OPT:REF /OPT:ICF")
    string(APPEND CMAKE_MODULE_LINKER_FLAGS_RELEASE " /DEBUG /OPT:REF /OPT:ICF")
endif()

if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "Set JUCE_DIR to your JUCE root directory before configuring.")
endif()

set(CREATION_SUITE_SHARED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../shared" CACHE PATH "Path to the Creation Suite shared code root")

add_subdirectory("${JUCE_DIR}" juce)

# No CEL/Language dependency -- this app has no scripting or domain-intrinsic
# surface, unlike the creative apps. It only discovers projects and deposits
# received assets; keep it minimal rather than pulling in the LLVM-backed CEL
# stack for a domain it doesn't have.
add_subdirectory("${CREATION_SUITE_SHARED_DIR}/AssetSystem"
                 "${CMAKE_CURRENT_BINARY_DIR}/CreationSuiteSharedAssetSystem")
add_subdirectory("${CREATION_SUITE_SHARED_DIR}/VFS"
                 "${CMAKE_CURRENT_BINARY_DIR}/CreationSuiteSharedVFS")
add_subdirectory("${CREATION_SUITE_SHARED_DIR}/Services"
                 "${CMAKE_CURRENT_BINARY_DIR}/CreationSuiteSharedServices")
add_subdirectory("${CREATION_SUITE_SHARED_DIR}/UI"
                 "${CMAKE_CURRENT_BINARY_DIR}/CreationSuiteSharedUI")
add_subdirectory("${CREATION_SUITE_SHARED_DIR}/Interop"
                 "${CMAKE_CURRENT_BINARY_DIR}/CreationSuiteSharedInterop")

juce_add_gui_app(CreationRemoteReceiverApp
    PRODUCT_NAME "Creation Remote Receiver"
    COMPANY_NAME "LagDaemon Software"
    BUNDLE_ID "com.lagdaemon.creationremotereceiver"
    ICON_BIG "${CREATION_SUITE_SHARED_DIR}/UI/assets/creation_suite.png"
    ICON_SMALL "${CREATION_SUITE_SHARED_DIR}/UI/assets/creation_suite.png"
)

# Mirrors the built executable into the workspace-level shared bin directory
# (D:\CreationSuite-Workspaces\claude-<config>-bin) so it's reachable at a
# stable path without needing OS symlink privileges -- every suite app does
# this for its own repo, see the umbrella AGENTS.md's Shared Bin Directory Rule.
add_custom_command(TARGET CreationRemoteReceiverApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "D:/CreationSuite-Workspaces/claude-$<LOWER_CASE:$<CONFIG>>-bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:CreationRemoteReceiverApp>" "D:/CreationSuite-Workspaces/claude-$<LOWER_CASE:$<CONFIG>>-bin/"
    COMMENT "Copying CreationRemoteReceiverApp to the shared claude bin directory"
)

juce_generate_juce_header(CreationRemoteReceiverApp)

target_sources(CreationRemoteReceiverApp PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/MainComponent.h
)

target_compile_definitions(CreationRemoteReceiverApp PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CreationRemoteReceiverApp,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CreationRemoteReceiverApp,JUCE_VERSION>"
)

target_link_libraries(CreationRemoteReceiverApp PRIVATE
    creation_suite_asset_system
    creation_suite_vfs
    creation_suite_services
    creation_suite_ui
    creation_suite_ui_assets
    creation_suite_interop
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

install(TARGETS CreationRemoteReceiverApp
    RUNTIME DESTINATION .
    RESOURCE DESTINATION .
)
install(FILES README.md LICENSE DESTINATION .)

set(CPACK_PACKAGE_NAME "Creation Remote Receiver")
set(CPACK_PACKAGE_VENDOR "LagDaemon Software")
set(CPACK_PACKAGE_CONTACT "LagDaemon Software")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Creation Remote Receiver")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "creation-remote-receiver-${CPACK_PACKAGE_VERSION}-win64")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Creation Remote Receiver")
set(CPACK_GENERATOR "ZIP;WIX")
set(CPACK_WIX_PROGRAM_MENU_FOLDER "Creation Remote Receiver")
set(CPACK_PACKAGE_EXECUTABLES "Creation Remote Receiver" "Creation Remote Receiver")
set(CPACK_CREATE_DESKTOP_LINKS "Creation Remote Receiver")

include(CPack)
