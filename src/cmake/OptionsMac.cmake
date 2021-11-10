# FIXME: These should line up with versions in Version.xcconfig
set(PURCMC_MAC_VERSION 0.0.1)
set(MACOSX_FRAMEWORK_BUNDLE_VERSION 0.0.1)

find_package(GLIB 2.44.0 REQUIRED COMPONENTS gio gio-unix gmodule)
find_package(Ncurses 5.0 REQUIRED)
#find_package(PurC 0.0.1)
find_package(LibXml2 2.8.0)

PURCMC_OPTION_BEGIN()
# Private options shared with other PurcMC ports. Add options here only if
# we need a value different from the default defined in GlobalFeatures.cmake.

PURCMC_OPTION_DEFAULT_PORT_VALUE(ENABLE_XML PUBLIC OFF)

PURCMC_OPTION_END()

set(PurcMC_PKGCONFIG_FILE ${CMAKE_BINARY_DIR}/src/purcmc/purcmc.pc)

set(PurcMC_LIBRARY_TYPE STATIC)
set(PurcMCTestSupport_LIBRARY_TYPE STATIC)

