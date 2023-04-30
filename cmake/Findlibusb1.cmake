#
# LIBUSB1_INCLUDE_DIRS
# LIBUSB1_LIBRARIES
# LIBUSB1_CFLAGS

find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
    pkg_check_modules(_LIBUSB1 libusb-1.0)

    if (BUILD_STATIC AND NOT _LIBUSB1_FOUND)
         message(FATAL_ERROR "Cannot find static build information")
    endif()
endif()

if (_LIBUSB1_FOUND) # we can rely on pkg-config
    if (NOT BUILD_STATIC)
        set(LIBUSB1_LIBRARIES ${_LIBUSB1_LIBRARIES})
        set(LIBUSB1_INCLUDE_DIRS ${_LIBUSB1_INCLUDE_DIRS})
        set(LIBUSB1_CFLAGS ${_LIBUSB1_CFLAGS_OTHER})
    else()
        set(LIBUSB1_LIBRARIES ${_LIBUSB1_STATIC_LIBRARIES})
        set(LIBUSB1_INCLUDE_DIRS ${_LIBUSB1_STATIC_INCLUDE_DIRS})
        set(LIBUSB1_CFLAGS ${_LIBUSB1_STATIC_CFLAGS_OTHER})
    endif()
else()
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_lib_suffix 64)
    else()
        set(_lib_suffix 32)
    endif()

    find_path(LIBUSB1_INC
        NAMES libusb.h
        HINTS
            ${_LIBUSB1_INCLUDE_DIRS}
        PATHS
            /usr/include/libusb-1.0 /usr/local/include/libusb-1.0
    )

    find_library(LIBUSB1_LIB
        NAMES ${_LIBUSB1_LIBRARIES} usb-1.0 libusb-1.0
        HINTS
            ${_LIBUSB1_LIBRARY_DIRS}
            ${_LIBUSB1_STATIC_LIBRARY_DIRS}
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(JsonC DEFAULT_MSG LIBUSB1_LIB LIBUSB1_INC)
    mark_as_advanced(LIBUSB1_INC LIBUSB1_LIB)

    if(LIBUSB1_FOUND)
        set(LIBUSB1_INCLUDE_DIRS ${LIBUSB1_INC})
        set(LIBUSB1_LIBRARIES ${LIBUSB1_LIB})
        if (BUILD_STATIC)
            set(LIBUSB1_LIBRARIES ${LIBUSB1_LIBRARIES} ${_LIBUSB1_STATIC_LIBRARIES})
        endif()
    endif()
endif()