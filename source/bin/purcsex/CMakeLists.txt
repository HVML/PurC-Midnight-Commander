include(GlobalCommon)
include(target/PurcMC)

PURCMC_EXECUTABLE_DECLARE(purcsex)

list(APPEND purcsex_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${PurcMC_DERIVED_SOURCES_DIR}"
    "${PURCMC_LIB_DIR}"
    "${PURCMC_BIN_DIR}"
)

list(APPEND purcsex_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

PURCMC_EXECUTABLE(purcsex)

list(APPEND purcsex_SOURCES
    purcsex.c
)

set(purcsex_LIBRARIES
    PurcMC::PurcMC
    PurC::PurC
    pthread
)

PURCMC_COMPUTE_SOURCES(purcsex)
PURCMC_FRAMEWORK(purcsex)

PURCMC_COPY_FILES(purcsex_copy_samples
    DESTINATION ${CMAKE_BINARY_DIR}/
    FILES shownews.json shownews.html shownews.frag
)
