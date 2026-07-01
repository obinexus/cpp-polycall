# cpp-polycall - C++ reference binding. Built when POLYCALL_BUILD_BINDINGS=ON.
# Header-only RAII wrapper (src/polycall.hpp) over the public C ABI.
add_executable(cpp-polycall examples/main.cpp)
target_include_directories(cpp-polycall PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(cpp-polycall PRIVATE polycall::polycall)
set_target_properties(cpp-polycall PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

if(POLYCALL_BUILD_TESTS)
    add_test(NAME binding_cpp_polycall
             COMMAND cpp-polycall ${CMAKE_CURRENT_SOURCE_DIR}/cpp-polycallrc)
endif()
