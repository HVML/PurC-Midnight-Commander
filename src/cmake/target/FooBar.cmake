if (NOT TARGET PurcMC::PurcMC)
    if (NOT INTERNAL_BUILD)
        message(FATAL_ERROR "PurcMC::PurcMC target not found")
    endif ()

    # This should be moved to an if block if the Apple Mac/iOS build moves completely to CMake
    # Just assuming Windows for the moment
    add_library(PurcMC::PurcMC STATIC IMPORTED)
    set_target_properties(PurcMC::PurcMC PROPERTIES
        IMPORTED_LOCATION ${WEBKIT_LIBRARIES_LINK_DIR}/PurcMC${DEBUG_SUFFIX}.lib
    )
    set(PurcMC_PRIVATE_FRAMEWORK_HEADERS_DIR "${CMAKE_BINARY_DIR}/../include/private")
    target_include_directories(PurcMC::PurcMC INTERFACE
        ${PurcMC_PRIVATE_FRAMEWORK_HEADERS_DIR}
    )
endif ()
