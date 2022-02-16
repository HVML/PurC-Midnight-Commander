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
#include "src/filemanager/ext.h"

#include "dom-viewer.h"

/*** global variables */

/*** file scope macro definitions */

/*** file scope type declarations */

/*** file scope variables */

/*** file scope functions */

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
        pcdom_document_t *doc)
{
    if (dom_tree_load (dom_tree, doc))
        return true;
    return false;
}

static pchtml_html_document_t *
parse_html (const vfs_path_t * filename_vpath)
{
    int fdin = -1;
    ssize_t sz_read;
    char buffer[BUF_1K * 8];

    pchtml_html_parser_t *parser = NULL;
    pchtml_html_document_t *html_doc = NULL;

    fdin = mc_open (filename_vpath, O_RDONLY | O_LINEAR);
    if (fdin == -1)
        goto fail;

    parser = pchtml_html_parser_create();
    if (parser == NULL)
        goto fail;
    pchtml_html_parser_init (parser);

    html_doc = pchtml_html_parse_chunk_begin (parser);
    while ((sz_read = mc_read (fdin, buffer, sizeof (buffer))) > 0) {
        pchtml_html_parse_chunk_process (parser,
                (unsigned char *)buffer, sz_read);
    }

    if (sz_read == -1)
        goto fail;

    pchtml_html_parse_chunk_end (parser);

    mc_close (fdin);
    pchtml_html_parser_destroy (parser);
    return html_doc;

fail:
    if (html_doc) {
        pchtml_html_document_destroy (html_doc);
    }

    if (parser) {
        pchtml_html_parser_destroy (parser);
    }

    if (fdin >= 0)
        mc_close (fdin);

    return NULL;
}

/*** public functions */

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
    view_dlg = dlg_create (FALSE, 0, 0, 1, 1, WPOS_FULLSCREEN, FALSE,
            dialog_colors, domview_dialog_callback, NULL, "[DOM Tree Viewer]",
            _("DOM Tree Viewer"));
    vw = WIDGET (view_dlg);
    widget_want_tab (vw, TRUE);

    g = GROUP (view_dlg);

    int half_cols = vw->cols / 2;
    dom_tree = dom_tree_new (vw->y, vw->x, vw->lines - 1, half_cols, TRUE);
    group_add_widget_autopos (g, dom_tree,
            WPOS_KEEP_LEFT | WPOS_KEEP_VERT, NULL);

    ele_attrs = dom_ele_attrs_new (vw->y, vw->x + half_cols,
            vw->lines - 10, vw->cols - half_cols);
    group_add_widget_autopos (g, ele_attrs,
            WPOS_KEEP_RIGHT | WPOS_KEEP_TOP, NULL);

    txt_view = mcview_new (vw->y + vw->lines - 10, vw->x + vw->cols / 2,
            9, vw->cols - half_cols, FALSE, _("Content"));
    group_add_widget_autopos (g, txt_view,
            WPOS_KEEP_RIGHT | WPOS_KEEP_BOTTOM, NULL);

    b = WIDGET (buttonbar_new ());
    group_add_widget_autopos (g, b, b->pos_flags, NULL);

    succeeded = domview_load (dom_tree, ele_attrs, txt_view, dom_doc);

    if (succeeded) {
        WDOMViewInfo info = { dom_doc, dom_tree, ele_attrs, txt_view };

        view_dlg->data = &info;
        dlg_run (view_dlg);
    }
    else
        dlg_stop (view_dlg);

    if (widget_get_state (vw, WST_CLOSED))
        widget_destroy (vw);

    return succeeded;
}

bool
domview_load_html (const vfs_path_t * file_vpath)
{
    pchtml_html_document_t *html_doc = NULL;
    char *mime;

    mime = get_file_mime_type (file_vpath);
    if (mime && strcmp (mime, "text/html") == 0) {
        if ((html_doc = parse_html (file_vpath))) {
            domview_viewer (pcdom_interface_document (html_doc));
            pchtml_html_document_destroy (html_doc);
        }
    }

    if (mime)
        g_free (mime);

    return html_doc != NULL;
}

