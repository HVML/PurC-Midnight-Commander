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
#include <assert.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/vfs/vfs.h"
#include "lib/strutil.h"
#include "lib/util.h"           /* load_file_position() */
#include "lib/kvlist.h"
#include "lib/widget.h"
#include "lib/event.h"          /* mc_event_raise() */

#include "src/filemanager/ext.h" /* get_file_mime_type() */
#include "src/keymap.h"

#include "dom-viewer.h"

/*** global variables */

/*** file scope macro definitions */

#define NOTIF_DOM_CHANGED       100

/*** file scope type declarations */

/*** file scope variables */

/* the map from file/runner to map */
static struct kvlist file2dom_map = KVLIST_INIT(file2dom_map, NULL);
static WDOMViewInfo view_info;

/*** file scope functions */

static cb_ret_t
on_dom_changed (Widget * w, WDOMViewInfo *info)
{
    WDialog *h = DIALOG (w);

    hline_set_textv (info->caption, " %s ", info->file_runner);

    if (dom_tree_load (info->dom_tree, info->dom_doc)) {
        WButtonBar *b;
        b = find_buttonbar (h);

        if (info->file_runner[0] == '@') {
            /* runner */
            buttonbar_set_label (  b, 7,  Q_ ("ButtonBar|Disconnect"), w->keymap, w);
        }
        else {
            /* file */
            buttonbar_set_label (  b, 7,  Q_ ("ButtonBar|Close"), w->keymap, w);
        }

        widget_draw (WIDGET (b));

        return MSG_HANDLED;
    }

    return MSG_NOT_HANDLED;
}

static inline int
get_number_doms (void)
{
    int n = 0;
    struct kvlist *kv = &file2dom_map;
    const char *name;
    void *data;

    kvlist_for_each(kv, name, data) {
        n++;
    }

    return n;
}

static inline unsigned char
get_hotkey (int n)
{
    return (n <= 9) ? '0' + n : 'a' + n - 10;
}

static void
on_switch_command(WDOMViewInfo * info)
{
    const int nr_doms = get_number_doms ();
    int lines, cols;
    Listbox *listbox;

    struct kvlist *kv = &file2dom_map;
    const char *name;
    void *data;
    int i = 0;

    if (nr_doms <= 1) {
        message (D_NORMAL, "DOM Viewer", "There is only one DOM!");
        return;
    }

    lines = MIN ((size_t) (LINES * 2 / 3), nr_doms);
    cols = COLS * 2 / 3;

    listbox = create_listbox_window (lines, cols,
            _("DOM Viewer"), "[DOM selector]");

    kvlist_for_each(kv, name, data) {
        listbox_add_item (listbox->list,
                LISTBOX_APPEND_AT_END, get_hotkey (i++),
                name, (void *)name, FALSE);
    }

    name = run_listbox_with_data (listbox, view_info.file_runner);

    if (name != NULL && strcmp (name, view_info.file_runner)) {

        data = kvlist_get (kv, name);
        pchtml_html_document_t *html_doc = *(pchtml_html_document_t **)data;

        view_info.file_runner = name;
        view_info.dom_doc = pcdom_interface_document (html_doc);

        bool succeed = dom_tree_load (view_info.dom_tree, view_info.dom_doc);
        if (succeed) {
            hline_set_textv (view_info.caption, " %s ", view_info.file_runner);
        }
    }
}

static void
on_reload_command(WDOMViewInfo * info)
{
    message (D_NORMAL, "DOM Viewer", "On reload command");
}

static void
on_saveto_command(WDOMViewInfo * info)
{
    message (D_NORMAL, "DOM Viewer", "On saveto command");
}

static void
on_close_command(WDOMViewInfo * info)
{
    int sel;

    sel = query_dialog (_("Confirmation"),
            (view_info.file_runner[0] == '@') ?
                _("Unload the DOM and shutdown the current renderer instance for the remote runner?") :
                _("Unload the DOM which was originated from a file?"),
            D_NORMAL, 2,
            _("&No"), _("&Yes"));

    if (sel == 0)
        return;

    struct kvlist *kv = &file2dom_map;
    void *data;

    data = kvlist_get (kv, view_info.file_runner);
    if (data) {
        pchtml_html_document_t *html_doc = *(pchtml_html_document_t **)data;
        kvlist_delete (kv, view_info.file_runner);
        pchtml_html_document_destroy (html_doc);

        view_info.file_runner = NULL;
        view_info.dom_doc = NULL;

        const char *name;
        kvlist_for_each(kv, name, data) {
            html_doc = *(pchtml_html_document_t **)data;

            view_info.file_runner = name;
            view_info.dom_doc = pcdom_interface_document (html_doc);

            bool succeed = dom_tree_load (view_info.dom_tree, view_info.dom_doc);
            if (succeed) {
                hline_set_textv (view_info.caption, " %s ", view_info.file_runner);
            }
            break;
        }
    }

    if (view_info.file_runner == NULL) {
        dlg_stop (view_info.dlg);
    }
}

static void
on_quit_command(WDOMViewInfo * info)
{
    int sel;

    sel = query_dialog (_("Confirmation"),
            _("Destroy DOMs or quit quitely?"),
            D_NORMAL, 4,
            _("&Quiet"), _("&Files"), _("&Runners"), _("&All"));

    if (sel == 0)
        return;

    struct kvlist *kv = &file2dom_map;
    const char* name;
    void *next, *data;

    kvlist_for_each_safe (kv, name, next, data) {
        pchtml_html_document_t *html_doc = *(pchtml_html_document_t **)data;

        // Files
        if (name[0] != '@' && (sel & 1)) {
            kvlist_delete (kv, name);
            pchtml_html_document_destroy (html_doc);
        }

        // Runners
        if (name[0] == '@' && (sel & 2)) {
            kvlist_delete (kv, name);
            pchtml_html_document_destroy (html_doc);
        }
    }

    view_info.file_runner = NULL;
    view_info.dom_doc = NULL;
    kvlist_for_each(kv, name, data) {
        pchtml_html_document_t *html_doc = *(pchtml_html_document_t **)data;

        view_info.file_runner = name;
        view_info.dom_doc = pcdom_interface_document (html_doc);
        break;
    }
}

static cb_ret_t
domview_execute_cmd (WDOMViewInfo * info, Widget * sender, long command)
{
    cb_ret_t res = MSG_HANDLED;

    (void) sender;

    switch (command) {
    case CK_Help:
        {
            ev_help_t event_data = { NULL, "[DOM Viewer]" };
            mc_event_raise (MCEVENT_GROUP_CORE, "help", &event_data);
        }
        break;

    case CK_View:
        on_switch_command (info);
        break;

    case CK_Edit:   /* F4 */
        on_reload_command (info);
        break;

    case CK_Copy:   /* F5 */
        on_saveto_command (info);
        break;

    case CK_Delete:
        on_close_command (info);
        break;

    case CK_Quit:
    case CK_Cancel:
        on_quit_command (info);
        dlg_stop (info->dlg);
        break;

    default:
        res = MSG_NOT_HANDLED;
    }

    return res;
}

static cb_ret_t
domview_dialog_callback (Widget * w, Widget * sender,
        widget_msg_t msg, int parm, void *data)
{
    WDialog *h = DIALOG (w);

    switch (msg) {
    case MSG_INIT:
        {
            WButtonBar *b = find_buttonbar (h);
            buttonbar_set_label (  b, 1,  Q_ ("ButtonBar|Help"), w->keymap, w);
            buttonbar_clear_label (b, 2,  w);
            buttonbar_set_label (  b, 3,  Q_ ("ButtonBar|Switch"), w->keymap, w);
            buttonbar_set_label (  b, 4,  Q_ ("ButtonBar|Reload"), w->keymap, w);
            buttonbar_set_label (  b, 5,  Q_ ("ButtonBar|SaveTo"), w->keymap, w);
            buttonbar_clear_label (b, 6,  w);
            buttonbar_clear_label (b, 7,  w);
            buttonbar_set_label (  b, 8,  Q_ ("ButtonBar|Close"), w->keymap, w);
            buttonbar_clear_label (b, 9,  w);
            buttonbar_set_label (  b, 10, Q_ ("ButtonBar|Quit"), w->keymap, w);
        }
        break;

    case MSG_FOCUS:
        {
            WButtonBar *b;
            b = find_buttonbar (h);
            widget_draw (WIDGET (b));
            break;
        }

    case MSG_NOTIFY:
        if (parm == NOTIF_DOM_CHANGED) {
            return on_dom_changed (w, data);
        }
        break;

    case MSG_KEY:
    case MSG_UNHANDLED_KEY:
        {
            cb_ret_t v = MSG_NOT_HANDLED;
            long command;

            if (parm == 0x1B)
                command = CK_Cancel;
            else
                command = widget_lookup_key (w, parm);
            if (command != CK_IgnoreKey)
                v = domview_execute_cmd (&view_info, NULL, command);
            return v;
        }

    case MSG_ACTION:
        /* Handle shortcuts. */
        return domview_execute_cmd (&view_info, sender, parm);

    case MSG_DESTROY:
        view_info.dlg = NULL;
        view_info.caption = NULL;
        return MSG_HANDLED;

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

    view_info.dom_doc = pcdom_interface_document (html_doc);
    return view_info.dom_doc != NULL;
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
    vw->keymap = filemanager_map;

    g = GROUP (info->dlg);
    info->caption = hline_new (vw->y, vw->x, vw->cols);
    group_add_widget_autopos (g, info->caption,
            WPOS_KEEP_HORZ | WPOS_KEEP_TOP, NULL);

    int left_lines = vw->lines - 1;
    int half_cols = vw->cols / 2;
    int cnt_lines = left_lines / 2 - 2;
    info->dom_tree = dom_tree_new (vw->y + 1, vw->x, left_lines - 1, half_cols, TRUE);
    group_add_widget_autopos (g, info->dom_tree,
            WPOS_KEEP_LEFT | WPOS_KEEP_VERT, NULL);

    info->ele_attrs = dom_ele_attrs_new (vw->y + 1, vw->x + half_cols,
            left_lines - cnt_lines, vw->cols - half_cols);
    group_add_widget_autopos (g, info->ele_attrs,
            WPOS_KEEP_RIGHT | WPOS_KEEP_TOP, NULL);

    info->dom_cnt = dom_content_new (
            vw->y + 1 + left_lines - cnt_lines, vw->x + vw->cols / 2,
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
        succeed = send_message (view_info.dlg, NULL,
                MSG_NOTIFY, NOTIF_DOM_CHANGED, &view_info) == MSG_HANDLED;
    }
    else {
        domview_create_dialog (&view_info);

        succeed = dom_tree_load (view_info.dom_tree, view_info.dom_doc);
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
        g_free (mime);
        mime = NULL;

        if (get_or_load_html_file (file_vpath)) {
            succeed = domview_show_dom ();
            // pchtml_html_document_destroy (html_doc);
        }
    }

    if (mime)
        g_free (mime);

    return succeed;
}

