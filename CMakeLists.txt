cmake_minimum_required(VERSION 3.16)

project(cpp_polycall VERSION 1.0.0 LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(CPP_POLYCALL_BUILD_EXAMPLE "Build the cpp-polycall example" OFF)
set(CPP_POLYCALL_CORE_LIBRARY "" CACHE FILEPATH
    "Path to a libpolycall v1.5 library for the example executable")

add_library(cpp_polycall STATIC src/polycall.cpp)
add_library(cpp_polycall::cpp_polycall ALIAS cpp_polycall)

target_compile_features(cpp_polycall PUBLIC cxx_std_17)
target_include_directories(cpp_polycall
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/generated)
set_target_properties(cpp_polycall PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME cpp_polycall)

if(MSVC)
    target_compile_options(cpp_polycall PRIVATE /W4)
else()
    target_compile_options(cpp_polycall PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(cpp_polycall_test
        tests/cpp_polycall_test.cpp
        tests/polycall_ffi_mock.cpp)
    target_link_libraries(cpp_polycall_test PRIVATE
        cpp_polycall::cpp_polycall)
    target_include_directories(cpp_polycall_test PRIVATE tests)
    add_test(NAME cpp_polycall_adapter COMMAND cpp_polycall_test)
endif()

if(CPP_POLYCALL_BUILD_EXAMPLE)
    add_executable(cpp_polycall_example examples/main.cpp)
    target_link_libraries(cpp_polycall_example PRIVATE
        cpp_polycall::cpp_polycall)
    set_target_properties(cpp_polycall_example PROPERTIES OUTPUT_NAME cpp-polycall)

    if(TARGET polycall::polycall)
        target_link_libraries(cpp_polycall_example PRIVATE polycall::polycall)
    elseif(CPP_POLYCALL_CORE_LIBRARY)
        target_link_libraries(cpp_polycall_example PRIVATE
            ${CPP_POLYCALL_CORE_LIBRARY})
    else()
        message(FATAL_ERROR
            "CPP_POLYCALL_BUILD_EXAMPLE requires polycall::polycall or "
            "-DCPP_POLYCALL_CORE_LIBRARY=/path/to/libpolycall")
    endif()
endif()

install(TARGETS cpp_polycall
    EXPORT cpp_polycallTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
    cmake/cpp_polycallConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cpp_polycallConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp_polycall)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cpp_polycallConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(EXPORT cpp_polycallTargets
    FILE cpp_polycallTargets.cmake
    NAMESPACE cpp_polycall::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp_polycall)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cpp_polycallConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cpp_polycallConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp_polycall)
