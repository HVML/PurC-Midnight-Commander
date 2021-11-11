set_property(DIRECTORY . PROPERTY FOLDER "misc")

include(GlobalCommon)

configure_file(mc.ext.in ${PurcMC_DERIVED_SOURCES_DIR}/misc/mc.ext @ONLY)
configure_file(mc.menu.in ${PurcMC_DERIVED_SOURCES_DIR}/misc/mc.menu @ONLY)
configure_file(mc.charsets.in ${PurcMC_DERIVED_SOURCES_DIR}/misc/mc.charsets @ONLY)
configure_file(mcedit.menu.in ${PurcMC_DERIVED_SOURCES_DIR}/misc/mcedit.menu @ONLY)

# TODO: List files should be installed to system here.
set(PurcMC_CONF_FILES
    "${PurcMC_DERIVED_SOURCES_DIR}/misc/mc.ext"
    "${PurcMC_DERIVED_SOURCES_DIR}/misc/mc.menu"
    "${PurcMC_DERIVED_SOURCES_DIR}/misc/mc.charsets"
    "${PurcMC_DERIVED_SOURCES_DIR}/misc/mcedit.menu"
    "${PURCMC_MISC_DIR}/edit.indent.rc"
    "${PURCMC_MISC_DIR}/filehighlight.ini"
    "${PURCMC_MISC_DIR}/mc.default.keymap"
    "${PURCMC_MISC_DIR}/mc.emacs.keymap"
    "${PURCMC_MISC_DIR}/mc.keymap"
    "${PURCMC_MISC_DIR}/sfs.ini"
)

install(FILES ${PurcMC_CONF_FILES} DESTINATION "${SYSCONFDIR}")
install(DIRECTORY "${PURCMC_MISC_DIR}/skins" DESTINATION "${DATADIR}")
install(DIRECTORY "${PURCMC_MISC_DIR}/syntax" DESTINATION "${DATADIR}")
