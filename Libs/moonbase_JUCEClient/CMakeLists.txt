cmake_minimum_required(VERSION 3.20)

project(moonbase_JUCEClient VERSION 1.0.0 LANGUAGES C CXX
    DESCRIPTION "Proprietary JUCE module to connect Moonbase for licensing"
    HOMEPAGE_URL "https://github.com/moonbase-sh/moonbase_JUCEClient")


include(cmake/CPM.cmake)

# This config is for running tests standalone
# not relevant when run from a plugin as a module
if (moonbase_JUCEClient_IS_TOP_LEVEL)
    
    # TODO: Remove this
    add_definitions(-w)

    option(CPP20 "Compile with C++20" OFF)
    if (CPP20)
        set(CMAKE_CXX_STANDARD 20)
        set(CMAKE_CXX_STANDARD_REQUIRED YES)
    else ()
        set(CMAKE_CXX_STANDARD 17)
        set(CMAKE_CXX_STANDARD_REQUIRED YES)
    endif ()

    option(JUCE7 "Run tests on JUCE 7" OFF)
    if (JUCE7)
        CPMAddPackage("gh:juce-framework/juce#7.0.12")
    else ()
        CPMAddPackage("gh:juce-framework/juce#8.0.4")
    endif ()

    # Use Catch2 v3 for testing
    CPMAddPackage("gh:catchorg/Catch2@3.7.1")

    # Add a test gui app
    juce_add_gui_app("mock_app"
        VERSION 1.0.0
        NEEDS_CURL TRUE
        NEEDS_WEB_BROWSER TRUE
    )
    target_sources("mock_app" PRIVATE "tests/mock_app.cpp")

    target_compile_definitions("mock_app" PUBLIC
        JUCE_USE_CURL=1
        JUCE_WEB_BROWSER=1
    )

    # Add mock json
    juce_add_binary_data(mock_binary SOURCES tests/moonbase_api_config.json)
    set_target_properties(mock_binary PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

    # Ideally this could be true, but lots of warnings in obfy
    # set_target_properties(mock_app PROPERTIES COMPILE_WARNING_AS_ERROR ON)

    # Add all test files to a console app
    juce_add_console_app(tests)
    juce_generate_juce_header(tests)
    set_target_properties(tests PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    file(GLOB_RECURSE TestFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.tests.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.tests.h")
    target_sources(tests PRIVATE ${TestFiles})
    target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
    target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    target_compile_definitions(tests PRIVATE
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_UNIT_TESTS=1
        JUCE_LOG_ASSERTIONS=1
    )
    target_compile_definitions(tests PRIVATE $<TARGET_PROPERTY:${PROJECT_NAME},COMPILE_DEFINITIONS>)
    include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)
    catch_discover_tests(tests ${DISCOVERY_MODE} "PRE_TEST")

    # Add the test executable
    enable_testing()

endif ()

if (NOT COMMAND juce_add_module)
    message(FATAL_ERROR "JUCE must be added to your project before moonbase!")
endif ()

# Note: CLion + Ninja + JUCE has a problem when JUCE modules are in the base dir
# Avoid ninja when building Moonbase's tests standalone
# See: https://forum.juce.com/t/infinite-loop-calling-juce-add-module-from-a-cmakelists-txt-in-the-modules-own-directory/
juce_add_module("${CMAKE_CURRENT_LIST_DIR}")

if (moonbase_JUCEClient_IS_TOP_LEVEL)
    target_link_libraries(mock_app PRIVATE
        moonbase_JUCEClient
        mock_binary
        juce_audio_processors
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
        
    target_link_libraries(tests PRIVATE
        juce_audio_processors
        juce_product_unlocking
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
endif ()
