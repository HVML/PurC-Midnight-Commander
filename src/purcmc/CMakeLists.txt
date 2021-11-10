set_property(DIRECTORY . PROPERTY FOLDER "PurcMC")

include(GlobalCommon)
include(MyMacros.cmake)

configure_file(purcmc_version.h.in ${PurcMC_DERIVED_SOURCES_DIR}/purcmc_version.h)
configure_file(purcmc_features.h.in ${PurcMC_DERIVED_SOURCES_DIR}/purcmc_features.h)

set(PurcMC_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${PURCMC_DIR}"
    "${PurcMC_DERIVED_SOURCES_DIR}"
    "${FORWARDING_HEADERS_DIR}"
)

# TODO: List the headers should be installed to system here.
set(PurcMC_INSTALLED_HEADERS
)

# TODO: List the directorires which do not contain any platform-dependent C source files
set(PurcMC_PLATFORM_INDEPENDENT_DIRS
    "lib"
    "src"
)

# This macro will append all C/CXX source files in ${PurcMC_PLATFORM_INDEPENDENT_DIRS}
# to ${PurcMC_SOURCES}
APPEND_ALL_SOURCE_FILES_IN_DIRLIST(PurcMC_SOURCES "${PurcMC_PLATFORM_INDEPENDENT_DIRS}")

# TODO: List the source files individually.
list(APPEND PurcMC_SOURCES
)

# Put the CXX source files which can be unified in `Sources.txt`
list(APPEND PurcMC_UNIFIED_SOURCE_LIST_FILES
    "Sources.txt"
)

set(PurcMC_LIBRARIES)

if (ENABLE_XML)
    list(APPEND PurcMC_LIBRARIES LibXml2::LibXml2)
endif ()

set(PurcMC_INTERFACE_LIBRARIES PurcMC)
set(PurcMC_INTERFACE_INCLUDE_DIRECTORIES ${PurcMC_PRIVATE_FRAMEWORK_HEADERS_DIR})

PURCMC_FRAMEWORK_DECLARE(PurcMC)

PURCMC_WRAP_SOURCELIST(${PurcMC_SOURCES})

PURCMC_COPY_FILES(PurcMC_CopyPrivateHeaders
    DESTINATION ${PurcMC_PRIVATE_FRAMEWORK_HEADERS_DIR}/purcmc
    FILES ${PurcMC_PRIVATE_FRAMEWORK_HEADERS}
    FLATTENED
)
list(APPEND PurcMC_INTERFACE_DEPENDENCIES PurcMC_CopyPrivateHeaders)
# PurcMC_CopyPrivateHeaders needs to have a direct or indirect
# dependency of PurcMC target for CMake Visual Studio generator to
# eliminate duplicated custom commands. Otherwise, CSSPropertyNames.h
# will be generated in both projects.
if (NOT INTERNAL_BUILD)
    add_dependencies(PurcMC_CopyPrivateHeaders PurcMC)
endif ()

PURCMC_COMPUTE_SOURCES(PurcMC)
PURCMC_FRAMEWORK(PurcMC)
PURCMC_FRAMEWORK_TARGET(PurcMC)

# The -ftree-sra optimization (implicit with -O2) causes crashes when
# allocating large chunks of memory using bmalloc on Intel 32bit.
# See https://bugs.webkit.org/show_bug.cgi?id=146440
string(TOLOWER ${CMAKE_HOST_SYSTEM_PROCESSOR} LOWERCASE_CMAKE_HOST_SYSTEM_PROCESSOR)
if (CMAKE_COMPILER_IS_GNUCXX AND "${LOWERCASE_CMAKE_HOST_SYSTEM_PROCESSOR}" MATCHES "(i[3-6]86|x86)$")
    PURCMC_ADD_TARGET_CXX_FLAGS(PurcMC -fno-tree-sra)
endif ()

if (${PurcMC_LIBRARY_TYPE} MATCHES "SHARED")
    set_target_properties(PurcMC PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
    install(TARGETS PurcMC DESTINATION "${LIB_INSTALL_DIR}")
endif ()

if (MSVC)
    set_target_properties(PurcMC PROPERTIES
        COMPILE_PDB_NAME ${PurcMC_OUTPUT_NAME}
    )
endif ()

# add_subdirectory(res)

install(TARGETS PurcMC
        DESTINATION "${LIB_INSTALL_DIR}/"
)

install(FILES ${PurcMC_INSTALLED_HEADERS}
        DESTINATION "${HEADER_INSTALL_DIR}/purcmc"
)
