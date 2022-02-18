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
 * \file dom-ele-attrs.c
 * \brief Source: DOM element attributes widget
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>           /* PRIuMAX */
#include <assert.h>
#include <sys/stat.h>

#include <purc/purc-dom.h>

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

#include "dom-tree.h"           /* select_element_hook */
#include "dom-ele-attrs.h"

/*** global variables ****************************************************************************/

/*** file scope macro definitions ****************************************************************/

#define FIELD_WIDTH_NAME    (w->cols / 3)
#define CONST_STR_UNDEFINED "@undefined"
#define CONST_STR_PUBLIC    "public"
#define CONST_STR_SYSTEM    "system"

/*** file scope type declarations ****************************************************************/

struct WEleAttrs
{
    Widget widget;
    pcdom_node_t *node;
};

/*** file scope variables ************************************************************************/

/*** file scope functions ************************************************************************/
/* --------------------------------------------------------------------------------------------- */

static void
domattrs_caption (WEleAttrs * attrs)
{
    Widget *w = WIDGET (attrs);

    const char *label = _("Attributes");
    int width = str_term_width1 (label);

    tty_set_normal_attrs ();
    tty_setcolor (NORMAL_COLOR);
    widget_erase (w);
    tty_draw_box (w->y, w->x, w->lines, w->cols, FALSE);

    widget_gotoyx (w, 0, (w->cols - width - 2) / 2);
    tty_printf (" %s ", label);

    widget_gotoyx (w, 2, 0);
    tty_print_alt_char (ACS_LTEE, FALSE);
    widget_gotoyx (w, 2, w->cols - 1);
    tty_print_alt_char (ACS_RTEE, FALSE);
    tty_draw_hline (w->y + 2, w->x + 1, ACS_HLINE, w->cols - 2);

    tty_setcolor (MARKED_COLOR);

    label = _("Name");
    width = str_term_width1 (label);
    widget_gotoyx (w, 1, FIELD_WIDTH_NAME - 1 - width);
    tty_print_string (label);

    label = _("Value");
    widget_gotoyx (w, 1, FIELD_WIDTH_NAME + 1);
    tty_print_string (label);
}

/* ------------------------------------------------------------------------- */

static void
domattrs_show_doctype_ids (WEleAttrs * attrs)
{
    Widget *w = WIDGET (attrs);
    GString *buff = g_string_new ("");
    pcdom_document_type_t *doctype;

    assert (attrs->node && attrs->node->type == PCDOM_NODE_TYPE_DOC_TYPE);

    tty_setcolor (NORMAL_COLOR);

    doctype = pcdom_interface_document_type (attrs->node);

    /* Print only lines which fit */
    const gchar *str;
    size_t len;
    int width;

    /* system identifier */
    width = str_term_width1 (CONST_STR_SYSTEM);
    widget_gotoyx (w, 3, FIELD_WIDTH_NAME - 3 - width);
    tty_print_string (CONST_STR_SYSTEM);

    str = (const gchar *)pcdom_document_type_system_id (doctype, &len);
    if (len > 0) {
        buff = g_string_overwrite_len (buff, 0, str, len);
    }
    else {
        buff = g_string_overwrite (buff, 0, CONST_STR_UNDEFINED);
    }
    widget_gotoyx (w, 3, FIELD_WIDTH_NAME + 1);
    tty_print_string (str_fit_to_term (buff->str,
                w->cols - FIELD_WIDTH_NAME - 2, J_LEFT_FIT));
    g_string_set_size (buff, 0);

    /* public identifier */
    width = str_term_width1 (CONST_STR_PUBLIC);
    widget_gotoyx (w, 4, FIELD_WIDTH_NAME - 3 - width);
    tty_print_string (CONST_STR_PUBLIC);

    str = (const gchar *)pcdom_document_type_public_id (doctype, &len);
    if (len > 0) {
        buff = g_string_overwrite_len (buff, 0, str, len);
    }
    else {
        buff = g_string_overwrite (buff, 0, CONST_STR_UNDEFINED);
    }
    widget_gotoyx (w, 4, FIELD_WIDTH_NAME + 1);
    tty_print_string (str_fit_to_term (buff->str,
                w->cols - FIELD_WIDTH_NAME - 2, J_LEFT_FIT));
    g_string_set_size (buff, 0);

    g_string_free (buff, TRUE);
}

static void
domattrs_show_element_attrs (WEleAttrs * attrs)
{
    Widget *w = WIDGET (attrs);
    GString *buff;

    pcdom_element_t *element;
    pcdom_attr_t *attr;
    int y;

    assert (attrs->node && attrs->node->type == PCDOM_NODE_TYPE_ELEMENT);

    tty_setcolor (NORMAL_COLOR);

    buff = g_string_new ("");
    element = (pcdom_element_t *)attrs->node;
    attr = pcdom_element_first_attribute(element);
    y = 3;

    /* Print only lines which fit */
    while (attr) {

        const gchar *str;
        size_t sz;

        str = (const gchar *)pcdom_attr_local_name (attr, &sz);
        buff = g_string_overwrite_len (buff, 0, str, sz);
        widget_gotoyx (w, y, 1);
        tty_print_string (str_fit_to_term (buff->str,
                FIELD_WIDTH_NAME - 2, J_RIGHT_FIT));
        g_string_set_size (buff, 0);

        str = (const gchar *)pcdom_attr_value (attr, &sz);
        buff = g_string_overwrite_len (buff, 0, str, sz);
        widget_gotoyx (w, y, FIELD_WIDTH_NAME + 1);
        tty_print_string (str_fit_to_term (buff->str,
                w->cols - FIELD_WIDTH_NAME - 2, J_LEFT_FIT));
        g_string_set_size (buff, 0);

        y++;
        if (y >= w->lines)
            break;

        attr = pcdom_element_next_attribute (attr);
    }

    g_string_free (buff, TRUE);
}

static void
domattrs_show_attrs (WEleAttrs * attrs)
{
    domattrs_caption (attrs);

    if (!attrs->node)
        return;

    if (attrs->node->type == PCDOM_NODE_TYPE_DOCUMENT_TYPE) {
        domattrs_show_doctype_ids (attrs);
    }
    else if (attrs->node->type == PCDOM_NODE_TYPE_ELEMENT) {
        domattrs_show_element_attrs (attrs);
    }
}

/* --------------------------------------------------------------------------------------------- */

static void
domattrs_hook (void *data, void *info)
{
    WEleAttrs *attrs = (WEleAttrs *) data;
    attrs->node = info;
    domattrs_show_attrs (attrs);
}

/* --------------------------------------------------------------------------------------------- */

static cb_ret_t
domattrs_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WEleAttrs *attrs = (WEleAttrs *) w;

    switch (msg)
    {
    case MSG_INIT:
        add_hook (&select_element_hook, domattrs_hook, attrs);
        attrs->node = NULL;
        return MSG_HANDLED;

    case MSG_DRAW:
        domattrs_hook (attrs, attrs->node);
        return MSG_HANDLED;

    case MSG_DESTROY:
        delete_hook (&select_element_hook, domattrs_hook);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

WEleAttrs *
dom_ele_attrs_new (int y, int x, int lines, int cols)
{
    WEleAttrs *attrs;
    Widget *w;

    attrs = g_new (struct WEleAttrs, 1);
    w = WIDGET (attrs);
    widget_init (w, y, x, lines, cols, domattrs_callback, NULL);

    return attrs;
}

/* --------------------------------------------------------------------------------------------- */
