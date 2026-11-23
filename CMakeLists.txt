# Copyright © 2021 CCP ehf.
cmake_minimum_required(VERSION 3.30)

project(EveLocalization VERSION 1.0.0)

include(cmake/CcpTargetConfigurations.cmake)
include(cmake/CcpDocsGenerator.cmake)
include(cmake/CcpPackageConfigHelpers.cmake)
include(cmake/CcpBuildConfigurations.cmake)

find_package(carbon-blueexposure REQUIRED CONFIG)
find_package(carbon-blue REQUIRED CONFIG)

set(headers
    autoversion.h
    collator.h
    formatters.h
    localization.h
    parser.h
    version.h
    WrapPointList.h
)

set(sources
    collator.cpp
    collator_blue.cpp
    formatters.cpp
    localization.cpp
    localization_blue.cpp
    parser.cpp
    parser_blue.cpp
    stdafx.cpp
    WrapPointList.cpp
    WrapPointList_Blue.cpp
)

if(APPLE)
    list(APPEND sources collator.mm)
endif()

ccp_add_library(EveLocalization SHARED
    ${headers}
    ${sources}
)
target_link_libraries(EveLocalization PRIVATE Blue BlueExposure)

target_precompile_headers(EveLocalization PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stdafx.h)
target_include_directories(EveLocalization PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(EveLocalization PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    OUTPUT_NAME "_evelocalization"
)

if(APPLE)
    enable_language(OBJCXX)
    set_source_files_properties(${sources} PROPERTIES LANGUAGE OBJCXX)

    set_target_properties(
        EveLocalization
        PROPERTIES 
            PREFIX ""
            SUFFIX ".so"
    )
    target_link_libraries(EveLocalization PRIVATE "-framework Foundation")

elseif(WIN32)

    target_link_libraries(EveLocalization PRIVATE usp10)

elseif(LINUX)

    # ICU provides locale-aware collation, number separators and line breaking on Linux,
    # mirroring what CoreFoundation provides on macOS and Win32/Uniscribe provide on Windows.
    find_package(ICU REQUIRED COMPONENTS uc i18n data)
    target_link_libraries(EveLocalization PRIVATE ICU::i18n ICU::uc ICU::data)

    set_target_properties(
        EveLocalization
        PROPERTIES
            PREFIX ""
            SUFFIX ".so"
    )

endif()

# Provide an install target iff this is the top level project only
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    option(BUILD_TESTING "Build tests. Enabled by default." ON)
    include(CTest)
    if(BUILD_TESTING)
        find_package(carbon-exefile-interpreter REQUIRED CONFIG)
        ccp_prepare_interpreter(
                TARGET EveLocalization
                BUILD_TYPE ${CMAKE_BUILD_TYPE}
                COMMAND_OUT PYTHON_INTERPRETER
                PYTHONPATH_OUT PYTHONPATH
                RUN_DIR $<TARGET_FILE_DIR:EveLocalization>
                BLUE_DIR_OUT BLUE_LOCATION_DIR
        )

        if (WIN32)
            add_custom_command(
                    TARGET EveLocalization POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:EveLocalization> $<TARGET_FILE_DIR:EveLocalization>
                    COMMAND_EXPAND_LISTS
            )
        endif ()

        # add the python tests
        list(APPEND PYTHON_TESTS test_eveLocalization.py)
        foreach(PYTHON_TEST ${PYTHON_TESTS})
            add_test(NAME ${PYTHON_TEST}
                    COMMAND ${PYTHON_INTERPRETER} /inherit /buildflavor=$<LOWER_CASE:$<CONFIG>> /py ${CMAKE_CURRENT_LIST_DIR}/python/eveLocalization/tests/${PYTHON_TEST}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python/eveLocalization/tests
            )

            if(WIN32)
                set_tests_properties(${PYTHON_TEST} PROPERTIES ENVIRONMENT "PYTHONPATH=${PYTHONPATH}\;${CMAKE_CURRENT_SOURCE_DIR}/python;BUILDFLAVOR=$<LOWER_CASE:$<CONFIG>>")
            elseif(APPLE OR LINUX)
                set_tests_properties(${PYTHON_TEST} PROPERTIES ENVIRONMENT "PYTHONPATH=${PYTHONPATH}:${CMAKE_CURRENT_SOURCE_DIR}/python;BUILDFLAVOR=$<LOWER_CASE:$<CONFIG>>")
            endif()

        endforeach()
    endif()

    option(INSTALL_TO_MONOLITH "Install this package to a perforce branch vendor folder - provided for backwards compatability" OFF)
    if (INSTALL_TO_MONOLITH)
        # Install rule to ensure that our runtime and linker files are in the expected, platform-specific folders
        install(
                TARGETS EveLocalization
                CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
                ARCHIVE DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
                LIBRARY DESTINATION lib/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
                RUNTIME DESTINATION bin/${CCP_PLATFORM}/${CCP_ARCHITECTURE}/${CCP_TOOLSET}/
        )

        # Install python package
        install(DIRECTORY python/ DESTINATION python
                FILES_MATCHING
                PATTERN "*.py"
                PATTERN "package.yaml"
        )
    else()
        # Install rule to ensure that our runtime and linker files are in the expected, platform-specific folders
        install(
                TARGETS EveLocalization
                CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES}
                ARCHIVE DESTINATION lib/
                LIBRARY DESTINATION lib/
                RUNTIME DESTINATION bin/
        )

        # Install python package
        install(DIRECTORY python/eveLocalization DESTINATION bin/python)
    endif()
endif()
