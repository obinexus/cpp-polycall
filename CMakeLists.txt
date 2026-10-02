cmake_minimum_required(VERSION 3.16)

project(cpp_polycall VERSION 1.0.0 LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(CPP_POLYCALL_BUILD_EXAMPLE "Build the cpp-polycall example" OFF)

# The installed Polycall core (>= 1.1.0, binding ABI 1). Point CMake at it with
# -DCMAKE_PREFIX_PATH=<prefix> (e.g. /opt/polycall or windows-msvc-x64).
find_package(polycall 1.1 CONFIG REQUIRED)

find_package(Threads REQUIRED)

add_library(cpp_polycall STATIC src/polycall.cpp)
add_library(cpp_polycall::cpp_polycall ALIAS cpp_polycall)

target_compile_features(cpp_polycall PUBLIC cxx_std_17)
target_include_directories(cpp_polycall
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(cpp_polycall PUBLIC polycall::polycall)
set_target_properties(cpp_polycall PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME cpp_polycall)

if(MSVC)
    target_compile_options(cpp_polycall PRIVATE /W4 /WX)
else()
    target_compile_options(cpp_polycall PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(BUILD_TESTING)
    add_executable(cpp_polycall_test tests/real_core_test.cpp)
    target_link_libraries(cpp_polycall_test PRIVATE
        cpp_polycall::cpp_polycall Threads::Threads)
    if(MSVC)
        target_compile_options(cpp_polycall_test PRIVATE /W4)
    else()
        target_compile_options(cpp_polycall_test PRIVATE -Wall -Wextra)
    endif()

    # Run against exactly the installed core: on Windows place its DLL next to
    # the test (the build-tree RPATH covers ELF platforms).
    if(WIN32 AND TARGET polycall::polycall_shared)
        add_custom_command(TARGET cpp_polycall_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:polycall::polycall_shared>
                    $<TARGET_FILE_DIR:cpp_polycall_test>)
    endif()
    set(_cpp_polycall_env "")

    find_program(CPP_POLYCALL_SH NAMES sh bash
        HINTS "C:/Program Files/Git/bin" "C:/msys64/usr/bin")
    if(TARGET polycall::polycall_cli AND CPP_POLYCALL_SH)
        # Full checklist incl. cross-language interop with the C CLI.
        add_test(NAME cpp_polycall_real_core
            COMMAND ${CPP_POLYCALL_SH} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-real.sh
                    $<TARGET_FILE:cpp_polycall_test> ${CMAKE_CURRENT_SOURCE_DIR})
        list(APPEND _cpp_polycall_env "POLYCALL_CLI=$<TARGET_FILE:polycall::polycall_cli>")
    else()
        # Interop checks print SKIP (never PASS) without the CLI.
        add_test(NAME cpp_polycall_real_core
            COMMAND cpp_polycall_test ${CMAKE_CURRENT_SOURCE_DIR})
    endif()
    set_tests_properties(cpp_polycall_real_core PROPERTIES
        ENVIRONMENT "${_cpp_polycall_env}"
        SKIP_RETURN_CODE 77
        TIMEOUT 300)
endif()

if(CPP_POLYCALL_BUILD_EXAMPLE)
    add_executable(cpp_polycall_example examples/main.cpp)
    target_link_libraries(cpp_polycall_example PRIVATE cpp_polycall::cpp_polycall)
    set_target_properties(cpp_polycall_example PROPERTIES OUTPUT_NAME cpp-polycall)
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
