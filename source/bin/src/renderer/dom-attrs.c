/*
   DOM element attributes widget.

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
 * \file dom-attrs.c
 * \brief Source: DOM element attributes widget
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <inttypes.h>           /* PRIuMAX */

#include <purc/purc-edom.h>

#include "lib/global.h"
#include "lib/unixcompat.h"
#include "lib/tty/tty.h"
#include "lib/tty/key.h"        /* is_idle() */
#include "lib/skin.h"
#include "lib/strutil.h"
#include "lib/timefmt.h"        /* file_date() */
#include "lib/util.h"
#include "lib/widget.h"

#include "src/setup.h"          /* panels_options */

#include "dom-attrs.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

/*** file scope type declarations ****************************************************************/

struct WDOMAttrs
{
    Widget widget;
    int bol;        /* the begin of the line */
    pcedom_node_t *node;
};

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
domattrs_caption (WDOMAttrs * attrs)
{
    Widget *w = WIDGET (attrs);

    const char *label = _("Attributes");
    int len = str_term_width1 (label);

    tty_set_normal_attrs ();
    tty_setcolor (NORMAL_COLOR);
    widget_erase (w);
    tty_draw_box (w->y, w->x, w->lines, w->cols, FALSE);

    widget_gotoyx (w, 0, (w->cols - len - 2) / 2);
    tty_printf (" %s ", label);

    widget_gotoyx (w, 2, 0);
    tty_print_alt_char (ACS_LTEE, FALSE);
    widget_gotoyx (w, 2, w->cols - 1);
    tty_print_alt_char (ACS_RTEE, FALSE);
    tty_draw_hline (w->y + 2, w->x + 1, ACS_HLINE, w->cols - 2);

    tty_setcolor (MARKED_COLOR);

    label = _("Name");
    len = str_term_width1 (label);
    widget_gotoyx (w, 1, (w->cols / 2 - len - 2) / 2);
    tty_print_string (label);

    label = _("Value");
    len = str_term_width1 (label);
    widget_gotoyx (w, 1, (w->cols - len - 2) / 2);
    tty_print_string (label);
}

/* --------------------------------------------------------------------------------------------- */

static void
domattrs_show_attrs (WDOMAttrs * attrs)
{
    Widget *w = WIDGET (attrs);
    GString *buff;

    pcedom_element_t *element;
    pcedom_attr_t *attr;
    int y;

    if (!is_idle ())
        return;

    domattrs_caption (attrs);

    if (!attrs->node || attrs->node->type != PCEDOM_NODE_TYPE_ELEMENT)
        return;

    tty_setcolor (NORMAL_COLOR);

    buff = g_string_new ("");
    element = (pcedom_element_t *)attrs->node;
    attr = pcedom_element_first_attribute(element);
    y = 1;

    /* Print only lines which fit */
    while (attr) {

        if (y >= attrs->bol) {
            const gchar *str;
            size_t sz;

            str = (const gchar *)pcedom_attr_local_name (attr, &sz);
            buff = g_string_overwrite_len (buff, 0, str, sz);
            widget_gotoyx (w, y, 3);
            tty_print_string (buff->str);
            g_string_set_size (buff, 0);

            str = (const gchar *)pcedom_attr_value (attr, &sz);
            buff = g_string_overwrite_len (buff, 0, str, sz);
            widget_gotoyx (w, y, 3 + w->cols / 2);
            tty_print_string (buff->str);
            g_string_set_size (buff, 0);
        }

        y++;
        if (y >= w->lines)
            break;

        attr = pcedom_element_next_attribute(attr);
    }

    g_string_free (buff, TRUE);
}

/* --------------------------------------------------------------------------------------------- */

static void
domattrs_hook (void *data)
{
    WDOMAttrs *attrs = (WDOMAttrs *) data;
    Widget *other_widget;

    other_widget = get_panel_widget (get_current_index ());
    if (!other_widget)
        return;
    if (widget_overlapped (WIDGET (attrs), other_widget))
        return;

    // attrs->ready = TRUE;
    domattrs_show_attrs (attrs);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
domattrs_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WDOMAttrs *attrs = (WDOMAttrs *) w;

    switch (msg)
    {
    case MSG_INIT:
        add_hook (&select_file_hook, domattrs_hook, attrs);
        attrs->node = NULL;
        return MSG_HANDLED;

    case MSG_DRAW:
        domattrs_hook (attrs);
        return MSG_HANDLED;

    case MSG_DESTROY:
        delete_hook (&select_file_hook, domattrs_hook);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WDOMAttrs *
domattrs_new (int y, int x, int lines, int cols)
{
    WDOMAttrs *attrs;
    Widget *w;

    attrs = g_new (struct WDOMAttrs, 1);
    w = WIDGET (attrs);
    widget_init (w, y, x, lines, cols, domattrs_callback, NULL);

    return attrs;
}

/* --------------------------------------------------------------------------------------------- */
