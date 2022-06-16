include(GlobalCommon)
include(target/PurcMC)

PURCMC_EXECUTABLE_DECLARE(purcsex)

list(APPEND purcsex_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${PurcMC_DERIVED_SOURCES_DIR}"
    "${PURCMC_LIB_DIR}"
    "${PURCMC_BIN_DIR}"
    "${FORWARDING_HEADERS_DIR}"
)

list(APPEND purcsex_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

PURCMC_EXECUTABLE(purcsex)

list(APPEND purcsex_SOURCES
    purcsex.c
    log.c
)

set(purcsex_LIBRARIES
    PurcMC::PurcMC
    PurC::PurC
    pthread
    dl
)

PURCMC_COMPUTE_SOURCES(purcsex)
PURCMC_FRAMEWORK(purcsex)

set(PurCSexExtension_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${PurCMC_DERIVED_SOURCES_DIR}"
    "${PURCMC_LIB_DIR}"
    "${PURCMC_BIN_DIR}"
    "${FORWARDING_HEADERS_DIR}"
)

set(PurCSexExtension_SYSTEM_INCLUDE_DIRECTORIES
    ${PURC_INCLUDE_DIRS}
)

set(PurCSexExtension_DEFINITIONS
    USE_STDOUT_WHEN_LOG_DISABLED
)

set(PurCSexExtension_LIBRARIES
    PurC::PurC
)

set(calculator_SOURCES
    "calculator/calculator.c"
    log.c
)

set(calculator_FILES
    "calculator/calculator.json"
    "calculator/calculator.html"
    "calculator/calculator-header.frag"
    "calculator/calculator-intro.frag"
)

set(shownews_FILES
    "shownews/shownews.json"
    "shownews/shownews.html"
    "shownews/shownews-intro.frag"
    "shownews/shownews-news.frag"
)

macro(ADD_PURCSEX_EXTENSION extension_name)
    add_library(${extension_name} MODULE ${ARGN})
    add_dependencies(${extension_name} purcsex)
    set_property(
        TARGET ${extension_name}
        APPEND
        PROPERTY COMPILE_DEFINITIONS WEBEXT_COMPILATION
    )
    set_target_properties(${extension_name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    target_compile_definitions(${extension_name} PUBLIC ${PurCSexExtension_DEFINITIONS})
    target_include_directories(${extension_name} PUBLIC ${PurCSexExtension_INCLUDE_DIRECTORIES})
    target_include_directories(${extension_name} SYSTEM PUBLIC ${PurCSexExtension_SYSTEM_INCLUDE_DIRECTORIES})
    target_link_libraries(${extension_name} ${PurCSexExtension_LIBRARIES})
endmacro()

ADD_PURCSEX_EXTENSION(calculator ${calculator_SOURCES})

PURCMC_COPY_FILES(purcsex_copy_shownews_files
    DESTINATION "${CMAKE_BINARY_DIR}"
    FILES ${shownews_FILES}
)

PURCMC_COPY_FILES(purcsex_copy_calculator_files
    DESTINATION "${CMAKE_BINARY_DIR}"
    FILES ${calculator_FILES}
)

