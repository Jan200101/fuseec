enable_language(CXX)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets Gui REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets Gui REQUIRED)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

list(APPEND
    QT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/window.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device_worker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device_worker.hpp

    ${CMAKE_CURRENT_SOURCE_DIR}/window.ui
    ${CMAKE_CURRENT_SOURCE_DIR}/assets.qrc
)

if(WIN32)
#    string(REPLACE "." "," CMAKE_PROJECT_COMMAVERSION ${CMAKE_PROJECT_VERSION})
#    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/assets/version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/version.rc @ONLY)
#    list(APPEND QT_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()

add_executable(fuseeqt WIN32 ${QT_SOURCES})
target_compile_options(fuseeqt PRIVATE ${CFLAGS})
target_link_libraries(fuseeqt LINK_PUBLIC fuseec)
target_link_libraries(fuseeqt PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
set_property(TARGET fuseeqt PROPERTY CXX_STANDARD 11)
install(TARGETS fuseeqt)
