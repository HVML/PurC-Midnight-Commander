/*
 * \file hiboxcompat.h
 * \brief Header: compatibility macros for HiBox.
 */

#ifndef MC_HIBOX_COMPAT_H
#define MC_HIBOX_COMPAT_H

#include "lib/global.h"
#include "lib/logging.h"

#define ULOG_INFO(fmt, ...) mc_log(fmt, ## __VA_ARGS__)
#define ULOG_NOTE(fmt, ...) mc_log(fmt, ## __VA_ARGS__)
#define ULOG_WARN(fmt, ...) mc_log(fmt, ## __VA_ARGS__)
#define ULOG_ERR(fmt, ...) mc_always_log(fmt, ## __VA_ARGS__)

#endif /* MC_HIBOX_COMPAT_H */
