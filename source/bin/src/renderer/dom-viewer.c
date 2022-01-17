/*
   DOM viewer for the PurC Midnight Commander

   Copyright (C) 2022
   Beijing FMSoft Technologies Co., Ltd.

   Written by:
   Vincent Wei <vincent@minigui.org>, 2022

   This file is part of the PurC Midnight Commander (`PurCMC` for short).

   PurCMC is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   PurCMC is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <errno.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/util.h"           /* load_file_position() */
#include "lib/widget.h"

#include "src/filemanager/layout.h"
#include "src/filemanager/filemanager.h"
#include "src/viewer/mcviewer.h"

#include "dom-tree.h"
#include "dom-ele-attrs.h"

/*** global variables */

/*** file scope macro definitions */

/*** file scope type declarations */
struct WDOMView {
    Widget widget;
    pcdom_document_t *doc;
};

/*** file scope variables */

/* ------------------------------------------------------------------------- */
/*** file scope functions */
/* ------------------------------------------------------------------------- */

cb_ret_t
domview_dialog_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    // WDialog *h = DIALOG (w);

    switch (msg)
    {
    case MSG_ACTION:
        /* Handle shortcuts. */
        break;

    case MSG_VALIDATE:
        break;

    default:
        break;
    }

    return dlg_default_callback (w, sender, msg, parm, data);
}

static bool
domview_load (WDOMTree *dom_tree, WEleAttrs *ele_attrs, WView *txt_view,
        pcdom_document_t *dom_doc)
{
    return FALSE;
}

/* --------------------------------------------------------------------------------------------- */
/*** public functions ****************************************************************************/
/* --------------------------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------------------------- */
/** Real view only */

bool
domview_viewer (pcdom_document_t *dom_doc)
{
    bool succeeded;
    WDOMTree *dom_tree;
    WEleAttrs *ele_attrs;
    WView *txt_view;
    WDialog *view_dlg;
    Widget *vw, *b;
    WGroup *g;

    /* Create dialog and widgets, put them on the dialog */
    view_dlg = dlg_create (FALSE, 0, 0, 1, 1, WPOS_FULLSCREEN, FALSE, NULL,
            domview_dialog_callback, NULL, "[DOM Tree Viewer]",
            _("DOM Tree Viewer"));
    vw = WIDGET (view_dlg);
    widget_want_tab (vw, TRUE);

    g = GROUP (view_dlg);

    dom_tree = dom_tree_new (vw->y, vw->x, vw->lines - 1, vw->cols / 2, FALSE);
    group_add_widget_autopos (g, dom_tree, WPOS_KEEP_ALL, NULL);

    ele_attrs = dom_ele_attrs_new (vw->y, vw->x, vw->lines - 10, vw->cols / 2);
    group_add_widget_autopos (g, ele_attrs, WPOS_KEEP_ALL, NULL);

    txt_view = mcview_new (vw->y + vw->lines - 10, vw->cols / 2, 10,
            vw->cols / 2, FALSE);
    group_add_widget_autopos (g, txt_view, WPOS_KEEP_ALL, NULL);

    b = WIDGET (buttonbar_new ());
    group_add_widget_autopos (g, b, b->pos_flags, NULL);

    succeeded = domview_load (dom_tree, ele_attrs, txt_view, dom_doc);

    if (succeeded) {
        dlg_run (view_dlg);
        view_dlg->data = dom_doc;
    }
    else
        dlg_stop (view_dlg);

    if (widget_get_state (vw, WST_CLOSED))
        widget_destroy (vw);

    return succeeded;
}

/* {{{ Miscellaneous functions }}} */

/* --------------------------------------------------------------------------------------------- */


/* --------------------------------------------------------------------------------------------- */
