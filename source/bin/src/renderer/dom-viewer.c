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
#include "lib/kvlist.h"
#include "lib/widget.h"

#include "../filemanager/ext.h" /* get_file_mime_type() */

#include "dom-viewer.h"

/*** global variables */

/*** file scope macro definitions */

/*** file scope type declarations */

/*** file scope variables */

/* the map from file/runner to map */
static struct kvlist file2dom_map = KVLIST_INIT(file2dom_map, NULL);
static WDOMViewInfo view_info;

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

static bool
get_or_load_html_file (const vfs_path_t * vpath)
{
    char *filename = NULL;
    pchtml_html_document_t **data;
    pchtml_html_document_t *html_doc = NULL;

    filename = vfs_path_to_str_flags (vpath, 0, VPF_STRIP_PASSWORD);
    if (filename == NULL)
        goto done;

    data = kvlist_get (&file2dom_map, filename);
    if (data) {
        html_doc = *data;
        goto done;
    }

    html_doc = parse_html (vpath);
    if (html_doc) {
        view_info.file_runner = kvlist_set_ex (&file2dom_map, filename, &html_doc);
        if (view_info.file_runner == NULL) {
            pchtml_html_document_destroy (html_doc);
            html_doc = NULL;
        }
    }

done:
    if (filename)
        free (filename);

    view_info.doc = pcdom_interface_document (html_doc);
    return view_info.doc != NULL;
}

static void
domview_create_dialog (WDOMViewInfo *info)
{
    Widget *vw, *b;
    WGroup *g;

    /* Create dialog and widgets, put them on the dialog */
    info->dlg = dlg_create (FALSE, 0, 0, 1, 1, WPOS_FULLSCREEN, FALSE,
            dialog_colors, domview_dialog_callback, NULL, "[DOM Viewer]",
            _("DOM Viewer"));
    vw = WIDGET (info->dlg);
    g = GROUP (info->dlg);

    info->caption = hline_new (vw->y, vw->x, vw->cols);
    group_add_widget_autopos (g, info->caption,
            WPOS_KEEP_HORZ | WPOS_KEEP_TOP, NULL);

    int left_lines = vw->lines - 1;
    int half_cols = vw->cols / 2;
    int cnt_lines = left_lines / 2 - 2;
    info->dom_tree = dom_tree_new (vw->y, vw->x, left_lines - 1, half_cols, TRUE);
    group_add_widget_autopos (g, info->dom_tree,
            WPOS_KEEP_LEFT | WPOS_KEEP_VERT, NULL);

    info->ele_attrs = dom_ele_attrs_new (vw->y, vw->x + half_cols,
            left_lines - cnt_lines, vw->cols - half_cols);
    group_add_widget_autopos (g, info->ele_attrs,
            WPOS_KEEP_RIGHT | WPOS_KEEP_TOP, NULL);

    info->dom_cnt = dom_content_new (
            vw->y + left_lines - cnt_lines, vw->x + vw->cols / 2,
            cnt_lines - 1, vw->cols - half_cols,
            _("Content"), NULL);
    group_add_widget_autopos (g, info->dom_cnt,
            WPOS_KEEP_RIGHT | WPOS_KEEP_BOTTOM, NULL);

    b = WIDGET (buttonbar_new ());
    group_add_widget_autopos (g, b, b->pos_flags, NULL);
}

static bool
domview_show_dom (void)
{
    bool succeed = false;

    if (view_info.dlg) {
        hline_set_textv (view_info.caption, " %s ", view_info.file_runner);
        succeed = dom_tree_load (view_info.dom_tree, view_info.doc);
    }
    else {
        domview_create_dialog (&view_info);

        succeed = dom_tree_load (view_info.dom_tree, view_info.doc);
        if (succeed) {
            hline_set_textv (view_info.caption, " %s ", view_info.file_runner);
            view_info.dlg->data = &view_info;
            dlg_run (view_info.dlg);
        }
        else {
            dlg_stop (view_info.dlg);
        }

        Widget *vw = WIDGET (view_info.dlg);
        if (widget_get_state (vw, WST_CLOSED)) {
            widget_destroy (vw);
            view_info.dlg = NULL;
        }
    }

    return succeed;
}

/*** public functions */

bool
domview_load_html (const vfs_path_t * file_vpath)
{
    bool succeed;
    char *mime;

    mime = get_file_mime_type (file_vpath);
    if (mime && strcmp (mime, "text/html") == 0) {
        if (get_or_load_html_file (file_vpath)) {
            succeed = domview_show_dom ();
            // pchtml_html_document_destroy (html_doc);
        }
    }

    if (mime)
        g_free (mime);

    return succeed;
}

