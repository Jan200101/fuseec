find_package(libusb1)

list(APPEND
    FUSEEC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_executable(fuseec ${FUSEEC_SOURCES})
target_link_libraries(fuseec PUBLIC intermezzo)

target_include_directories(fuseec PUBLIC ${LIBUSB1_INCLUDE_DIRS})
target_link_libraries(fuseec LINK_PUBLIC ${LIBUSB1_LIBRARIES})
