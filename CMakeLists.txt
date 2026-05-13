cmake_minimum_required(VERSION 3.22)

option(MCSOLVERENGINE_WITH_OCCT "Build OCCT-backed BREP export when OpenCASCADE is available" ON)

set(MCSOLVERENGINE_HAS_OCCT OFF)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(McSolverEngine LANGUAGES CXX)
    option(MCSOLVERENGINE_BUILD_TESTS "Build McSolverEngine tests" ON)
    include(CTest)
    if(NOT EIGEN3_INCLUDE_DIR)
        set(_mcsolverengine_eigen_candidates
            "${CMAKE_CURRENT_SOURCE_DIR}/../.pixi/envs/default/Library/include/eigen3"
            "C:/work/bim-common-client/Common/CppThird/Eigen"
        )
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

    if(NOT BOOST_INCLUDEDIR)
        set(_mcsolverengine_boost_candidates
            "${CMAKE_CURRENT_SOURCE_DIR}/../.pixi/envs/default/Library/include"
            "C:/work/bim-common-client/packages/boost.1.81.0-native.1.0.3/lib/native/inc"
        )
        foreach(_mcsolverengine_boost_candidate IN LISTS _mcsolverengine_boost_candidates)
            if(EXISTS "${_mcsolverengine_boost_candidate}/boost/graph/graph_concepts.hpp")
                set(BOOST_INCLUDEDIR "${_mcsolverengine_boost_candidate}")
                break()
            endif()
        endforeach()
    endif()

    if(MCSOLVERENGINE_WITH_OCCT AND NOT OpenCASCADE_DIR AND NOT OCC_INCLUDE_DIR)
        set(_mcsolverengine_occt_config_candidates
            "${CMAKE_CURRENT_SOURCE_DIR}/../.pixi/envs/default/Library/lib/cmake/opencascade"
            "C:/work/bim-common-client/packages/opencascade.7.9-native.1.0.0/lib/cmake/opencascade"
        )
        foreach(_mcsolverengine_occt_config_candidate IN LISTS _mcsolverengine_occt_config_candidates)
            if(EXISTS "${_mcsolverengine_occt_config_candidate}/OpenCASCADEConfig.cmake")
                set(OpenCASCADE_DIR "${_mcsolverengine_occt_config_candidate}")
                break()
            endif()
        endforeach()
    endif()

    if(MCSOLVERENGINE_WITH_OCCT AND NOT OpenCASCADE_DIR AND NOT OCC_INCLUDE_DIR)
        set(_mcsolverengine_occt_roots
            "C:/work/bim-common-client/packages/opencascade.7.9-native.1.0.0/lib"
            "C:/work/bim-common-client/packages/opencascade.7.9-native.1.0.0"
        )
        foreach(_mcsolverengine_occt_root IN LISTS _mcsolverengine_occt_roots)
            if(
                EXISTS "${_mcsolverengine_occt_root}/build/lib/TKBRep.lib"
                AND EXISTS "${_mcsolverengine_occt_root}/build/bin/TKBRep.dll"
                AND EXISTS "${_mcsolverengine_occt_root}/../content/inc/BRepTools_ShapeSet.hxx"
            )
                file(GLOB _mcsolverengine_occ_libs "${_mcsolverengine_occt_root}/build/lib/*.lib")
                list(SORT _mcsolverengine_occ_libs)
                set(OCC_INCLUDE_DIR "${_mcsolverengine_occt_root}/../content/inc")
                set(OCC_LIBRARY_DIR "${_mcsolverengine_occt_root}/build/lib")
                set(OpenCASCADE_BINARY_DIR "${_mcsolverengine_occt_root}/build/bin")
                set(OCC_LIBRARIES ${_mcsolverengine_occ_libs})
                break()
            elseif(
                EXISTS "${_mcsolverengine_occt_root}/lib/build/lib/TKBRep.lib"
                AND EXISTS "${_mcsolverengine_occt_root}/lib/build/bin/TKBRep.dll"
                AND EXISTS "${_mcsolverengine_occt_root}/content/inc/BRepTools_ShapeSet.hxx"
            )
                file(GLOB _mcsolverengine_occ_libs "${_mcsolverengine_occt_root}/lib/build/lib/*.lib")
                list(SORT _mcsolverengine_occ_libs)
                set(OCC_INCLUDE_DIR "${_mcsolverengine_occt_root}/content/inc")
                set(OCC_LIBRARY_DIR "${_mcsolverengine_occt_root}/lib/build/lib")
                set(OpenCASCADE_BINARY_DIR "${_mcsolverengine_occt_root}/lib/build/bin")
                set(OCC_LIBRARIES ${_mcsolverengine_occ_libs})
                break()
            endif()
        endforeach()
    endif()

    if(MCSOLVERENGINE_WITH_OCCT AND OCC_LIBRARY_DIR AND NOT OpenCASCADE_BINARY_DIR)
        if(EXISTS "${OCC_LIBRARY_DIR}/../bin")
            get_filename_component(OpenCASCADE_BINARY_DIR "${OCC_LIBRARY_DIR}/../bin" ABSOLUTE)
        endif()
    endif()

    if(NOT EIGEN3_INCLUDE_DIR)
        find_package(Eigen3 REQUIRED)
    endif()

    if(MCSOLVERENGINE_WITH_OCCT)
        find_package(OpenCASCADE CONFIG QUIET COMPONENTS FoundationClasses ModelingData ModelingAlgorithms)
    endif()
else()
    if(NOT DEFINED MCSOLVERENGINE_BUILD_TESTS)
        set(MCSOLVERENGINE_BUILD_TESTS ${ENABLE_DEVELOPER_TESTS})
    endif()
endif()

if(MCSOLVERENGINE_WITH_OCCT AND (OpenCASCADE_FOUND OR TARGET TKBRep OR OCC_INCLUDE_DIR))
    set(MCSOLVERENGINE_HAS_OCCT ON)
elseif(MCSOLVERENGINE_WITH_OCCT)
    message(STATUS "OpenCASCADE not found; building McSolverEngine without BREP export support.")
endif()

set(MCSOLVERENGINE_RUNTIME_PATH "")
if(MCSOLVERENGINE_HAS_OCCT AND OpenCASCADE_BINARY_DIR)
    set(MCSOLVERENGINE_RUNTIME_PATH "${OpenCASCADE_BINARY_DIR}")
elseif(MCSOLVERENGINE_HAS_OCCT AND OCC_LIBRARY_DIR)
    get_filename_component(_mcsolverengine_occ_root "${OCC_LIBRARY_DIR}" DIRECTORY)
    if(EXISTS "${_mcsolverengine_occ_root}/bin")
        set(MCSOLVERENGINE_RUNTIME_PATH "${_mcsolverengine_occ_root}/bin")
    endif()
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
        target_compile_options(${target} PRIVATE /permissive-)
    endif()
endfunction()

set(MCSOLVERENGINE_CORE_SOURCES
    src/BRepExport.cpp
    src/CompatModel.cpp
    src/CompatSolver.cpp
    src/DocumentXml.cpp
    src/Engine.cpp
    src/GeometryExport.cpp
    src/Version.cpp
    src/planegcs/Constraints.cpp
    src/planegcs/GCS.cpp
    src/planegcs/Geo.cpp
    src/planegcs/SubSystem.cpp
    src/planegcs/qp_eq.cpp
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

if(TARGET Eigen3::Eigen)
    target_link_libraries(McSolverEngineCore PRIVATE Eigen3::Eigen)
elseif(EIGEN3_INCLUDE_DIR)
    target_include_directories(McSolverEngineCore SYSTEM PRIVATE ${EIGEN3_INCLUDE_DIR})
endif()

if(BOOST_INCLUDEDIR)
    target_include_directories(McSolverEngineCore SYSTEM PRIVATE ${BOOST_INCLUDEDIR})
endif()

if(MCSOLVERENGINE_HAS_OCCT AND TARGET TKBRep)
    target_link_libraries(McSolverEngineCore PRIVATE TKBRep TKTopAlgo TKShHealing)
elseif(MCSOLVERENGINE_HAS_OCCT AND OCC_INCLUDE_DIR)
    target_include_directories(McSolverEngineCore SYSTEM PRIVATE ${OCC_INCLUDE_DIR})
    if(OCC_LIBRARY_DIR)
        target_link_directories(McSolverEngineCore PRIVATE ${OCC_LIBRARY_DIR})
    endif()
    if(OCC_LIBRARIES)
        target_link_libraries(McSolverEngineCore PRIVATE ${OCC_LIBRARIES})
    endif()
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
    elseif(MCSOLVERENGINE_HAS_OCCT AND OCC_INCLUDE_DIR)
        target_include_directories(McSolverEngineSmokeTest SYSTEM PRIVATE ${OCC_INCLUDE_DIR})
        if(OCC_LIBRARY_DIR)
            target_link_directories(McSolverEngineSmokeTest PRIVATE ${OCC_LIBRARY_DIR})
        endif()
        if(OCC_LIBRARIES)
            target_link_libraries(McSolverEngineSmokeTest PRIVATE ${OCC_LIBRARIES})
        endif()
    endif()

    target_link_libraries(McSolverEngineSmokeTest
        PRIVATE
            McSolverEngineCore
    )

    target_link_libraries(McSolverEngineCApiSmokeTest
        PRIVATE
            McSolverEngineNative
    )

    if(BUILD_TESTING)
        if(MCSOLVERENGINE_RUNTIME_PATH)
            add_test(
                NAME McSolverEngineSmokeTest
                COMMAND
                    ${CMAKE_COMMAND} -E env "PATH=${MCSOLVERENGINE_RUNTIME_PATH};$ENV{PATH}"
                    $<TARGET_FILE:McSolverEngineSmokeTest>
            )
            add_test(
                NAME McSolverEngineCApiSmokeTest
                COMMAND
                    ${CMAKE_COMMAND} -E env "PATH=$<TARGET_FILE_DIR:McSolverEngineNative>;${MCSOLVERENGINE_RUNTIME_PATH};$ENV{PATH}"
                    $<TARGET_FILE:McSolverEngineCApiSmokeTest>
            )
        else()
            add_test(NAME McSolverEngineSmokeTest COMMAND McSolverEngineSmokeTest)
            add_test(
                NAME McSolverEngineCApiSmokeTest
                COMMAND
                    ${CMAKE_COMMAND} -E env "PATH=$<TARGET_FILE_DIR:McSolverEngineNative>;$ENV{PATH}"
                    $<TARGET_FILE:McSolverEngineCApiSmokeTest>
            )
        endif()
    endif()
endif()

install(
    TARGETS
        McSolverEngineCore
        McSolverEngineNative
        McSolverEngineCli
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
