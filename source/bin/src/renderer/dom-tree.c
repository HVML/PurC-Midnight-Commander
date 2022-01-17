/*
   DOM tree widget for the PurC Midnight Commander

   Copyright (C) 2022 Beijing FMSoft Technologies Co., Ltd.

   Written by:
   Vincent Wei, 2022

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
 * \file dom-tree.c
 * \brief Source: DOM tree widget
 */

#include <config.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>

#include "lib/global.h"

#include "lib/tty/tty.h"
#include "lib/tty/key.h"
#include "lib/skin.h"
#include "lib/vfs/vfs.h"
#include "lib/fileloc.h"
#include "lib/strutil.h"
#include "lib/util.h"
#include "lib/widget.h"
#include "lib/event.h"          /* mc_event_raise() */
#include "lib/list.h"           /* struct list_head */

#include "src/setup.h"          /* confirm_delete, panels_options */
#include "src/keymap.h"
#include "src/history.h"
#include "src/filemanager/filemanager.h"        /* change_panel */

#include "dom-tree.h"

/*** global variables */

/*** file scope macro definitions */

#define MC_DEFXPATHLEN      128

#define NF_UNFOLDED         0x0001

#define tlines(t) (t->is_panel ? WIDGET (t)->lines - 2 - \
        (panels_options.show_mini_info ? 2 : 0) : WIDGET (t)->lines)

/*** file scope type declarations */

typedef struct tree_entry {
    struct list_head list;

    int                 sublevel;
    unsigned int        is_close_tag:1;     /* is a close tag? */
    unsigned int        is_self_close:1;    /* is self-close? */
    pcdom_node_t        *node;
} tree_entry;

struct WDOMTree {
    Widget widget;
    pcdom_document_t *doc;

    struct list_head entries;       /* the entry list */
    tree_entry *selected;           /* The currently selected entry */
    tree_entry *topmost;            /* The topmost entry */

    unsigned int nr_entries;    /* number of all entries */

    GString *search_buffer;     /* Current search string */
    GString *xpath_buffer;      /* XPath string */

    bool searching;         /* Are we on searching mode? */
    bool is_panel;          /* panel or plain widget flag */
};

/*** file scope variables */

/*** file scope functions */

static void
get_xpath_to_entry (WDOMTree * tree, tree_entry *entry)
{
    // TODO
    g_string_assign (tree->xpath_buffer, "<XPath>");
}

static void
tree_show_mini_info (WDOMTree * tree, int tree_lines, int tree_cols)
{
    Widget *w = WIDGET (tree);
    int line;

    /* Show mini info */
    if (tree->is_panel) {
        if (!panels_options.show_mini_info)
            return;
        line = tree_lines + 2;
    }
    else
        line = tree_lines + 1;

    if (tree->searching) {
        /* Show search string */
        tty_setcolor (INPUT_COLOR);
        tty_draw_hline (w->y + line, w->x + 1, ' ', tree_cols);
        widget_gotoyx (w, line, 1);
        tty_print_char (PATH_SEP);
        tty_print_string (str_fit_to_term (tree->search_buffer->str,
                    tree_cols - 2, J_LEFT_FIT));
        tty_print_char (' ');
    }
    else {
        /* Show full XPath of selected element */
        const int *colors;

        colors = widget_get_colors (w);
        tty_setcolor (tree->is_panel ? NORMAL_COLOR : colors[DLG_COLOR_NORMAL]);
        tty_draw_hline (w->y + line, w->x + 1, ' ', tree_cols);
        widget_gotoyx (w, line, 1);

        get_xpath_to_entry(tree, tree->selected);
        tty_print_string (str_fit_to_term (tree->xpath_buffer->str,
                    tree_cols, J_LEFT_FIT));
    }
}

static inline int
get_entry_color (const tree_entry *entry)
{
    switch (entry->node->type) {
        case PCDOM_NODE_TYPE_COMMENT:
        case PCDOM_NODE_TYPE_DOCUMENT_TYPE:
            return DISABLED_COLOR;
        case PCDOM_NODE_TYPE_TEXT:
        case PCDOM_NODE_TYPE_CDATA_SECTION:
            return MARKED_COLOR;
        case PCDOM_NODE_TYPE_ELEMENT:
        default:
            break;
    }

    return NORMAL_COLOR;
}

static void
show_entry(const tree_entry *entry, int width, align_crt_t just_mode)
{
    GString *buff = NULL;

    switch (entry->node->type) {
        case PCDOM_NODE_TYPE_COMMENT:
        {
            const unsigned char *name;
            size_t len;
            pcdom_comment_t *comment;

            comment = pcdom_interface_comment(entry->node);
            name = comment->char_data.data.data;
            len = comment->char_data.data.length;

            buff = g_string_new ("<!-- ");
            if (len > 6) {
                g_string_append_len (buff, (const gchar *)name, 6);
                g_string_append (buff, "…");
            }
            else {
                g_string_append (buff, (const gchar *)name);
            }
            g_string_append (buff, " -->");
            break;
        }

        case PCDOM_NODE_TYPE_DOCUMENT_TYPE:
        {
            const unsigned char *name;
            size_t len;
            pcdom_document_type_t *doctype;

            doctype = pcdom_interface_document_type(entry->node);
            name = pcdom_document_type_name (doctype, &len);

            buff = g_string_new ("<!DOCTYPE");
            if (len > 0) {
                g_string_append_c (buff, ' ');
                g_string_append_len (buff, (const gchar *)name, len);
            }
            g_string_append_c (buff, '>');
            break;
        }

        case PCDOM_NODE_TYPE_TEXT:
        {
            const unsigned char *name;
            size_t len;
            pcdom_text_t *text;

            text = pcdom_interface_text(entry->node);
            name = text->char_data.data.data;
            len = text->char_data.data.length;

            buff = g_string_new ("“");
            if (len > 6) {
                g_string_append_len (buff, (const gchar *)name, 6);
                g_string_append (buff, "…");
            }
            else {
                g_string_append (buff, (const gchar *)name);
            }
            g_string_append (buff, "”");
            break;
        }

        case PCDOM_NODE_TYPE_CDATA_SECTION:
        {
            const unsigned char *name;
            size_t len;
            pcdom_cdata_section_t *cdata_section;

            cdata_section = pcdom_interface_cdata_section(entry->node);
            name = cdata_section->text.char_data.data.data;
            len = cdata_section->text.char_data.data.length;

            buff = g_string_new ("<![CDATA[ ");
            if (len > 6) {
                g_string_append_len (buff, (const gchar *)name, 6);
                g_string_append (buff, "…");
            }
            else {
                g_string_append (buff, (const gchar *)name);
            }
            g_string_append (buff, " ]]>");
            break;
        }

        case PCDOM_NODE_TYPE_ELEMENT:
        {
            const unsigned char *name;
            size_t len;
            pcdom_element_t *element;

            element = pcdom_interface_element (entry->node);
            name = pcdom_element_local_name (element, &len);

            if (entry->is_close_tag) {
                buff = g_string_new ("</");
                g_string_append_len (buff, (const gchar *)name, len);
                g_string_append_c (buff, '>');
            }
            else if (entry->is_self_close) {
                buff = g_string_new ("<");
                g_string_append_len (buff, (const gchar *)name, len);
                g_string_append (buff, " … />");
            }
            else if (entry->node->flags & NF_UNFOLDED) {
                buff = g_string_new ("<");
                g_string_append_len (buff, (const gchar *)name, len);
                g_string_append (buff, " … >…</");
                g_string_append_len (buff, (const gchar *)name, len);
                g_string_append_c (buff, '>');
            }
            else {
                buff = g_string_new ("<");
                g_string_append_len (buff, (const gchar *)name, len);
                g_string_append (buff, " … >");
            }
            break;
        }

        default:
            break;
    }

    if (buff) {
        tty_print_string (str_fit_to_term(buff->str, width, just_mode));
        g_string_free (buff, TRUE);
    }
}

static void
show_tree (WDOMTree * tree)
{
    Widget *w = WIDGET (tree);
    tree_entry *current;
    int i, j;
    unsigned int toplevel = 0;
    int x = 0, y = 0;
    int tree_lines, tree_cols;

    if (list_empty (&tree->entries))
        return;

    /* Initialize */
    tree_lines = tlines (tree);
    tree_cols = w->cols;

    widget_gotoyx (w, y, x);
    if (tree->is_panel) {
        tree_cols -= 2;
        x = y = 1;
    }

    if (tree->topmost != NULL)
        toplevel = tree->topmost->sublevel;

    if (tree->selected == NULL) {
        tree->selected = list_first_entry (&tree->entries, tree_entry, list);
    }
    current = tree->topmost;

    /* Loop for every line */
    for (i = 0; i < tree_lines; i++) {
        const int *colors;

        colors = widget_get_colors (w);
        tty_setcolor (tree->is_panel ? NORMAL_COLOR : colors[DLG_COLOR_NORMAL]);

        /* Move to the beginning of the line */
        tty_draw_hline (w->y + y + i, w->x + x, ' ', tree_cols);

        if (current->list.next == &tree->entries)
            continue;

        if (tree->is_panel) {
            bool selected;

            selected = widget_get_state (w, WST_FOCUSED) && current == tree->selected;
            tty_setcolor (selected ? SELECTED_COLOR : get_entry_color (current));
        }
        else {
            int idx = current == tree->selected ? DLG_COLOR_FOCUS : DLG_COLOR_NORMAL;

            tty_setcolor (colors[idx]);
        }

        if (current->sublevel == toplevel) {
            /* Show entry */
            show_entry(current,
                    tree_cols + (tree->is_panel ? 0 : 1), J_LEFT_FIT);
        }
        else {
            /* Sub level directory */
            tty_set_alt_charset (TRUE);

            /* Output branch parts */
            for (j = 0; j < current->sublevel - toplevel - 1; j++)
            {
                if (tree_cols - 8 - 3 * j < 9)
                    break;
                tty_print_char (' ');
                if (current->sublevel > j + toplevel + 1)
                    tty_print_char (ACS_VLINE);
                else
                    tty_print_char (' ');
                tty_print_char (' ');
            }
            tty_print_char (' ');
            j++;

            if (current->is_close_tag)
                tty_print_char (ACS_LLCORNER);
            else
                tty_print_char (ACS_LTEE);
            tty_print_char (ACS_HLINE);
            tty_set_alt_charset (FALSE);

            /* Show sub-name */
            tty_print_char (' ');
            show_entry (current, tree_cols - x - 3 * j, J_LEFT_FIT);
        }

        /* Calculate the next value for current */
        current = list_entry(current->list.next, tree_entry, list);
    }

    tree_show_mini_info (tree, tree_lines, tree_cols);
}

static tree_entry *
back_ptr (WDOMTree *tree, tree_entry *ptr, int *count)
{
    struct list_head *p = &ptr->list;
    int i;

    for (i = 0; p != &tree->entries && i < *count; p = p->prev, i++)
        ;

    *count = i;
    return container_of (p, tree_entry, list);
}

static bool
tree_move_backward (WDOMTree * tree, int i)
{
    tree->selected = back_ptr (tree, tree->selected, &i);
    return (i > 0);
}

static tree_entry *
forw_ptr (WDOMTree *tree, tree_entry *ptr, int *count)
{
    struct list_head *p = &ptr->list;
    int i;

    for (i = 0; p != &tree->entries && i < *count; p = p->next, i++)
        ;

    *count = i;
    return container_of (p, tree_entry, list);
}

static bool
tree_move_forward (WDOMTree * tree, int i)
{
    tree->selected = forw_ptr (tree, tree->selected, &i);
    return (i > 0);
}

static bool
tree_move_to_top (WDOMTree * tree)
{
    bool v = FALSE;
    tree_entry *new_topmost, *new_selected;

    new_topmost = list_entry (tree->entries.next, tree_entry, list);
    new_selected = new_topmost;

    if (new_topmost != tree->topmost || new_selected != tree->selected) {
        tree->topmost = new_topmost;
        tree->selected = new_selected;
        v = TRUE;
    }

    return v;
}

static bool
tree_move_to_bottom (WDOMTree * tree)
{
    bool v = FALSE;
    int tree_lines;
    tree_entry *new_topmost, *new_selected;

    new_selected = list_entry (tree->entries.prev, tree_entry, list);

    tree_lines = tlines (tree);
    new_topmost = back_ptr (tree, new_selected, &tree_lines);
    if (new_topmost != tree->topmost || new_selected != tree->selected) {
        tree->topmost = new_topmost;
        tree->selected = new_selected;
        v = TRUE;
    }

    return v;
}

static inline void
tree_move_up (WDOMTree * tree)
{
    if (tree_move_backward (tree, 1))
        show_tree (tree);
}

static inline void
tree_move_down (WDOMTree * tree)
{
    if (tree_move_forward (tree, 1))
        show_tree (tree);
}

static inline void
tree_move_home (WDOMTree * tree)
{
    if (tree_move_to_top (tree))
        show_tree (tree);
}

static inline void
tree_move_end (WDOMTree * tree)
{
    if (tree_move_to_bottom (tree))
        show_tree (tree);
}

static void
tree_move_pgup (WDOMTree * tree)
{
    if (tree_move_backward (tree, tlines (tree) - 1))
        show_tree (tree);
}

static void
tree_move_pgdn (WDOMTree * tree)
{
    if (tree_move_forward (tree, tlines (tree) - 1))
        show_tree (tree);
}

static bool
tree_fold_selected (WDOMTree *tree)
{
    size_t count = 0;
    pcdom_node_t *node = tree->selected->node;
    tree_entry *p, *n;

    for (p = tree->selected, n = list_entry(p->list.next, tree_entry, list);
            &p->list != &tree->entries;
            p = n, n = list_entry(n->list.next, tree_entry, list)) {
        bool tail = false;

        if (p->node == node && p->is_close_tag) {
            tail = true;
        }

        assert(tree->nr_entries > 0);
        tree->nr_entries--;
        list_del (&p->list);
        g_free (p);
        count++;

        if (tail)
            break;
    }

    node->flags &= ~NF_UNFOLDED;

    return count > 0;
}

struct my_subtree_walker_ctxt {
    unsigned int level;

    WDOMTree *tree;
    struct list_head *curr_tail;
};

static pchtml_action_t
my_subtree_walker(pcdom_node_t *node, void *ctx)
{
    tree_entry *entry;
    struct my_subtree_walker_ctxt *ctxt = ctx;

    switch (node->type) {
    case PCDOM_NODE_TYPE_COMMENT:
    case PCDOM_NODE_TYPE_TEXT:
    case PCDOM_NODE_TYPE_CDATA_SECTION:
        entry = g_new (tree_entry, 1);
        entry->sublevel = ctxt->level;
        entry->is_close_tag = 0;
        entry->is_self_close = 0;
        entry->node = node;

        list_add_tail (&entry->list, ctxt->curr_tail);
        ctxt->tree->nr_entries++;
        return PCHTML_ACTION_NEXT;

    case PCDOM_NODE_TYPE_ELEMENT:
        /* we unfold the `html`, `head` and `body` elements initially */
        if (node->flags & NF_UNFOLDED) {
            entry = g_new (tree_entry, 1);
            entry->sublevel = ctxt->level;
            entry->is_close_tag = 0;
            entry->is_self_close = pchtml_html_node_is_void(node);
            entry->node = node;

            list_add_tail(&entry->list, ctxt->curr_tail);
            ctxt->tree->nr_entries++;
            ctxt->curr_tail = &entry->list;

            /* insert a close tag entry */
            if (!entry->is_self_close && node->first_child != NULL
                    && (node->flags & NF_UNFOLDED)) {

                entry = g_new (tree_entry, 1);
                entry->sublevel = ctxt->level;
                entry->is_close_tag = 1;
                entry->is_self_close = 0;
                entry->node = node;

                /* add the close tag entry to the real tail */
                list_add_tail(&entry->list, &ctxt->tree->entries);
                ctxt->tree->nr_entries++;

                ctxt->level++;
                ctxt->curr_tail = &entry->list;

                // walk to the children
                return PCHTML_ACTION_OK;
            }
        }
        else {
            entry = g_new (tree_entry, 1);
            entry->sublevel = ctxt->level;
            entry->is_close_tag = 0;
            entry->is_self_close = pchtml_html_node_is_void(node);
            entry->node = node;

            list_add_tail(&entry->list, ctxt->curr_tail);
            ctxt->tree->nr_entries++;

            ctxt->curr_tail = &entry->list;
        }

        ctxt->curr_tail = &ctxt->tree->entries;

        /* walk to the siblings. */
        return PCHTML_ACTION_NEXT;

    case PCDOM_NODE_TYPE_DOCUMENT_TYPE:
    default:
        /* ignore any unknown node types */
        break;

    }

    return PCHTML_ACTION_OK;
}

static bool
tree_unfold_selected (WDOMTree *tree)
{
    struct my_subtree_walker_ctxt ctxt = {
        .level          = tree->selected->sublevel + 1,
        .tree           = tree,
        .curr_tail      = &tree->selected->list,
    };

    pcdom_node_simple_walk (tree->selected->node, my_subtree_walker, &ctxt);
    return false;
}

static bool
tree_move_to_open_tag (WDOMTree *tree)
{
    return false;
}

static bool
tree_move_to_parent (WDOMTree *tree)
{
    return false;
}

static bool
tree_move_left (WDOMTree * tree)
{
    bool v = FALSE;

    if (!tree->selected->is_close_tag &&
            tree->selected->node->flags & NF_UNFOLDED) {
        v = tree_fold_selected (tree);
    }
    else if (tree->selected->is_close_tag) {
        v = tree_move_to_open_tag (tree);
    }
    else {
        v = tree_move_to_parent (tree);
    }

    if (v)
        show_tree (tree);
    return v;
}

static bool
tree_move_right (WDOMTree * tree)
{
    bool v = FALSE;

    if (tree->selected->is_self_close ||
           tree->selected->node->flags & NF_UNFOLDED ) {
        v = tree_move_forward (tree, 1);
    }
    else {
        v = tree_unfold_selected (tree);
    }

    if (v)
        show_tree (tree);

    return v;
}

static cb_ret_t
tree_execute_cmd (WDOMTree * tree, long command)
{
    cb_ret_t res = MSG_HANDLED;

    if (command != CK_Search)
        tree->searching = FALSE;

    switch (command)
    {
    case CK_Help:
        {
            ev_help_t event_data = { NULL, "[Directory Tree]" };
            mc_event_raise (MCEVENT_GROUP_CORE, "help", &event_data);
        }
        break;
    case CK_Forget:
        // tree_forget (tree);
        break;
    case CK_Copy:
        // tree_copy (tree, "");
        break;
    case CK_Move:
        // tree_move (tree, "");
        break;
    case CK_Up:
        tree_move_up (tree);
        break;
    case CK_Down:
        tree_move_down (tree);
        break;
    case CK_Top:
        tree_move_home (tree);
        break;
    case CK_Bottom:
        tree_move_end (tree);
        break;
    case CK_PageUp:
        tree_move_pgup (tree);
        break;
    case CK_PageDown:
        tree_move_pgdn (tree);
        break;
    case CK_Enter:
        // tree_chdir_sel (tree);
        break;
    case CK_Reread:
        // tree_rescan (tree);
        break;
    case CK_Search:
        // tree_start_search (tree);
        break;
    case CK_Delete:
        // tree_rmdir (tree);
        break;
    case CK_Quit:
        if (!tree->is_panel)
            dlg_stop (DIALOG (WIDGET (tree)->owner));
        return res;
    default:
        res = MSG_NOT_HANDLED;
    }

    show_tree (tree);

    return res;
}

static cb_ret_t
tree_key (WDOMTree * tree, int key)
{
    long command;

    if (is_abort_char (key)) {
        if (tree->is_panel) {
            tree->searching = FALSE;
            show_tree (tree);
            return MSG_HANDLED; /* eat abort char */
        }
        /* modal tree dialog: let upper layer see the
           abort character and close the dialog */
        return MSG_NOT_HANDLED;
    }

    if (tree->searching && ((key >= ' ' && key <= 255) ||
                key == KEY_BACKSPACE)) {
        // VW: tree_do_search (tree, key);
        // VW: show_tree (tree);
        return MSG_HANDLED;
    }

    command = widget_lookup_key (WIDGET (tree), key);
    switch (command)
    {
    case CK_IgnoreKey:
        break;
    case CK_Left:
        return tree_move_left (tree) ? MSG_HANDLED : MSG_NOT_HANDLED;
    case CK_Right:
        return tree_move_right (tree) ? MSG_HANDLED : MSG_NOT_HANDLED;
    default:
        tree_execute_cmd (tree, command);
        return MSG_HANDLED;
    }

    /* Do not eat characters not meant for the tree below ' ' (e.g. C-l). */
    if (!command_prompt && ((key >= ' ' && key <= 255) ||
                key == KEY_BACKSPACE)) {
        // tree_start_search (tree);
        // tree_do_search (tree, key);
        return MSG_HANDLED;
    }

    return MSG_NOT_HANDLED;
}

static void
tree_frame (WDialog * h, WDOMTree * tree)
{
    Widget *w = WIDGET (tree);

    (void) h;

    tty_setcolor (NORMAL_COLOR);
    widget_erase (w);
    if (tree->is_panel)
    {
        const char *title = _("Directory tree");
        const int len = str_term_width1 (title);

        tty_draw_box (w->y, w->x, w->lines, w->cols, FALSE);

        widget_gotoyx (w, 0, (w->cols - len - 2) / 2);
        tty_printf (" %s ", title);

        if (panels_options.show_mini_info)
        {
            int y;

            y = w->lines - 3;
            widget_gotoyx (w, y, 0);
            tty_print_alt_char (ACS_LTEE, FALSE);
            widget_gotoyx (w, y, w->cols - 1);
            tty_print_alt_char (ACS_RTEE, FALSE);
            tty_draw_hline (w->y + y, w->x + 1, ACS_HLINE, w->cols - 2);
        }
    }
}

#if 0
static void
remove_callback (pcdom_node_t *node, void *data)
{
    WDOMTree *tree = data;

    if (tree->selected->node == node) {
        if (tree->selected != NULL)
            tree->selected = tree->selected->next;
        else
            tree->selected = tree->selected->prev;
    }
}
#endif

static void
tree_destroy (WDOMTree * tree)
{
    tree_entry *p, *n;

    // tree_store_remove_entry_remove_hook (remove_callback);

    list_for_each_entry_safe (p, n, &tree->entries, list) {
        list_del (&p->list);
        g_free (p);
    }

    tree->selected = NULL;
    tree->topmost = NULL;
    tree->nr_entries = 0;

    g_string_free (tree->search_buffer, TRUE);
    g_string_free (tree->xpath_buffer, TRUE);
}

static cb_ret_t
tree_callback (Widget * w, Widget * sender, widget_msg_t msg, int parm, void *data)
{
    WDOMTree *tree = (WDOMTree *) w;
    WDialog *h = DIALOG (w->owner);
    WButtonBar *b;

    switch (msg)
    {
    case MSG_DRAW:
        tree_frame (h, tree);
        show_tree (tree);
        if (widget_get_state (w, WST_FOCUSED))
        {
            b = find_buttonbar (h);
            widget_draw (WIDGET (b));
        }
        return MSG_HANDLED;

    case MSG_FOCUS:
        b = find_buttonbar (h);
        buttonbar_set_label (b, 1, Q_ ("ButtonBar|Help"), w->keymap, w);
        buttonbar_set_label (b, 2, Q_ ("ButtonBar|Rescan"), w->keymap, w);
        buttonbar_set_label (b, 3, Q_ ("ButtonBar|Forget"), w->keymap, w);
        buttonbar_set_label (b, 4, Q_ ("ButtonBar|Static"), w->keymap, w);
        buttonbar_set_label (b, 5, Q_ ("ButtonBar|Copy"), w->keymap, w);
        buttonbar_set_label (b, 6, Q_ ("ButtonBar|RenMov"), w->keymap, w);
        buttonbar_clear_label (b, 7, w);
        buttonbar_set_label (b, 8, Q_ ("ButtonBar|Rmdir"), w->keymap, w);
        return MSG_HANDLED;

    case MSG_UNFOCUS:
        tree->searching = FALSE;
        return MSG_HANDLED;

    case MSG_KEY:
        return tree_key (tree, parm);

    case MSG_ACTION:
        /* command from buttonbar */
        return tree_execute_cmd (tree, parm);

    case MSG_DESTROY:
        tree_destroy (tree);
        return MSG_HANDLED;

    default:
        return widget_default_callback (w, sender, msg, parm, data);
    }
}

/*** Mouse callback */
static void
tree_mouse_callback (Widget * w, mouse_msg_t msg, mouse_event_t * event)
{
    WDOMTree *tree = (WDOMTree *) w;
    int y;

    y = event->y;
    if (tree->is_panel)
        y--;

    switch (msg) {
    case MSG_MOUSE_DOWN:
        /* rest of the upper frame - call menu */
        if (tree->is_panel && event->y == WIDGET (w->owner)->y) {
            /* return MOU_UNHANDLED */
            event->result.abort = TRUE;
        }
        else if (!widget_get_state (w, WST_FOCUSED))
            (void) change_panel ();
        break;

    case MSG_MOUSE_CLICK:
        {
            int lines;

            lines = tlines (tree);

            if (y < 0) {
                if (tree_move_backward (tree, lines - 1))
                    show_tree (tree);
            }
            else if (y >= lines) {
                if (tree_move_forward (tree, lines - 1))
                    show_tree (tree);
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

/*** public functions */

WDOMTree *
dom_tree_new (int y, int x, int lines, int cols, bool is_panel)
{
    WDOMTree *tree;
    Widget *w;

    tree = g_new (WDOMTree, 1);

    w = WIDGET (tree);
    widget_init (w, y, x, lines, cols, tree_callback, tree_mouse_callback);
    w->options |= WOP_SELECTABLE | WOP_TOP_SELECT;
    w->keymap = tree_map;

    tree->doc = NULL;

    INIT_LIST_HEAD (&tree->entries);
    tree->selected = NULL;
    tree->topmost = NULL;
    tree->is_panel = is_panel;

    tree->search_buffer = g_string_sized_new (MC_MAXPATHLEN);
    tree->searching = FALSE;

    tree->xpath_buffer = g_string_sized_new (MC_DEFXPATHLEN);

    tree->nr_entries = 0;
    return tree;
}

struct my_tree_walker_ctxt {
    bool is_first_time;

    unsigned int level;

    WDOMTree *tree;
    struct list_head *curr_tail;
};

static pchtml_action_t
my_tree_walker(pcdom_node_t *node, void *ctx)
{
    tree_entry *entry;
    struct my_tree_walker_ctxt *ctxt = ctx;

    switch (node->type) {
    case PCDOM_NODE_TYPE_DOCUMENT_TYPE:
        entry = g_new (tree_entry, 1);
        entry->sublevel = 0;    /* always be zero */
        entry->is_close_tag = 0;
        entry->is_self_close = 0;
        entry->node = node;

        list_add_tail (&entry->list, ctxt->curr_tail);
        ctxt->tree->nr_entries++;
        return PCHTML_ACTION_NEXT;

    case PCDOM_NODE_TYPE_COMMENT:
    case PCDOM_NODE_TYPE_TEXT:
    case PCDOM_NODE_TYPE_CDATA_SECTION:
        entry = g_new (tree_entry, 1);
        entry->sublevel = ctxt->level;
        entry->is_close_tag = 0;
        entry->is_self_close = 0;
        entry->node = node;

        list_add_tail (&entry->list, ctxt->curr_tail);
        ctxt->tree->nr_entries++;
        return PCHTML_ACTION_NEXT;

    case PCDOM_NODE_TYPE_ELEMENT:
        /* we unfold the `html`, `head` and `body` elements initially */
        if ((ctxt->is_first_time && ctxt->level < 2) ||
                (node->flags & NF_UNFOLDED)) {
            node->flags |= NF_UNFOLDED;

            entry = g_new (tree_entry, 1);
            entry->sublevel = ctxt->level;
            entry->is_close_tag = 0;
            entry->is_self_close = pchtml_html_node_is_void(node);
            entry->node = node;

            list_add_tail(&entry->list, ctxt->curr_tail);
            ctxt->tree->nr_entries++;
            ctxt->curr_tail = &entry->list;

            /* insert a close tag entry */
            if (!entry->is_self_close && node->first_child != NULL
                    && (node->flags & NF_UNFOLDED)) {

                entry = g_new (tree_entry, 1);
                entry->sublevel = ctxt->level;
                entry->is_close_tag = 1;
                entry->is_self_close = 0;
                entry->node = node;

                /* add the close tag entry to the real tail */
                list_add_tail(&entry->list, &ctxt->tree->entries);
                ctxt->tree->nr_entries++;

                ctxt->level++;
                ctxt->curr_tail = &entry->list;

                // walk to the children
                return PCHTML_ACTION_OK;
            }
        }
        else {
            entry = g_new (tree_entry, 1);
            entry->sublevel = ctxt->level;
            entry->is_close_tag = 0;
            entry->is_self_close = pchtml_html_node_is_void(node);
            entry->node = node;

            list_add_tail(&entry->list, ctxt->curr_tail);
            ctxt->tree->nr_entries++;

            ctxt->curr_tail = &entry->list;
        }

        ctxt->curr_tail = &ctxt->tree->entries;

        /* walk to the siblings. */
        return PCHTML_ACTION_NEXT;

    default:
        /* ignore any unknown node types */
        break;

    }

    return PCHTML_ACTION_OK;
}

bool
dom_tree_load (WDOMTree *tree, pcdom_document_t *doc)
{
    struct my_tree_walker_ctxt ctxt = {
        .is_first_time  = false,
        .level          = 0,
        .tree           = tree,
        .curr_tail      = &tree->entries,
    };

    assert (tree->nr_entries == 0);

    /* use the user-defined pointer of document for
       whether it is the first time to load the tree. */
    if (doc->user == NULL) {
        ctxt.is_first_time = true;
    }
    doc->user = tree;

    pcdom_node_simple_walk (&doc->node, my_tree_walker, &ctxt);

    if (tree->nr_entries > 0) {
        tree->doc = doc;
        tree->topmost = list_entry (tree->entries.next, tree_entry, list);
        tree->selected = tree->topmost;
        show_tree (tree);
    }

    return false;
}
