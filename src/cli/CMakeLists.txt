
list(APPEND
	CLI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
)

add_executable(fuseecli ${CLI_SOURCES})
target_compile_options(fuseecli PRIVATE ${CFLAGS})
target_link_libraries(fuseecli LINK_PUBLIC fuseec)
install(TARGETS fuseecli)
