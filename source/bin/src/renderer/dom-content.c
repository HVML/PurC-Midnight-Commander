/*
   DOM domcnt widget.

   Copyright (C) 2022
   Beijing FMSoft Technologies Co., Ltd.

   Written by:
   Vincent Wei <vincent@minigui.org>, 2022

   This file is part of the PurC Midnight Commander (`PurCMC` for short).

   The PurCMC is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The PurCMC is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file dom-domcnt.c
 * \brief Source: DOM domcnt widget
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>           /* PRIuMAX */
#include <assert.h>
#include <sys/stat.h>

#include "lib/global.h"
#include "lib/unixcompat.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"        /* is_idle() */
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/timefmt.h"        /* file_date() */
#include "lib/util.h"
#include "lib/widget.h"

#include "dom-content.h"

/*** global variables ********************************************************/

/*** file scope macro definitions ********************************************/

/*** file scope type declarations ********************************************/

struct WDOMContent
{
    Widget widget;
    char *title;

    gunichar *ucs;
    glong     nr_ucs;
};

/*** file scope variables ****************************************************/

/*** file scope functions ****************************************************/
/* ------------------------------------------------------------------------- */

static void
domcnt_caption (WDOMContent * domcnt)
{
    Widget *w = WIDGET (domcnt);
    int width = str_term_width1 (domcnt->title);

    tty_set_normal_attrs ();
    tty_setcolor (NORMAL_COLOR);
    widget_erase (w);
    tty_draw_box (w->y, w->x, w->lines, w->cols, FALSE);

    widget_gotoyx (w, 0, (w->cols - width - 2) / 2);
    tty_printf (" %s ", domcnt->title);
}

/* ------------------------------------------------------------------------- */

static void
domcnt_show_text (WDOMContent * domcnt)
{
}

static void
domcnt_show_content (WDOMContent * domcnt)
{
    domcnt_caption (domcnt);
    if (domcnt->ucs && domcnt->nr_ucs > 0)
        domcnt_show_text (domcnt);
}

static cb_ret_t
domcnt_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WDOMContent *domcnt = (WDOMContent *) w;

    switch (msg)
    {
    case MSG_INIT:
        return MSG_HANDLED;

    case MSG_DRAW:
        domcnt_show_content (domcnt);
        return MSG_HANDLED;

    case MSG_DESTROY:
        if (domcnt->title)
            g_free (domcnt->title);
        if (domcnt->ucs)
            g_free (domcnt->ucs);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/*** public functions ********************************************************/

WDOMContent *
dom_content_new (int y, int x, int lines, int cols, const char *title)
{
    WDOMContent *domcnt;
    Widget *w;

    domcnt = g_new (struct WDOMContent, 1);
    w = WIDGET (domcnt);
    widget_init (w, y, x, lines, cols, domcnt_callback, NULL);

    if (title)
        domcnt->title = g_strdup (title);

    return domcnt;
}

bool
dom_content_load (WDOMContent *domcnt, const gchar *text, gsize len)
{
    if (domcnt->ucs) {
        g_free (domcnt->ucs);
    }

    gchar *tmp = g_utf8_normalize (text, len, G_NORMALIZE_DEFAULT);
    if (tmp) {
        domcnt->ucs = g_utf8_to_ucs4_fast (tmp, -1, &domcnt->nr_ucs);
        g_free (tmp);
    }

    return tmp != NULL;
}

