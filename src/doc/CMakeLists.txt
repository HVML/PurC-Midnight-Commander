set_property(DIRECTORY . PROPERTY FOLDER "doc")

include(GlobalCommon)

set(PREFIX ${CMAKE_INSTALL_PREFIX})
set(MAN_VERSION ${PROJECT_VERSION})
STRING(TIMESTAMP DATE_OF_MAN_PAGE "%B %Y")

configure_file(man/purcmc.1.in ${PurcMC_DERIVED_SOURCES_DIR}/man/purcmc.1 @ONLY)
configure_file(man/purcmcedit.1.in ${PurcMC_DERIVED_SOURCES_DIR}/man/purcmcedit.1 @ONLY)
configure_file(man/purcmcview.1.in ${PurcMC_DERIVED_SOURCES_DIR}/man/purcmcview.1 @ONLY)

# TODO: List man page files should be installed to system here.
set(PurcMC_MAN_FILES
    "${PurcMC_DERIVED_SOURCES_DIR}/man/purcmc.1"
    "${PurcMC_DERIVED_SOURCES_DIR}/man/purcmcedit.1"
    "${PurcMC_DERIVED_SOURCES_DIR}/man/purcmcview.1"
)

set(PurcMC_HELP_FILES
    "${PurcMC_DERIVED_SOURCES_DIR}/help/purcmc.hlp"
)

add_custom_command(
    OUTPUT ${PurcMC_HELP_FILES}
    MAIN_DEPENDENCY ${CMAKE_BINARY_DIR}/man2hlp
    DEPENDS ${PurcMC_DERIVED_SOURCES_DIR}/man/purcmc.1 ${PURCMC_DOC_DIR}/help/xnc.hlp
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PurcMC_DERIVED_SOURCES_DIR}/help"
    COMMAND ${PERL_EXECUTABLE} ${CMAKE_BINARY_DIR}/man2hlp ${PurcMC_DERIVED_SOURCES_DIR}/man/purcmc.1 ${PURCMC_DOC_DIR}/help/xnc.hlp ${PurcMC_DERIVED_SOURCES_DIR}/help/purcmc.hlp
    COMMENT "Generating purcmc.hlp..."
    VERBATIM)

add_custom_target(purcmc_help DEPENDS ${PurcMC_HELP_FILES})
add_dependencies(purcmc purcmc_help)

install(FILES ${PurcMC_MAN_FILES} DESTINATION "${DATAROOTDIR}/man/man1")
install(DIRECTORY "${PURCMC_DOC_DIR}/hints" DESTINATION "${DATADIR}")
install(FILES ${PurcMC_HELP_FILES} DESTINATION "${DATAROOTDIR}/help")

