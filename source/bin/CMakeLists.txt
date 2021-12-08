set_property(DIRECTORY . PROPERTY FOLDER "bin")

include(GlobalCommon)
include(target/PurcMC)
include(MyMacros.cmake)

configure_file(man2hlp/man2hlp.in ${CMAKE_BINARY_DIR}/man2hlp
#        FILE_PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ
        @ONLY)

PURCMC_EXECUTABLE_DECLARE(purcmc)

list(APPEND purcmc_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${PurcMC_DERIVED_SOURCES_DIR}"
    "${PURCMC_LIB_DIR}"
    "${PURCMC_BIN_DIR}"
    "${PURCMC_BIN_DIR}/vfs-plugins"
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

if (ENABLE_VFS_CPIO)
    list(APPEND purcmc_SOURCES
        "${PURCMC_BIN_DIR}/vfs-plugins/cpio/cpio.h"
        "${PURCMC_BIN_DIR}/vfs-plugins/cpio/cpio.c"
    )
endif ()

if (ENABLE_VFS_EXTFS)
    list(APPEND purcmc_SOURCES
        "${PURCMC_BIN_DIR}/vfs-plugins/extfs/extfs.h"
        "${PURCMC_BIN_DIR}/vfs-plugins/extfs/extfs.c"
    )
endif ()

if (ENABLE_VFS_FTP)
    list(APPEND purcmc_SOURCES
        "${PURCMC_BIN_DIR}/vfs-plugins/ftpfs/ftpfs.h"
        "${PURCMC_BIN_DIR}/vfs-plugins/ftpfs/ftpfs.c"
        "${PURCMC_BIN_DIR}/vfs-plugins/ftpfs/ftpfs_parse_ls.c"
    )
endif ()

if (ENABLE_VFS_SFS)
    list(APPEND purcmc_SOURCES
        "${PURCMC_BIN_DIR}/vfs-plugins/sfs/sfs.h"
        "${PURCMC_BIN_DIR}/vfs-plugins/sfs/sfs.c"
    )
endif ()

if (ENABLE_VFS_SFTP)
    list(APPEND purcmc_SOURCES
        "${PURCMC_BIN_DIR}/vfs-plugins/sftpfs/init.h"
        "${PURCMC_BIN_DIR}/vfs-plugins/sftpfs/internal.h"
        "${PURCMC_BIN_DIR}/vfs-plugins/sftpfs/config_parser.c"
        "${PURCMC_BIN_DIR}/vfs-plugins/sftpfs/dir.c"
        "${PURCMC_BIN_DIR}/vfs-plugins/sftpfs/init.c"
        "${PURCMC_BIN_DIR}/vfs-plugins/sftpfs/internal.c"
        "${PURCMC_BIN_DIR}/vfs-plugins/sftpfs/vfs_class.c"
        "${PURCMC_BIN_DIR}/vfs-plugins/sftpfs/vfs_subclass.c"
        "${PURCMC_BIN_DIR}/vfs-plugins/sftpfs/connection.c"
        "${PURCMC_BIN_DIR}/vfs-plugins/sftpfs/file.c"
    )
endif ()

if (ENABLE_VFS_UNDELFS)
    list(APPEND purcmc_SOURCES
        "${PURCMC_BIN_DIR}/vfs-plugins/undelfs/undelfs.h"
        "${PURCMC_BIN_DIR}/vfs-plugins/undelfs/undelfs.c"
    )
endif ()

if (ENABLE_VFS_TAR)
    list(APPEND purcmc_SOURCES
        "${PURCMC_BIN_DIR}/vfs-plugins/tar/tar.h"
        "${PURCMC_BIN_DIR}/vfs-plugins/tar/tar.c"
    )
endif ()

if (ENABLE_VFS_FISH)
    list(APPEND purcmc_SOURCES
        "${PURCMC_BIN_DIR}/vfs-plugins/fish/fish.h"
        "${PURCMC_BIN_DIR}/vfs-plugins/fish/fishdef.h"
        "${PURCMC_BIN_DIR}/vfs-plugins/fish/fish.c"
    )
endif ()

set(purcmc_LIBRARIES
    PurcMC::PurcMC
    PurC::PurC
    pthread
)

PURCMC_COMPUTE_SOURCES(purcmc)
PURCMC_FRAMEWORK(purcmc)

install(TARGETS purcmc DESTINATION "${EXEC_INSTALL_DIR}/")
install(CODE "file(CREATE_LINK \"${EXEC_INSTALL_DIR}/purcmc\" \"${EXEC_INSTALL_DIR}/purcmcdiff\" COPY_ON_ERROR SYMBOLIC)")
install(CODE "file(CREATE_LINK \"${EXEC_INSTALL_DIR}/purcmc\" \"${EXEC_INSTALL_DIR}/purcmcview\" COPY_ON_ERROR SYMBOLIC)")
install(CODE "file(CREATE_LINK \"${EXEC_INSTALL_DIR}/purcmc\" \"${EXEC_INSTALL_DIR}/purcmcedit\" COPY_ON_ERROR SYMBOLIC)")

if (WTF_OS_LINUX)
    PURCMC_EXECUTABLE_DECLARE(consaver)
    list(APPEND consaver_PRIVATE_INCLUDE_DIRECTORIES
        "${CMAKE_BINARY_DIR}"
        "${PurcMC_DERIVED_SOURCES_DIR}"
        "${PURCMC_LIB_DIR}"
        "${PURCMC_BIN_DIR}"
    )

    list(APPEND consaver_SYSTEM_INCLUDE_DIRECTORIES
        ${GLIB_INCLUDE_DIRS}
    )

    PURCMC_EXECUTABLE(consaver)

    set(consaver_PLATFORM_INDEPENDENT_DIRS
        "consaver"
    )

    APPEND_ALL_SOURCE_FILES_IN_DIRLIST(consaver_SOURCES "${consaver_PLATFORM_INDEPENDENT_DIRS}")

    list(APPEND consaver_SOURCES
    )

    set(consaver_LIBRARIES
    )

    PURCMC_COMPUTE_SOURCES(consaver)
    PURCMC_FRAMEWORK(consaver)
endif ()

PURCMC_EXECUTABLE_DECLARE(purcsmg)
list(APPEND purcsmg_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${PurcMC_DERIVED_SOURCES_DIR}"
    "${PURCMC_LIB_DIR}"
    "${PURCMC_BIN_DIR}"
)

list(APPEND purcsmg_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

PURCMC_EXECUTABLE(purcsmg)

set(purcsmg_PLATFORM_INDEPENDENT_DIRS
    "purcsmg"
)

APPEND_ALL_SOURCE_FILES_IN_DIRLIST(purcsmg_SOURCES "${purcsmg_PLATFORM_INDEPENDENT_DIRS}")

list(APPEND purcsmg_SOURCES
)

set(purcsmg_LIBRARIES
)

PURCMC_COMPUTE_SOURCES(purcsmg)
PURCMC_FRAMEWORK(purcsmg)
