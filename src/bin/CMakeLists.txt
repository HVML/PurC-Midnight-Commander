include(GlobalCommon)
include(target/PurcMC)
include(MyMacros.cmake)

set(BIN_DIR "${CMAKE_SOURCE_DIR}/src/bin/")

PURCMC_EXECUTABLE_DECLARE(purcmc)

list(APPEND purcmc_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${PurcMC_DERIVED_SOURCES_DIR}"
    "${CMAKE_SOURCE_DIR}/src/purcmc/"
    "${CMAKE_SOURCE_DIR}/src/bin/"
    "${CMAKE_SOURCE_DIR}/src/bin/vfs-plugins/"
)

list(APPEND purcmc_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

PURCMC_EXECUTABLE(purcmc)

set(purcmc_PLATFORM_INDEPENDENT_DIRS
    "src"
)

APPEND_ALL_SOURCE_FILES_IN_DIRLIST(purcmc_SOURCES "${purcmc_PLATFORM_INDEPENDENT_DIRS}")

list(APPEND purcmc_SOURCES
)

set(purcmc_LIBRARIES
    PurcMC::PurcMC
    pthread
)

PURCMC_COMPUTE_SOURCES(purcmc)
PURCMC_FRAMEWORK(purcmc)

