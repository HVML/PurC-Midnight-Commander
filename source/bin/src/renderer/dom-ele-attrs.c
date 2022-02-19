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
#include "lib/event.h"          /* mc_event_raise() */

#include "src/setup.h"          /* panels_options */
#include "src/keymap.h"

#include "dom-tree.h"           /* select_element_hook */
#include "dom-ele-attrs.h"

/*** global variables ********************************************************/

/*** file scope macro definitions ********************************************/

#define FIELD_WIDTH_NAME    (w->cols / 3)
#define CONST_STR_UNDEFINED "@undefined"
#define CONST_STR_PUBLIC    "public"
#define CONST_STR_SYSTEM    "system"

#define tlines(t) (WIDGET (t)->lines - 5)

/*** file scope type declarations ********************************************/

struct WEleAttrs
{
    Widget widget;
    pcdom_node_t *node;

    int     nr_attrs;
    int     topmost;
    int     selected;
};

/*** file scope variables ****************************************************/

/*** file scope functions ****************************************************/

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

    if (widget_get_state (w, WST_FOCUSED))
        tty_setcolor (SELECTED_COLOR);
    widget_gotoyx (w, 0, (w->cols - width - 2) / 2);
    tty_printf (" %s ", label);
    tty_setcolor (NORMAL_COLOR);

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

    assert (attrs->node && attrs->node->type == PCDOM_NODE_TYPE_DOCUMENT_TYPE);

    tty_setcolor (NORMAL_COLOR);

    doctype = pcdom_interface_document_type (attrs->node);

    /* Print only lines which fit */
    const gchar *str;
    size_t len;

    /* system identifier */
    widget_gotoyx (w, 3, 1);
    tty_print_string (str_fit_to_term (CONST_STR_SYSTEM,
            FIELD_WIDTH_NAME - 2, J_RIGHT_FIT));

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
    widget_gotoyx (w, 4, 1);
    tty_print_string (str_fit_to_term (CONST_STR_PUBLIC,
            FIELD_WIDTH_NAME - 2, J_RIGHT_FIT));

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
    int i, y;

    assert (attrs->node && attrs->node->type == PCDOM_NODE_TYPE_ELEMENT);

    tty_setcolor (NORMAL_COLOR);

    buff = g_string_new ("");
    element = (pcdom_element_t *)attrs->node;
    attr = pcdom_element_first_attribute(element);
    y = 3;
    i = 0;

    /* Print only lines which fit */
    while (attr) {

        const gchar *str;
        size_t sz;
        bool selected;

        if (i < attrs->topmost) {
            goto inc;
        }

        selected = widget_get_state (w, WST_FOCUSED) && i == attrs->selected;
        tty_setcolor (selected ? SELECTED_COLOR : NORMAL_COLOR);

        str = (const gchar *)pcdom_attr_local_name (attr, &sz);
        buff = g_string_overwrite_len (buff, 0, str, sz);
        widget_gotoyx (w, y, 1);
        tty_print_string (str_fit_to_term (buff->str,
                FIELD_WIDTH_NAME - 2, J_RIGHT_FIT));
        g_string_set_size (buff, 0);

        if (selected) {
            /* pad the gap */
            widget_gotoyx (w, y, FIELD_WIDTH_NAME - 1);
            tty_print_string ("  ");
        }

        str = (const gchar *)pcdom_attr_value (attr, &sz);
        buff = g_string_overwrite_len (buff, 0, str, sz);
        widget_gotoyx (w, y, FIELD_WIDTH_NAME + 1);
        tty_print_string (str_fit_to_term (buff->str,
                w->cols - FIELD_WIDTH_NAME - 2, J_LEFT_FIT));
        g_string_set_size (buff, 0);

        y++;
        if (y >= w->lines - 1)
            break;

inc:
        attr = pcdom_element_next_attribute (attr);
        i++;
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

static void
domattrs_hook (void *data, void *info)
{
    WEleAttrs *attrs = (WEleAttrs *) data;
    if (attrs->node != info) {
        attrs->node = info;

        attrs->nr_attrs = 0;
        attrs->topmost = 0;
        attrs->selected = -1;

        if (attrs->node) {
            if (attrs->node->type == PCDOM_NODE_TYPE_DOCUMENT_TYPE) {
                attrs->nr_attrs = 2;
            }
            else if (attrs->node->type == PCDOM_NODE_TYPE_ELEMENT) {
                pcdom_element_t *element;
                pcdom_attr_t *attr;

                element = pcdom_interface_element (attrs->node);

                attr = pcdom_element_first_attribute (element);
                while (attr) {
                    attrs->nr_attrs++;
                    attr = pcdom_element_next_attribute (attr);
                }

                if (attrs->nr_attrs > 0) {
                    attrs->selected = 0;
                }
            }
        }
    }

    domattrs_show_attrs (attrs);
}

static bool
domattrs_move_backward (WEleAttrs * attrs, int n)
{
    int new_selected;

    if (attrs->nr_attrs < 2)
        return false;

    if (n > attrs->selected) {
        new_selected = 0;
    }
    else {
        new_selected = attrs->selected - n;
    }

    if (new_selected != attrs->selected)
        attrs->selected = new_selected;
    else
        return false;

    if (attrs->selected < attrs->topmost)
        attrs->topmost = attrs->selected;

    return true;
}

static bool
domattrs_move_forward (WEleAttrs * attrs, int n)
{
    int new_selected;

    if (attrs->nr_attrs < 2)
        return false;

    if ((n + attrs->selected) >= attrs->nr_attrs) {
        new_selected = attrs->nr_attrs - 1;
    }
    else {
        new_selected = attrs->selected + n;
    }

    if (new_selected != attrs->selected)
        attrs->selected = new_selected;
    else
        return false;

    if (attrs->selected - attrs->topmost > tlines(attrs)) {
        attrs->topmost = attrs->selected - tlines(attrs);
    }

    return true;
}

static bool
domattrs_move_to_top (WEleAttrs * attrs)
{
    if (attrs->nr_attrs < 2)
        return false;

    if (attrs->selected != 0 || attrs->topmost != 0) {
        attrs->selected = 0;
        attrs->topmost = 0;
        return true;
    }

    return false;
}

static bool
domattrs_move_to_bottom (WEleAttrs * attrs)
{
    int new_selected;

    if (attrs->nr_attrs < 2)
        return false;

    new_selected = attrs->nr_attrs - 1;
    if (attrs->selected != new_selected) {
        attrs->selected = new_selected;

        if (new_selected >= tlines(attrs)) {
            attrs->topmost = new_selected - tlines(attrs);
        }

        return true;
    }

    return false;
}

static inline void
domattrs_move_up (WEleAttrs * attrs)
{
    if (domattrs_move_backward (attrs, 1))
        domattrs_show_attrs (attrs);
}

static inline void
domattrs_move_down (WEleAttrs * attrs)
{
    if (domattrs_move_forward (attrs, 1))
        domattrs_show_attrs (attrs);
}

static inline void
domattrs_move_home (WEleAttrs * attrs)
{
    if (domattrs_move_to_top (attrs))
        domattrs_show_attrs (attrs);
}

static inline void
domattrs_move_end (WEleAttrs * attrs)
{
    if (domattrs_move_to_bottom (attrs))
        domattrs_show_attrs (attrs);
}

static void
domattrs_move_pgup (WEleAttrs * attrs)
{
    if (domattrs_move_backward (attrs, tlines (attrs) - 1))
        domattrs_show_attrs (attrs);
}

static void
domattrs_move_pgdn (WEleAttrs * attrs)
{
    if (domattrs_move_forward (attrs, tlines (attrs) - 1))
        domattrs_show_attrs (attrs);
}

static void
domattrs_change_current (WEleAttrs * attrs)
{
}

static void
domattrs_delete_current (WEleAttrs * attrs)
{
}

static cb_ret_t
domattrs_execute_cmd (WEleAttrs * attrs, long command)
{
    cb_ret_t res = MSG_HANDLED;

    switch (command) {
    case CK_Help:
        {
            ev_help_t event_data = { NULL, "[DOM Element Attributes]" };
            mc_event_raise (MCEVENT_GROUP_CORE, "help", &event_data);
        }
        break;

    case CK_Up:
        domattrs_move_up (attrs);
        break;
    case CK_Down:
        domattrs_move_down (attrs);
        break;
    case CK_Top:
        domattrs_move_home (attrs);
        break;
    case CK_Bottom:
        domattrs_move_end (attrs);
        break;
    case CK_PageUp:
        domattrs_move_pgup (attrs);
        break;
    case CK_PageDown:
        domattrs_move_pgdn (attrs);
        break;
    case CK_Enter:
        domattrs_change_current (attrs);
        break;
    case CK_Search:
        // domattrs_start_search (attrs);
        break;
    case CK_Delete:
        domattrs_delete_current (attrs);
        break;

    case CK_Quit:
        dlg_run_done (DIALOG (WIDGET (attrs)->owner));
        return res;

    default:
        res = MSG_NOT_HANDLED;
    }

    domattrs_show_attrs (attrs);
    return res;
}

static cb_ret_t
domattrs_key (WEleAttrs * attrs, int key)
{
    long command;

    if (is_abort_char (key)) {
        /* modal tree dialog: let upper layer see the
           abort character and close the dialog */
        return MSG_NOT_HANDLED;
    }

    command = widget_lookup_key (WIDGET (attrs), key);
    switch (command)
    {
    case CK_IgnoreKey:
        break;
    case CK_Left:
    case CK_Right:
        return MSG_NOT_HANDLED;
    default:
        return domattrs_execute_cmd (attrs, command);
    }

    return MSG_NOT_HANDLED;
}

static cb_ret_t
domattrs_callback (Widget * w, Widget * sender, widget_msg_t msg,
        int parm, void *data)
{
    WEleAttrs *attrs = (WEleAttrs *) w;
    WDialog *h = DIALOG (w->owner);
    WButtonBar *b;

    switch (msg) {
    case MSG_INIT:
        add_hook (&select_element_hook, domattrs_hook, attrs);
        attrs->node = NULL;
        return MSG_HANDLED;

    case MSG_DRAW:
        domattrs_hook (attrs, attrs->node);
        if (widget_get_state (w, WST_FOCUSED)) {
            b = find_buttonbar (h);
            widget_draw (WIDGET (b));
        }
        return MSG_HANDLED;

    case MSG_FOCUS:
        b = find_buttonbar (h);
        buttonbar_set_label (b, 1, Q_ ("ButtonBar|Help"), w->keymap, w);
        buttonbar_clear_label (b, 2, w);
        buttonbar_clear_label (b, 3, w);
        buttonbar_clear_label (b, 4, w);
        buttonbar_clear_label (b, 5, w);
        buttonbar_set_label (b, 6, Q_ ("ButtonBar|Change"), w->keymap, w);
        buttonbar_set_label (b, 7, Q_ ("ButtonBar|New"), w->keymap, w);
        buttonbar_set_label (b, 8, Q_ ("ButtonBar|Delete"), w->keymap, w);
        buttonbar_clear_label (b, 9, w);
        buttonbar_clear_label (b, 10, w);
        return MSG_HANDLED;

    case MSG_UNFOCUS:
        return MSG_HANDLED;

    case MSG_KEY:
        return domattrs_key (attrs, parm);

    case MSG_ACTION:
        /* command from buttonbar */
        return domattrs_execute_cmd (attrs, parm);

    case MSG_DESTROY:
        delete_hook (&select_element_hook, domattrs_hook);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/*** Mouse callback */
static void
domattrs_mouse_callback (Widget * w, mouse_msg_t msg, mouse_event_t * event)
{
    WEleAttrs *attrs = (WEleAttrs *) w;
    int y;

    y = event->y;
    y--;

    switch (msg) {
    case MSG_MOUSE_DOWN:
        /* rest of the upper frame - call menu */
        if (event->y == WIDGET (w->owner)->y) {
            /* return MOU_UNHANDLED */
            event->result.abort = TRUE;
        }
        break;

    case MSG_MOUSE_CLICK:
        {
            int lines;

            lines = tlines (attrs);

            if (y < 0) {
                if (domattrs_move_backward (attrs, lines - 1))
                    domattrs_show_attrs (attrs);
            }
            else if (y >= lines) {
                if (domattrs_move_forward (attrs, lines - 1))
                    domattrs_show_attrs (attrs);
            }
            else if ((event->count & GPM_DOUBLE) != 0) {
                // TODO
            }
        }
        break;

    case MSG_MOUSE_SCROLL_UP:
    case MSG_MOUSE_SCROLL_DOWN:
        /* TODO: Ticket #2218 */
        break;

    default:
        break;
    }
}

/*** public functions ********************************************************/

WEleAttrs *
dom_ele_attrs_new (int y, int x, int lines, int cols)
{
    WEleAttrs *attrs;
    Widget *w;

    attrs = g_new0 (struct WEleAttrs, 1);
    w = WIDGET (attrs);
    widget_init (w, y, x, lines, cols, domattrs_callback,
            domattrs_mouse_callback);
    w->options |= WOP_SELECTABLE;
    w->keymap = tree_map;

    return attrs;
}

