# Packaging check: a separate project consuming an INSTALLED cpp-polycall
# through find_package(cpp_polycall), which pulls in find_package(polycall).
#
#   cmake --install <cpp-polycall build> --prefix <inst>
#   cmake -S tests/consumer -B <dir> -DCMAKE_PREFIX_PATH="<inst>;<polycall prefix>"
#   cmake --build <dir> && <dir>/cpp_polycall_consumer
cmake_minimum_required(VERSION 3.16)
project(cpp_polycall_consumer LANGUAGES CXX)

find_package(cpp_polycall 1 CONFIG REQUIRED)

add_executable(cpp_polycall_consumer main.cpp)
target_link_libraries(cpp_polycall_consumer PRIVATE cpp_polycall::cpp_polycall)
if(WIN32 AND TARGET polycall::polycall_shared)
    add_custom_command(TARGET cpp_polycall_consumer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:polycall::polycall_shared>
                $<TARGET_FILE_DIR:cpp_polycall_consumer>)
endif()
