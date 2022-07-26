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

set(calculator_SOURCES
    "calculator/calculator.c"
    log.c
)

ADD_PURCSEX_EXTENSION(calculator ${calculator_SOURCES})

set(introduction3_SOURCES
    "introduction3/introduction3.c"
    log.c
)

ADD_PURCSEX_EXTENSION(introduction3 ${introduction3_SOURCES})

set(introduction5_SOURCES
    "introduction5/introduction5.c"
    log.c
)

ADD_PURCSEX_EXTENSION(introduction5 ${introduction5_SOURCES})

set(navigation_SOURCES
    "navigation/navigation.c"
    log.c
)

ADD_PURCSEX_EXTENSION(navigation ${navigation_SOURCES})

set(asset_FILES
    "hello.json"
    "shownews.json"
    "shownews/shownews.html"
    "shownews/shownews-intro.frag"
    "shownews/shownews-news.frag"
    "calculator.json"
    "calculator/calculator.html"
    "calculator/calculator-header.frag"
    "calculator/calculator-intro.frag"
    "introduction.json"
    "introduction2.json"
    "introduction3.json"
    "introduction4.json"
    "introduction5.json"
    "navigation.json"
    "assets/hello.html"
    "assets/layout.html"
    "assets/layout-panes.html"
    "assets/layout-two-panes.html"
    "assets/modal-group.frag"
    "assets/page-groups.frag"
    "assets/splash.html"
    "assets/nav-tabs.html"
    "assets/about-hvml.html"
    "assets/about-purc.html"
    "assets/about-xguipro.html"
    "assets/layout-sidebar.html"
    "assets/sidebar.html"
)

PURCMC_COPY_FILES(purcsex_copy_asset_files
    DESTINATION "${CMAKE_BINARY_DIR}"
    FILES ${asset_FILES}
)

