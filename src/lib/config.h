/**
 * @file config.h
 * @author Vincent Wei (https://github.com/VincentWei)
 * @date 2021/11/05
 * @brief The configuration header file for UE (udom-editor).
 *
 * Copyright (C) 2021 FMSoft <https://www.fmsoft.cn>
 *
 * This file is a part of UE (short for eDOM Editor), a uDOM renderer
 * in text-mode.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H && defined(BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#endif

#include <wtf/Platform.h>
#include <wtf/ExportMacros.h>

#define PACKAGE                     "purc-midnight-commander"
#define SEARCH_TYPE_GLIB            1
#define SIG_ATOMIC_VOLATILE_T       sig_atomic_t

#if HAVE(MODE_T_LT_INT)
#define PROMOTED_MODE_T             int
#else
#define PROMOTED_MODE_T             mode_t
#endif

/* Undefine the macros for macros defined as 0 in cmakeconfig.h */
#if !HAVE(ARPA_INET_H)
#undef HAVE_ARPA_INET_H
#endif
#if !HAVE(FCNTL_H)
#undef HAVE_FCNTL_H
#endif
#if !HAVE(LIBUTIL_H)
#undef HAVE_LIBUTIL_H
#endif
#if !HAVE(LINUX_FS_H)
#undef HAVE_LINUX_FS_H
#endif
#if !HAVE(MEMORY_H)
#undef HAVE_MEMORY_H
#endif
#if !HAVE(NCURSESW_CURSES_H)
#undef HAVE_NCURSESW_CURSES_H
#endif
#if !HAVE(NCURSES_CURSES_H)
#undef HAVE_NCURSES_CURSES_H
#endif
#if !HAVE(NCURSES_H)
#undef HAVE_NCURSES_H
#endif
#if !HAVE(NCURSES_HCURSES_H)
#undef HAVE_NCURSES_HCURSES_H
#endif
#if !HAVE(NCURSES_NCURSES_H)
#undef HAVE_NCURSES_NCURSES_H
#endif
#if !HAVE(NCURSES_TERM_H)
#undef HAVE_NCURSES_TERM_H
#endif
#if !HAVE(OS_H)
#undef HAVE_OS_H
#endif
#if !HAVE(PTY_H)
#undef HAVE_PTY_H
#endif
#if !HAVE(STRING_H)
#undef HAVE_STRING_H
#endif
#if !HAVE(STROPTS_H)
#undef HAVE_STROPTS_H
#endif
#if !HAVE(SYS_FS_S5PARAM_H)
#undef HAVE_SYS_FS_S5PARAM_H
#endif
#if !HAVE(SYS_FS_TYPES_H)
#undef HAVE_SYS_FS_TYPES_H
#endif
#if !HAVE(SYS_IOCTL_H)
#undef HAVE_SYS_IOCTL_H
#endif
#if !HAVE(SYS_MNTENT_H)
#undef HAVE_SYS_MNTENT_H
#endif
#if !HAVE(SYS_MOUNT_H)
#undef HAVE_SYS_MOUNT_H
#endif
#if !HAVE(SYS_PARAM_H)
#undef HAVE_SYS_PARAM_H
#endif
#if !HAVE(SYS_SELECT_H)
#undef HAVE_SYS_SELECT_H
#endif
#if !HAVE(SYS_STATFS_H)
#undef HAVE_SYS_STATFS_H
#endif
#if !HAVE(SYS_STATVFS_H)
#undef HAVE_SYS_STATVFS_H
#endif
#if !HAVE(SYS_UCRED_H)
#undef HAVE_SYS_UCRED_H
#endif
#if !HAVE(SYS_VFS_H)
#undef HAVE_SYS_VFS_H
#endif
#if !HAVE(UTIL_H)
#undef HAVE_UTIL_H
#endif
#if !HAVE(UTIME_H)
#undef HAVE_UTIME_H
#endif

#if !HAVE(ESCDELAY)
#undef HAVE_ESCDELAY
#endif
#if !HAVE(RESIZETERM)
#undef HAVE_RESIZETERM
#endif

#if !HAVE(GETPT)
#undef HAVE_GETPT
#endif
#if !HAVE(GET_PROCESS_STATS)
#undef HAVE_GET_PROCESS_STATS
#endif
#if !HAVE(GRANTPT)
#undef HAVE_GRANTPT
#endif
#if !HAVE(MMAP)
#undef HAVE_MMAP
#endif
#if !HAVE(OPENPTY)
#undef HAVE_OPENPTY
#endif
#if !HAVE(POSIX_FALLOCATE)
#undef HAVE_POSIX_FALLOCATE
#endif
#if !HAVE(POSIX_OPENPT)
#undef HAVE_POSIX_OPENPT
#endif
#if !HAVE(REALPATH)
#undef HAVE_REALPATH
#endif
#if !HAVE(SETLOCALE)
#undef HAVE_SETLOCALE
#endif
#if !HAVE(SOCKLEN_T)
#undef HAVE_SOCKLEN_T
#endif
#if !HAVE(STATLSTAT)
#undef HAVE_STATLSTAT
#endif
#if !HAVE(STRCASECMP)
#undef HAVE_STRCASECMP
#endif
#if !HAVE(STRNCASECMP)
#undef HAVE_STRNCASECMP
#endif
#if !HAVE(STRVERSCMP)
#undef HAVE_STRVERSCMP
#endif
#if !HAVE(UTIMENSAT)
#undef HAVE_UTIMENSAT
#endif

#if !HAVE(STRUCT_FSSTAT_F_FSTYPENAME)
#undef HAVE_STRUCT_FSSTAT_F_FSTYPENAME
#endif
#if !HAVE(STRUCT_LINGER_L_LINGER)
#undef HAVE_STRUCT_LINGER_L_LINGER
#endif
#if !HAVE(STRUCT_STATFS_F_FSTYPENAME)
#undef HAVE_STRUCT_STATFS_F_FSTYPENAME
#endif
#if !HAVE(STRUCT_STATFS_F_TYPE)
#undef HAVE_STRUCT_STATFS_F_TYPE
#endif
#if !HAVE(STRUCT_STATVFS_F_BASETYPE)
#undef HAVE_STRUCT_STATVFS_F_BASETYPE
#endif
#if !HAVE(STRUCT_STATVFS_F_FSTYPENAME)
#undef HAVE_STRUCT_STATVFS_F_FSTYPENAME
#endif
#if !HAVE(STRUCT_STATVFS_F_TYPE)
#undef HAVE_STRUCT_STATVFS_F_TYPE
#endif
#if !HAVE(STRUCT_STAT_ST_BLKSIZE)
#undef HAVE_STRUCT_STAT_ST_BLKSIZE
#endif
#if !HAVE(STRUCT_STAT_ST_BLOCKS)
#undef HAVE_STRUCT_STAT_ST_BLOCKS
#endif
#if !HAVE(STRUCT_STAT_ST_MTIM)
#undef HAVE_STRUCT_STAT_ST_MTIM
#endif
#if !HAVE(STRUCT_STAT_ST_RDEV)
#undef HAVE_STRUCT_STAT_ST_RDEV
#endif

/* enable/disable features managed by cmake */
#if !ENABLE(VFS_CPIO)
#undef ENABLE_VFS_CPIO
#endif
#if !ENABLE(VFS_EXTFS)
#undef ENABLE_VFS_EXTFS
#endif
#if !ENABLE(VFS_FTP)
#undef ENABLE_VFS_FTP
#endif
#if !ENABLE(VFS_NET)
#undef ENABLE_VFS_NET
#endif
#if !ENABLE(VFS_SFS)
#undef ENABLE_VFS_SFS
#endif
#if !ENABLE(VFS_SFTP)
#undef ENABLE_VFS_SFTP
#endif
#if !ENABLE(VFS_UNDELFS)
#undef ENABLE_VFS_UNDELFS
#endif
#if !ENABLE(VFS_TAR)
#undef ENABLE_VFS_TAR
#endif
#if !ENABLE(VFS_FISH)
#undef ENABLE_VFS_FISH
#endif

/* enable/disable features managed mannually */
#define ENABLE_BACKGROUND   1
#define ENABLE_SUBSHELL     1
#define ENABLE_VFS          1

#undef ENABLE_NLS
#undef ENABLE_EXT2FS_ATTR

#undef HAVE_QNX_KEYS
#undef HAVE_TEXTMODE_X11_SUPPORT
#undef HAVE_CHARSET
#undef HAVE_SLANG
#undef HAVE_LIBGPM

#undef HAVE_ASPELL
#undef HAVE_AWK
#undef HAVE_HEAD
#undef HAVE_PERL
#undef HAVE_SED
#undef HAVE_TAIL
#undef HAVE_ZIPINFO
#undef HAVE_INFOMOUNT
#undef HAVE_INFOMOUNT_LIST
#undef HAVE_INFOMOUNT_QNX

#undef HAVE_DATE_MDYT
#undef HAVE_HASMNTOPT
#undef STAT_STATVFS
#undef STAT_STATVFS64

#define USE_INTERNAL_EDIT                   1
#define USE_DRAG                            1
#define USE_CLICK                           1
#define USE_DIFF_VIEW                       1
#define USE_GNU                             1
#define USE_NCURSES                         1
#define USE_XTERM                           1
#define USE_XTERM_BUTTON_EVENT_TRACKING     1
#define USE_XTERM_NORMAL_TRACKING           1
#define USE_DOWN                            1
#define USE_UP                              1
#define USE_SCROLL_DOWN                     1
#define USE_SCROLL_UP                       1

#undef USE_ANONYMOUS
#undef USE_DEPRECATED_PARSER
#undef USE_DISABLED
#undef USE_FILE_CMD
#undef USE_GPM
#undef USE_H
#undef USE_MAGIC
#undef USE_MAINTAINER_MODE
#undef USE_MEMSET_IN_LCS
#undef USE_MOVE
#undef USE_NCURSESW
#undef USE_NONE
#undef USE_QNX_TI
#undef USE_SIGV4
#undef USE_STR_UTF8_CREATE_KEY_FOR_FILENAME

