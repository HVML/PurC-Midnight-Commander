/*
 * \file ulog.h
 * \brief Header: define the ULOG_XXX macros.
 */

#ifndef MC_RENDERER_ULOG_H
#define MC_RENDERER_ULOG_H

#include "lib/global.h"
#include "lib/logging.h"

#define ULOG_INFO(fmt, ...) mc_log(fmt, ## __VA_ARGS__)
#define ULOG_NOTE(fmt, ...) mc_log(fmt, ## __VA_ARGS__)
#define ULOG_WARN(fmt, ...) mc_log(fmt, ## __VA_ARGS__)
#define ULOG_ERR(fmt, ...) mc_always_log(fmt, ## __VA_ARGS__)

#endif /* MC_RENDERER_ULOG_H */
