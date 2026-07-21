cmake_minimum_required(VERSION 3.22)

if(MSVC)
    if(DEFINED ENV{CMAKE_GENERATOR_INSTANCE})
        message(STATUS "Using Visual Studio instance: $ENV{CMAKE_GENERATOR_INSTANCE}")
    elseif(CMAKE_GENERATOR_INSTANCE)
        message(STATUS "Using Visual Studio instance: ${CMAKE_GENERATOR_INSTANCE}")
    else()
        message(STATUS "Using default Visual Studio generator (Visual Studio 17 2022)")
    endif()
endif()

option(MCSOLVERENGINE_WITH_OCCT "Build OCCT-backed BREP export when OpenCASCADE is available" OFF)

set(MCSOLVERENGINE_HAS_OCCT OFF)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(McSolverEngine LANGUAGES CXX)
    option(MCSOLVERENGINE_BUILD_TESTS "Build McSolverEngine tests" ON)
    include(CTest)
    set(_mcsolverengine_pixi_env_dir "${CMAKE_CURRENT_SOURCE_DIR}/.pixi/envs/default")
    set(_mcsolverengine_pixi_library_dir "${_mcsolverengine_pixi_env_dir}/Library")
    set(_mcsolverengine_eigen_candidates
        "${_mcsolverengine_pixi_library_dir}/include/eigen3"
        "${_mcsolverengine_pixi_env_dir}/include/eigen3"
    )
    if(EXISTS "${_mcsolverengine_pixi_library_dir}/include/boost")
        set(_mcsolverengine_boost_candidate "${_mcsolverengine_pixi_library_dir}/include")
    else()
        set(_mcsolverengine_boost_candidate "${_mcsolverengine_pixi_env_dir}/include")
    endif()

    if(EIGEN3_INCLUDE_DIR)
        get_filename_component(EIGEN3_INCLUDE_DIR "${EIGEN3_INCLUDE_DIR}" ABSOLUTE)
        set(_mcsolverengine_eigen_dir_allowed OFF)
        foreach(_mcsolverengine_eigen_candidate IN LISTS _mcsolverengine_eigen_candidates)
            get_filename_component(
                _mcsolverengine_eigen_candidate
                "${_mcsolverengine_eigen_candidate}"
                ABSOLUTE
            )
            get_filename_component(
                _mcsolverengine_eigen_candidate_parent
                "${_mcsolverengine_eigen_candidate}"
                DIRECTORY
            )
            if(
                EIGEN3_INCLUDE_DIR STREQUAL "${_mcsolverengine_eigen_candidate}"
                OR EIGEN3_INCLUDE_DIR STREQUAL "${_mcsolverengine_eigen_candidate_parent}"
            )
                set(_mcsolverengine_eigen_dir_allowed ON)
                break()
            endif()
        endforeach()
        if(NOT _mcsolverengine_eigen_dir_allowed)
            message(FATAL_ERROR
                "Eigen3 must come from .pixi/envs/default. "
                "Set EIGEN3_INCLUDE_DIR to the Eigen headers inside .pixi or run `pixi install`."
            )
        endif()
    else()
        foreach(_mcsolverengine_eigen_candidate IN LISTS _mcsolverengine_eigen_candidates)
            if(EXISTS "${_mcsolverengine_eigen_candidate}/Eigen/Core")
                set(EIGEN3_INCLUDE_DIR "${_mcsolverengine_eigen_candidate}")
                break()
            elseif(
                EXISTS "${_mcsolverengine_eigen_candidate}/Core"
                AND EXISTS "${_mcsolverengine_eigen_candidate}/src"
            )
                get_filename_component(
                    _mcsolverengine_eigen_parent
                    "${_mcsolverengine_eigen_candidate}"
                    DIRECTORY
                )
                set(EIGEN3_INCLUDE_DIR "${_mcsolverengine_eigen_parent}")
                break()
            endif()
        endforeach()
    endif()

    if(NOT EIGEN3_INCLUDE_DIR)
        message(FATAL_ERROR
            "Eigen3 headers were not found in .pixi/envs/default. Run `pixi install`."
        )
    endif()

    if(BOOST_INCLUDEDIR)
        get_filename_component(BOOST_INCLUDEDIR "${BOOST_INCLUDEDIR}" ABSOLUTE)
        if(NOT BOOST_INCLUDEDIR STREQUAL "${_mcsolverengine_boost_candidate}")
            message(FATAL_ERROR
                "Boost headers must come from .pixi/envs/default. "
                "Set BOOST_INCLUDEDIR to the Boost headers inside .pixi or run `pixi install`."
            )
        endif()
    elseif(EXISTS "${_mcsolverengine_boost_candidate}/boost/graph/graph_concepts.hpp")
        set(BOOST_INCLUDEDIR "${_mcsolverengine_boost_candidate}")
    endif()

    if(NOT BOOST_INCLUDEDIR)
        message(FATAL_ERROR
            "Boost headers were not found in .pixi/envs/default. Run `pixi install`."
        )
    endif()

    if(MCSOLVERENGINE_WITH_OCCT)
        set(_mcsolverengine_occt_config_candidates
            "${_mcsolverengine_pixi_library_dir}/lib/cmake/opencascade"
            "${_mcsolverengine_pixi_env_dir}/lib/cmake/opencascade"
        )

        if(OpenCASCADE_DIR)
            get_filename_component(OpenCASCADE_DIR "${OpenCASCADE_DIR}" ABSOLUTE)
            set(_mcsolverengine_occt_dir_allowed OFF)
            foreach(_mcsolverengine_occt_config_candidate IN LISTS _mcsolverengine_occt_config_candidates)
                get_filename_component(
                    _mcsolverengine_occt_config_candidate
                    "${_mcsolverengine_occt_config_candidate}"
                    ABSOLUTE
                )
                if(OpenCASCADE_DIR STREQUAL "${_mcsolverengine_occt_config_candidate}")
                    set(_mcsolverengine_occt_dir_allowed ON)
                    break()
                endif()
            endforeach()
            if(NOT _mcsolverengine_occt_dir_allowed)
                message(FATAL_ERROR
                    "MCSOLVERENGINE_WITH_OCCT=ON only supports OCCT from .pixi. "
                    "Set OpenCASCADE_DIR to a .pixi opencascade config directory or run `pixi install`."
                )
            endif()
            if(NOT EXISTS "${OpenCASCADE_DIR}/OpenCASCADEConfig.cmake")
                message(FATAL_ERROR
                    "OpenCASCADEConfig.cmake was not found under `${OpenCASCADE_DIR}`. "
                    "Run `pixi install` to provision OCCT in .pixi."
                )
            endif()
        else()
            foreach(_mcsolverengine_occt_config_candidate IN LISTS _mcsolverengine_occt_config_candidates)
                if(EXISTS "${_mcsolverengine_occt_config_candidate}/OpenCASCADEConfig.cmake")
                    set(OpenCASCADE_DIR "${_mcsolverengine_occt_config_candidate}")
                    break()
                endif()
            endforeach()
        endif()

        if(NOT OpenCASCADE_DIR)
            message(FATAL_ERROR
                "MCSOLVERENGINE_WITH_OCCT=ON requires OCCT from .pixi/envs/default. "
                "Run `pixi install` and ensure the `occt` package is present."
            )
        endif()

        if(EXISTS "${_mcsolverengine_pixi_library_dir}/bin")
            set(OpenCASCADE_BINARY_DIR "${_mcsolverengine_pixi_library_dir}/bin")
        elseif(EXISTS "${_mcsolverengine_pixi_env_dir}/bin")
            set(OpenCASCADE_BINARY_DIR "${_mcsolverengine_pixi_env_dir}/bin")
        elseif(EXISTS "${_mcsolverengine_pixi_env_dir}/lib")
            set(OpenCASCADE_BINARY_DIR "${_mcsolverengine_pixi_env_dir}/lib")
        endif()

        find_package(OpenCASCADE CONFIG REQUIRED
            PATHS "${OpenCASCADE_DIR}"
            NO_DEFAULT_PATH
            COMPONENTS FoundationClasses ModelingData ModelingAlgorithms
        )
    endif()

else()
    if(NOT DEFINED MCSOLVERENGINE_BUILD_TESTS)
        set(MCSOLVERENGINE_BUILD_TESTS ${ENABLE_DEVELOPER_TESTS})
    endif()
endif()

if(MCSOLVERENGINE_WITH_OCCT AND (OpenCASCADE_FOUND OR TARGET TKBRep))
    set(MCSOLVERENGINE_HAS_OCCT ON)
endif()

set(MCSOLVERENGINE_RUNTIME_PATH "")
if(MCSOLVERENGINE_HAS_OCCT AND OpenCASCADE_BINARY_DIR)
    set(MCSOLVERENGINE_RUNTIME_PATH "${OpenCASCADE_BINARY_DIR}")
endif()

include(GNUInstallDirs)

set(MCSOLVERENGINE_VERSION "0.1.0-dev")

function(mcsolverengine_enable_cxx20 target visibility)
    target_compile_features(${target} ${visibility} cxx_std_20)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    if(MSVC)
        target_compile_options(${target} PRIVATE /permissive- /utf-8)
    endif()
endfunction()

enable_language(C)

set(MCSOLVERENGINE_ZLIB_SOURCES
    src/third_party/zlib/adler32.c
    src/third_party/zlib/crc32.c
    src/third_party/zlib/inffast.c
    src/third_party/zlib/inflate.c
    src/third_party/zlib/inftrees.c
    src/third_party/zlib/zutil.c
)

add_library(McSolverEngineZip STATIC
    ${MCSOLVERENGINE_ZLIB_SOURCES}
    src/ZipExtract.cpp
)

mcsolverengine_enable_cxx20(McSolverEngineZip PRIVATE)
target_include_directories(McSolverEngineZip
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/zlib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
set(_mcsolverengine_zlib_prefix_header
    "${CMAKE_CURRENT_SOURCE_DIR}/src/third_party/zlib/mse_zlib_prefix.h"
)
if(MSVC)
    target_compile_options(McSolverEngineZip PRIVATE
        /FI"${_mcsolverengine_zlib_prefix_header}"
        /wd4127
        /wd4131
    )
else()
    target_compile_options(McSolverEngineZip PRIVATE
        -include "${_mcsolverengine_zlib_prefix_header}"
    )
endif()

add_library(McSolverEngine::Zip ALIAS McSolverEngineZip)

set(MCSOLVERENGINE_CORE_SOURCES
    src/BRepExport.cpp
    src/CompatModel.cpp
    src/CompatSolver.cpp
    src/DocumentSolver.cpp
    src/DocumentXml.cpp
    src/PlaneTransform.cpp
    src/SubElementResolver.cpp
    src/Engine.cpp
    src/GeometryExport.cpp
    src/VarSetExpressionEngine.cpp
    src/Version.cpp
    src/core/planegcs/Constraints.cpp
    src/core/planegcs/GCS.cpp
    src/core/planegcs/Geo.cpp
    src/core/planegcs/SubSystem.cpp
    src/core/planegcs/qp_eq.cpp
)

if(MCSOLVERENGINE_HAS_OCCT)
    list(APPEND MCSOLVERENGINE_CORE_SOURCES
        src/SketchShapeBuilder.cpp
    )
endif()

add_library(McSolverEngineCore STATIC ${MCSOLVERENGINE_CORE_SOURCES})

add_library(McSolverEngine::Core ALIAS McSolverEngineCore)

mcsolverengine_enable_cxx20(McSolverEngineCore PUBLIC)
target_compile_definitions(McSolverEngineCore
    PUBLIC
        MCSOLVERENGINE_STATIC_DEFINE
        MCSOLVERENGINE_WITH_OCCT=$<BOOL:${MCSOLVERENGINE_HAS_OCCT}>
    PRIVATE
        MCSOLVERENGINE_CORE_BUILD
        MCSOLVERENGINE_VERSION="${MCSOLVERENGINE_VERSION}"
)

target_include_directories(McSolverEngineCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(McSolverEngineCore PRIVATE McSolverEngineZip)

if(TARGET Eigen3::Eigen)
    target_link_libraries(McSolverEngineCore PRIVATE Eigen3::Eigen)
elseif(EIGEN3_INCLUDE_DIR)
    target_include_directories(McSolverEngineCore SYSTEM PRIVATE ${EIGEN3_INCLUDE_DIR})
endif()

if(BOOST_INCLUDEDIR)
    target_include_directories(McSolverEngineCore SYSTEM PRIVATE ${BOOST_INCLUDEDIR})
endif()

if(MCSOLVERENGINE_HAS_OCCT AND TARGET TKBRep)
    target_link_libraries(McSolverEngineCore PUBLIC TKBRep TKTopAlgo TKShHealing)
endif()

add_library(McSolverEngineNative SHARED
    src/CApi.cpp
)

mcsolverengine_enable_cxx20(McSolverEngineNative PRIVATE)
target_compile_definitions(McSolverEngineNative
    PRIVATE
        MCSOLVERENGINE_CAPI_BUILD
        MCSOLVERENGINE_VERSION="${MCSOLVERENGINE_VERSION}"
)

target_include_directories(McSolverEngineNative
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(McSolverEngineNative
    PRIVATE
        McSolverEngineCore
)

set_target_properties(McSolverEngineNative PROPERTIES OUTPUT_NAME mcsolverengine_native)

add_executable(McSolverEngineCli
    apps/mcsolverengine_cli.cpp
)

mcsolverengine_enable_cxx20(McSolverEngineCli PRIVATE)

target_include_directories(McSolverEngineCli
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(McSolverEngineCli
    PRIVATE
        McSolverEngineCore
)

set_target_properties(McSolverEngineCli PROPERTIES OUTPUT_NAME mcsolverengine)

if(MCSOLVERENGINE_BUILD_TESTS)
    add_executable(McSolverEngineSmokeTest
        tests/smoke.cpp
    )
    mcsolverengine_enable_cxx20(McSolverEngineSmokeTest PRIVATE)

    add_executable(McSolverEngineCApiSmokeTest
        tests/capi_smoke.cpp
    )
    mcsolverengine_enable_cxx20(McSolverEngineCApiSmokeTest PRIVATE)

    add_executable(McSolverEngineFreeCadSourcesTest
        tests/freecad_sources_test.cpp
    )
    mcsolverengine_enable_cxx20(McSolverEngineFreeCadSourcesTest PRIVATE)

    add_executable(McSolverEngineV111T003DebugTest
        tests/v111t003_debug.cpp
    )
    mcsolverengine_enable_cxx20(McSolverEngineV111T003DebugTest PRIVATE)

    add_executable(McSolverEngineUnitTest
        tests/unit.cpp
    )
    mcsolverengine_enable_cxx20(McSolverEngineUnitTest PRIVATE)

    target_include_directories(McSolverEngineSmokeTest
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_compile_definitions(McSolverEngineSmokeTest
        PRIVATE
            MCSOLVERENGINE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_include_directories(McSolverEngineCApiSmokeTest
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_compile_definitions(McSolverEngineCApiSmokeTest
        PRIVATE
            MCSOLVERENGINE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
            MCSOLVERENGINE_WITH_OCCT=$<BOOL:${MCSOLVERENGINE_HAS_OCCT}>
    )

    target_include_directories(McSolverEngineUnitTest
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_include_directories(McSolverEngineFreeCadSourcesTest
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_include_directories(McSolverEngineV111T003DebugTest
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_compile_definitions(McSolverEngineV111T003DebugTest
        PRIVATE
            MCSOLVERENGINE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )

    if(TARGET Eigen3::Eigen)
        target_link_libraries(McSolverEngineSmokeTest PRIVATE Eigen3::Eigen)
    elseif(EIGEN3_INCLUDE_DIR)
        target_include_directories(McSolverEngineSmokeTest SYSTEM PRIVATE ${EIGEN3_INCLUDE_DIR})
    endif()

    if(BOOST_INCLUDEDIR)
        target_include_directories(McSolverEngineSmokeTest SYSTEM PRIVATE ${BOOST_INCLUDEDIR})
    endif()

    if(MCSOLVERENGINE_HAS_OCCT AND TARGET TKBRep)
        target_link_libraries(McSolverEngineSmokeTest PRIVATE TKBRep TKTopAlgo TKShHealing)
    endif()

    target_link_libraries(McSolverEngineSmokeTest
        PRIVATE
            McSolverEngineCore
    )

    target_link_libraries(McSolverEngineCApiSmokeTest
        PRIVATE
            McSolverEngineNative
    )

    target_link_libraries(McSolverEngineUnitTest
        PRIVATE
            McSolverEngineCore
    )

    target_link_libraries(McSolverEngineFreeCadSourcesTest
        PRIVATE
            McSolverEngineCore
    )

    target_link_libraries(McSolverEngineV111T003DebugTest
        PRIVATE
            McSolverEngineCore
    )

    if(BUILD_TESTING)
        if(WIN32)
            set(MCSOLVERENGINE_PATH_SEP ";")
            set(MCSOLVERENGINE_LIB_PATH_VAR "PATH")
        elseif(APPLE)
            set(MCSOLVERENGINE_PATH_SEP ":")
            set(MCSOLVERENGINE_LIB_PATH_VAR "DYLD_LIBRARY_PATH")
        else()
            set(MCSOLVERENGINE_PATH_SEP ":")
            set(MCSOLVERENGINE_LIB_PATH_VAR "LD_LIBRARY_PATH")
        endif()

        if(MCSOLVERENGINE_RUNTIME_PATH)
            add_test(
                NAME McSolverEngineSmokeTest
                COMMAND
                    ${CMAKE_COMMAND} -E env "${MCSOLVERENGINE_LIB_PATH_VAR}=${MCSOLVERENGINE_RUNTIME_PATH}${MCSOLVERENGINE_PATH_SEP}$ENV{${MCSOLVERENGINE_LIB_PATH_VAR}}"
                    $<TARGET_FILE:McSolverEngineSmokeTest>
            )
            add_test(
                NAME McSolverEngineCApiSmokeTest
                COMMAND
                    ${CMAKE_COMMAND} -E env "${MCSOLVERENGINE_LIB_PATH_VAR}=$<TARGET_FILE_DIR:McSolverEngineNative>${MCSOLVERENGINE_PATH_SEP}${MCSOLVERENGINE_RUNTIME_PATH}${MCSOLVERENGINE_PATH_SEP}$ENV{${MCSOLVERENGINE_LIB_PATH_VAR}}"
                    $<TARGET_FILE:McSolverEngineCApiSmokeTest>
            )
            add_test(
                NAME McSolverEngineUnitTest
                COMMAND
                    ${CMAKE_COMMAND} -E env "${MCSOLVERENGINE_LIB_PATH_VAR}=$<TARGET_FILE_DIR:McSolverEngineNative>${MCSOLVERENGINE_PATH_SEP}${MCSOLVERENGINE_RUNTIME_PATH}${MCSOLVERENGINE_PATH_SEP}$ENV{${MCSOLVERENGINE_LIB_PATH_VAR}}"
                    $<TARGET_FILE:McSolverEngineUnitTest>
            )
            add_test(
                NAME McSolverEngineFreeCadSourcesTest
                COMMAND
                    ${CMAKE_COMMAND} -E env "${MCSOLVERENGINE_LIB_PATH_VAR}=$<TARGET_FILE_DIR:McSolverEngineNative>${MCSOLVERENGINE_PATH_SEP}${MCSOLVERENGINE_RUNTIME_PATH}${MCSOLVERENGINE_PATH_SEP}$ENV{${MCSOLVERENGINE_LIB_PATH_VAR}}"
                    $<TARGET_FILE:McSolverEngineFreeCadSourcesTest>
            )
            add_test(
                NAME McSolverEngineV111T003DebugTest
                COMMAND
                    ${CMAKE_COMMAND} -E env "${MCSOLVERENGINE_LIB_PATH_VAR}=$<TARGET_FILE_DIR:McSolverEngineNative>${MCSOLVERENGINE_PATH_SEP}${MCSOLVERENGINE_RUNTIME_PATH}${MCSOLVERENGINE_PATH_SEP}$ENV{${MCSOLVERENGINE_LIB_PATH_VAR}}"
                    $<TARGET_FILE:McSolverEngineV111T003DebugTest>
            )
        else()
            add_test(NAME McSolverEngineSmokeTest COMMAND McSolverEngineSmokeTest)
            add_test(
                NAME McSolverEngineCApiSmokeTest
                COMMAND
                    ${CMAKE_COMMAND} -E env "${MCSOLVERENGINE_LIB_PATH_VAR}=$<TARGET_FILE_DIR:McSolverEngineNative>${MCSOLVERENGINE_PATH_SEP}$ENV{${MCSOLVERENGINE_LIB_PATH_VAR}}"
                    $<TARGET_FILE:McSolverEngineCApiSmokeTest>
            )
            add_test(
                NAME McSolverEngineUnitTest
                COMMAND
                    ${CMAKE_COMMAND} -E env "${MCSOLVERENGINE_LIB_PATH_VAR}=$<TARGET_FILE_DIR:McSolverEngineNative>${MCSOLVERENGINE_PATH_SEP}$ENV{${MCSOLVERENGINE_LIB_PATH_VAR}}"
                    $<TARGET_FILE:McSolverEngineUnitTest>
            )
            add_test(
                NAME McSolverEngineFreeCadSourcesTest
                COMMAND
                    ${CMAKE_COMMAND} -E env "${MCSOLVERENGINE_LIB_PATH_VAR}=$<TARGET_FILE_DIR:McSolverEngineNative>${MCSOLVERENGINE_PATH_SEP}$ENV{${MCSOLVERENGINE_LIB_PATH_VAR}}"
                    $<TARGET_FILE:McSolverEngineFreeCadSourcesTest>
            )
            add_test(
                NAME McSolverEngineV111T003DebugTest
                COMMAND
                    ${CMAKE_COMMAND} -E env "${MCSOLVERENGINE_LIB_PATH_VAR}=$<TARGET_FILE_DIR:McSolverEngineNative>${MCSOLVERENGINE_PATH_SEP}$ENV{${MCSOLVERENGINE_LIB_PATH_VAR}}"
                    $<TARGET_FILE:McSolverEngineV111T003DebugTest>
            )
        endif()

        # ── C# wrapper integration ───────────────────────────────────
        find_program(DOTNET_EXECUTABLE dotnet)
        if(DOTNET_EXECUTABLE)
            set(_csharp_wrapper_proj "${CMAKE_CURRENT_SOURCE_DIR}/wrapper/csharp/McSolverEngine.Wrapper.csproj")
            set(_csharp_tests_proj "${CMAKE_CURRENT_SOURCE_DIR}/wrapper/csharp/tests/McSolverEngine.Wrapper.Tests.csproj")

            if(WIN32)
                set(_csharp_build_args_wrapper "")
                set(_csharp_build_args_tests "")
            else()
                set(_csharp_build_args_wrapper "--framework" "net8.0")
                set(_csharp_build_args_tests "--framework" "net8.0")
            endif()

            add_custom_target(McSolverEngineCSharpWrapper ALL
                COMMAND ${CMAKE_COMMAND} -E env DOTNET_CLI_UI_LANGUAGE=en
                    ${DOTNET_EXECUTABLE} build "${_csharp_wrapper_proj}"
                    --configuration $<CONFIG>
                    ${_csharp_build_args_wrapper}
                DEPENDS McSolverEngineNative
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Building C# wrapper ($<CONFIG>)"
            )

            add_custom_target(McSolverEngineCSharpTests ALL
                COMMAND ${CMAKE_COMMAND} -E env DOTNET_CLI_UI_LANGUAGE=en
                    ${DOTNET_EXECUTABLE} build "${_csharp_tests_proj}"
                    --configuration $<CONFIG>
                    ${_csharp_build_args_tests}
                DEPENDS McSolverEngineCSharpWrapper
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Building C# tests ($<CONFIG>)"
            )

            if(WIN32)
                add_test(
                    NAME McSolverEngineCSharpWrapperTests_net48
                    COMMAND
                        ${CMAKE_COMMAND} -E env
                        "${MCSOLVERENGINE_LIB_PATH_VAR}=$<TARGET_FILE_DIR:McSolverEngineNative>${MCSOLVERENGINE_PATH_SEP}${MCSOLVERENGINE_RUNTIME_PATH}${MCSOLVERENGINE_PATH_SEP}$ENV{${MCSOLVERENGINE_LIB_PATH_VAR}}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/wrapper/csharp/tests/bin/$<CONFIG>/net48/McSolverEngine.Wrapper.Tests.exe"
                )
                set_tests_properties(McSolverEngineCSharpWrapperTests_net48 PROPERTIES
                    LABELS "csharp"
                )
            endif()

            add_test(
                NAME McSolverEngineCSharpWrapperTests_net80
                COMMAND
                    ${CMAKE_COMMAND} -E env
                    DOTNET_CLI_UI_LANGUAGE=en
                    "${MCSOLVERENGINE_LIB_PATH_VAR}=$<TARGET_FILE_DIR:McSolverEngineNative>${MCSOLVERENGINE_PATH_SEP}${MCSOLVERENGINE_RUNTIME_PATH}${MCSOLVERENGINE_PATH_SEP}$ENV{${MCSOLVERENGINE_LIB_PATH_VAR}}"
                    ${DOTNET_EXECUTABLE} run --project "${_csharp_tests_proj}"
                    --configuration $<CONFIG>
                    --framework net8.0
                    --no-build
            )
            set_tests_properties(McSolverEngineCSharpWrapperTests_net80 PROPERTIES
                LABELS "csharp"
            )
        endif()
    endif()
endif()

install(
    TARGETS
        McSolverEngineCore
        McSolverEngineNative
        McSolverEngineCli
        McSolverEngineZip
    EXPORT McSolverEngineTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/McSolverEngine
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT McSolverEngineTargets
    NAMESPACE McSolverEngine::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/McSolverEngine
)
