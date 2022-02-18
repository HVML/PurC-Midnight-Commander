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

#include "dom-viewer.h"
#include "dom-tree.h"
#include "dom-text.h"

/*** global variables */
hook_t *select_element_hook;

/*** file scope macro definitions */

#define MAX_ENTRY_CHARS     6

#define MC_DEFXPATHLEN      128

#define NF_UNFOLDED         0x0001

#define tlines(t) (t->is_panel ? WIDGET (t)->lines - 2 - \
        (panels_options.show_mini_info ? 2 : 0) : WIDGET (t)->lines)

/*** file scope type declarations */

typedef struct tree_entry {
    struct list_head list;

    int                 level;
    unsigned int        is_close_tag:1;     /* is a close tag? */
    unsigned int        is_self_close:1;    /* is self-close? */
    pcdom_node_t        *node;
    gchar               *normalized_text;   /* normalized text or NULL */
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
        case PCDOM_NODE_TYPE_CDATA_SECTION:
            return MARKED_COLOR;
        case PCDOM_NODE_TYPE_ELEMENT:
        case PCDOM_NODE_TYPE_TEXT:
        default:
            break;
    }

    return NORMAL_COLOR;
}

static void
append_intrinsic_attrs(GString *string, pcdom_element_t *element)
{
    const gchar *attr_value;
    size_t attr_value_len;

    attr_value = (const gchar *)pcdom_element_id (element, &attr_value_len);
    if (attr_value) {
        g_string_append (string, " id=\"");
        g_string_append_len (string, attr_value, attr_value_len);
        g_string_append (string, "\"");
    }

    attr_value = (const gchar *)pcdom_element_class (element, &attr_value_len);
    if (attr_value) {
        g_string_append (string, " class=\"");
        g_string_append_len (string, attr_value, attr_value_len);
        g_string_append (string, "\"");
    }
}

static void
show_entry(const tree_entry *entry, int width, align_crt_t just_mode)
{
    GString *buff = NULL;

    switch (entry->node->type) {
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

        case PCDOM_NODE_TYPE_ELEMENT:
        {
            const unsigned char *name;
            size_t len;
            pcdom_element_t *element;
            bool has_attr;

            element = pcdom_interface_element (entry->node);
            name = pcdom_element_local_name (element, &len);
            has_attr = (pcdom_element_first_attribute (element) != NULL);

            if (entry->is_close_tag) {
                buff = g_string_new ("</");
                g_string_append_len (buff, (const gchar *)name, len);
                g_string_append_c (buff, '>');
            }
            else if (entry->is_self_close) {
                buff = g_string_new ("<");
                g_string_append_len (buff, (const gchar *)name, len);
                if (has_attr) {
                    append_intrinsic_attrs(buff, element);
                    g_string_append (buff, " … ");
                }
                g_string_append (buff, "/>");
            }
            else if (entry->node->flags & NF_UNFOLDED) {
                buff = g_string_new ("<");
                g_string_append_len (buff, (const gchar *)name, len);
                if (has_attr) {
                    append_intrinsic_attrs(buff, element);
                    g_string_append (buff, " … ");
                }
                g_string_append (buff, ">");
            }
            else {
                buff = g_string_new ("<");
                g_string_append_len (buff, (const gchar *)name, len);
                if (has_attr) {
                    append_intrinsic_attrs(buff, element);
                    g_string_append (buff, " … ");
                }
                g_string_append (buff, ">");
                if (entry->node->first_child) {
                    g_string_append (buff, " … ");
                }
                g_string_append (buff, "</");
                g_string_append_len (buff, (const gchar *)name, len);
                g_string_append_c (buff, '>');
            }
            break;
        }

        case PCDOM_NODE_TYPE_TEXT:
        {
            buff = g_string_new (entry->normalized_text);
            dom_text_truncate_with_ellipsis (buff, MAX_ENTRY_CHARS);
            g_string_prepend (buff, "“");
            g_string_append (buff, "”");
            break;
        }

        case PCDOM_NODE_TYPE_COMMENT:
        {
            buff = g_string_new (entry->normalized_text);
            dom_text_truncate_with_ellipsis (buff, MAX_ENTRY_CHARS);
            g_string_prepend (buff, "<!-- ");
            g_string_append (buff, " -->");
            break;
        }

        case PCDOM_NODE_TYPE_CDATA_SECTION:
        {
            buff = g_string_new (entry->normalized_text);
            dom_text_truncate_with_ellipsis (buff, MAX_ENTRY_CHARS);
            g_string_prepend (buff, "<![CDATA[ ");
            g_string_append (buff, " ]]>");
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
    int toplevel = 0;
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

    // toplevel = tree->topmost->level;
    current = tree->topmost;

    /* Loop for every line */
    for (i = 0; i < tree_lines; i++) {
        const int *colors;

        colors = widget_get_colors (w);
        tty_setcolor (tree->is_panel ? NORMAL_COLOR : colors[DLG_COLOR_NORMAL]);

        /* Move to the beginning of the line */
        tty_draw_hline (w->y + y + i, w->x + x, ' ', tree_cols);

        if (&current->list == &tree->entries)
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

        if (current->level == toplevel) {
            /* Show entry */
            show_entry(current,
                    tree_cols + (tree->is_panel ? 0 : 1), J_LEFT_FIT);
        }
        else {
            /* Output branch parts */
            tty_set_alt_charset (TRUE);
            for (j = 0; j < current->level - toplevel - 1; j++)
            {
                if (tree_cols - 8 - 3 * j < 9)
                    break;
                tty_print_char (' ');
                if (current->level > j + toplevel + 1)
                    tty_print_char (ACS_VLINE);
                else
                    tty_print_char (' ');
                tty_print_char (' ');
            }
            tty_print_char (' ');
            j++;

            if ((current->is_close_tag || !(current->node->flags & NF_UNFOLDED))
                    && current->level > 0 &&
                    list_entry (current->list.next,
                        tree_entry, list)->level < current->level)
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

    for (i = 0; p != tree->entries.next && i < *count; p = p->prev, i++)
        ;

    *count = i;
    return container_of (p, tree_entry, list);
}

static void
set_entry_content(const tree_entry *entry, WDOMContent *dom_cnt)
{
    GString *buff = NULL;

    switch (entry->node->type) {
        case PCDOM_NODE_TYPE_DOCUMENT_TYPE:
        {
            buff = g_string_new (_("(NO CONTENT)"));
            break;
        }

        case PCDOM_NODE_TYPE_COMMENT:
        {
            const unsigned char *name;
            size_t len;
            pcdom_comment_t *comment;

            comment = pcdom_interface_comment (entry->node);
            name = comment->char_data.data.data;
            len = comment->char_data.data.length;

            buff = g_string_new ("");
            g_string_append_len (buff, (const gchar *)name, len);
            break;
        }

        case PCDOM_NODE_TYPE_ELEMENT:
        {
            if (entry->is_self_close) {
                buff = g_string_new (_("(NO CONTENT)"));
            }
            else {
                buff = g_string_new (_("(SEE CHILDREN)"));
            }
            break;
        }

        case PCDOM_NODE_TYPE_TEXT:
        {
            const unsigned char *name;
            size_t len;
            pcdom_text_t *text;

            text = pcdom_interface_text (entry->node);
            name = text->char_data.data.data;
            len = text->char_data.data.length;

            buff = g_string_new ("");
            g_string_append_len (buff, (const gchar *)name, len);
            break;
        }

        case PCDOM_NODE_TYPE_CDATA_SECTION:
        {
            const unsigned char *name;
            size_t len;
            pcdom_cdata_section_t *cdata_section;

            cdata_section = pcdom_interface_cdata_section (entry->node);
            name = cdata_section->text.char_data.data.data;
            len = cdata_section->text.char_data.data.length;

            buff = g_string_new ("");
            g_string_append_len (buff, (const gchar *)name, len);
            break;
        }

        default:
            break;
    }

    if (buff) {
        dom_content_load (dom_cnt, buff);
    }
}

static inline void
tree_set_selected(WDOMTree * tree, tree_entry *new_selected, bool adjust_topmost)
{
    if (tree->selected != new_selected) {
        tree->selected = new_selected;

        /* adjust topmost */
        if (adjust_topmost) {
            tree_entry *p, *new_topmost = NULL;

            int lines = tlines (tree);
            int off = 1;
            for (p = list_entry(new_selected->list.prev, tree_entry, list);
                    &p->list != &tree->entries;
                    p = list_entry(p->list.prev, tree_entry, list)) {
                off++;
                if (off >= lines) {
                    new_topmost = p;
                    break;
                }
            }
            if (new_topmost)
                tree->topmost = new_topmost;

            /* check if the new selected is ahead of the current topmost */
            for (p = new_selected;
                    &p->list != &tree->entries;
                    p = list_entry(p->list.next, tree_entry, list)) {
                if (p == tree->topmost) {
                    tree->topmost = new_selected;
                }
            }
        }

        /* update other widgets */
        execute_hooks (select_element_hook, tree->selected->node);
        WDialog *h = DIALOG (WIDGET (tree)->owner);
        WDOMViewInfo* info = h->data;
        if (info->dom_cnt)
            set_entry_content(tree->selected, info->dom_cnt);
    }
}

static bool
tree_move_backward (WDOMTree * tree, int i)
{
    tree_entry *e = back_ptr (tree, tree->selected, &i);
    tree_set_selected (tree, e, true);
    return (i > 0);
}

static tree_entry *
forw_ptr (WDOMTree *tree, tree_entry *ptr, int *count)
{
    struct list_head *p = &ptr->list;
    int i;

    for (i = 0; p != tree->entries.prev && i < *count; p = p->next, i++)
        ;

    *count = i;
    return container_of (p, tree_entry, list);
}

static bool
tree_move_forward (WDOMTree * tree, int i)
{
    tree_entry *e = forw_ptr (tree, tree->selected, &i);
    tree_set_selected (tree, e, true);
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
        tree_set_selected (tree, new_selected, false);
        v = TRUE;
    }

    return v;
}

static bool
tree_move_to_bottom (WDOMTree * tree)
{
    bool v = FALSE;
    tree_entry *new_selected;

    new_selected = list_entry (tree->entries.prev, tree_entry, list);
    if (new_selected != tree->selected) {
        tree_set_selected (tree, new_selected, true);
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

    p = list_entry(tree->selected->list.next, tree_entry, list);
    for (n = list_entry(p->list.next, tree_entry, list);
            &p->list != &tree->entries;
            p = n, n = list_entry(n->list.next, tree_entry, list)) {
        bool tail = false;

        if (p->node == node && p->is_close_tag) {
            tail = true;
        }

        assert(tree->nr_entries > 0);
        tree->nr_entries--;

        if (p->normalized_text)
            g_free (p->normalized_text);
        list_del (&p->list);
        g_free (p);
        count++;

        if (tail)
            break;
    }

    node->flags &= ~NF_UNFOLDED;

    return count > 0;
}

static inline int node_to_level (pcdom_node_t *node)
{
    int level = 0;

    while (node->parent) {
        level++;

        node = node->parent;
    }

    level--;
    if (level < 0)
        level = 0;

    return level;
}

struct my_tree_walker_ctxt {
    bool is_first_time;
    WDOMTree *tree;
    tree_entry *last;
};

static tree_entry *find_parent_close_entry_before (WDOMTree *tree,
        tree_entry *last, tree_entry *entry)
{
    tree_entry *p;

    if (last == NULL)
        p = list_last_entry (&tree->entries, tree_entry, list);
    else
        p = last;

    for (; &p->list != &tree->entries;
            p = list_entry(p->list.prev, tree_entry, list)) {

        if (p->node == entry->node->parent) {
            if (p->is_close_tag)
                return p;
            else
                break;
        }
    }

    return NULL;
}

static void my_tree_insert_entry (struct my_tree_walker_ctxt *ctxt,
        tree_entry *entry)
{
    tree_entry *parent_close_entry;

    parent_close_entry = find_parent_close_entry_before (ctxt->tree,
            ctxt->last, entry);

    if (parent_close_entry) {
        /* insert the entry before the parent close entry */
        list_add (&entry->list, parent_close_entry->list.prev);
    }
    else
        list_add_tail (&entry->list, &ctxt->tree->entries);

    ctxt->tree->nr_entries++;
}

static pchtml_action_t
my_subtree_walker(pcdom_node_t *node, void *ctx)
{
    tree_entry *entry;
    struct my_tree_walker_ctxt *ctxt = ctx;
    int level;
    GString *string = NULL;

    switch (node->type) {
    case PCDOM_NODE_TYPE_TEXT:
    case PCDOM_NODE_TYPE_COMMENT:
    case PCDOM_NODE_TYPE_CDATA_SECTION:
        if (node->type == PCDOM_NODE_TYPE_TEXT) {
            pcdom_text_t *text;

            text = pcdom_interface_text (node);
            string = g_string_new_len (
                    (const gchar *)text->char_data.data.data,
                    text->char_data.data.length);

            dom_text_normalize(string);
            if (string->len == 0) {
                g_string_free (string, TRUE);
                return PCHTML_ACTION_NEXT;
            }
        }
        else if (node->type == PCDOM_NODE_TYPE_COMMENT) {
            pcdom_comment_t *comment;

            comment = pcdom_interface_comment (node);
            string = g_string_new_len (
                    (const gchar *)comment->char_data.data.data,
                    comment->char_data.data.length);

            dom_text_normalize(string);
        }
        else {
            pcdom_cdata_section_t *cdata_section;

            cdata_section = pcdom_interface_cdata_section(node);
            string = g_string_new_len (
                    (const gchar *)cdata_section->text.char_data.data.data,
                    cdata_section->text.char_data.data.length);

            dom_text_normalize (string);
        }

        assert (string != NULL);

        entry = g_new0 (tree_entry, 1);
        entry->level = node_to_level (node);
        entry->is_close_tag = 0;
        entry->is_self_close = 0;
        entry->node = node;
        entry->normalized_text = g_string_free (string, FALSE);

        my_tree_insert_entry (ctxt, entry);
        return PCHTML_ACTION_NEXT;

    case PCDOM_NODE_TYPE_ELEMENT:
        level = node_to_level (node);
        if (node == ctxt->tree->selected->node)
            break;

        if (node->flags & NF_UNFOLDED) {
            entry = g_new0 (tree_entry, 1);
            entry->level = level;
            entry->is_close_tag = 0;
            entry->is_self_close = pchtml_html_node_is_void(node);
            entry->node = node;

            my_tree_insert_entry (ctxt, entry);

            /* insert a close tag entry */
            if (!entry->is_self_close && node->first_child != NULL
                    && (node->flags & NF_UNFOLDED)) {
                tree_entry *close_entry;

                close_entry = g_new0 (tree_entry, 1);
                close_entry->level = level;
                close_entry->is_close_tag = 1;
                close_entry->is_self_close = 0;
                close_entry->node = node;

                /* insert the close tag entry after the open tag entry */
                list_add (&close_entry->list, &entry->list);

                // walk to the children
                return PCHTML_ACTION_OK;
            }
        }
        else {
            entry = g_new0 (tree_entry, 1);
            entry->level = level;
            entry->is_close_tag = 0;
            entry->is_self_close = pchtml_html_node_is_void(node);
            entry->node = node;

            my_tree_insert_entry (ctxt, entry);
        }

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
    struct my_tree_walker_ctxt ctxt = {
        .tree           = tree,
    };

    if (!(tree->selected->node->flags & NF_UNFOLDED)) {

        /* insert a close tag entry */
        if (!tree->selected->is_self_close &&
                tree->selected->node->first_child != NULL) {

            tree_entry *close_entry;

            tree->selected->node->flags |= NF_UNFOLDED;

            close_entry = g_new0 (tree_entry, 1);
            close_entry->level = tree->selected->level;
            close_entry->is_close_tag = 1;
            close_entry->is_self_close = 0;
            close_entry->node = tree->selected->node;

            /* insert the close tag entry after the open tag entry */
            list_add (&close_entry->list, &tree->selected->list);

            ctxt.last = close_entry;
            pcdom_node_simple_walk (tree->selected->node, my_subtree_walker, &ctxt);
        }
    }
    return false;
}

static bool
tree_move_to_open_tag (WDOMTree *tree)
{
    bool found = false;
    pcdom_node_t *node = tree->selected->node;
    tree_entry *p = tree->selected;

    for (p = list_entry(p->list.prev, tree_entry, list);
            &p->list != &tree->entries;
            p = list_entry(p->list.prev, tree_entry, list)) {

        if (p->node == node && !p->is_close_tag) {
            tree_set_selected (tree, p, true);
            found = true;
            break;
        }
    }

    return found;
}

static bool
tree_move_to_parent (WDOMTree *tree)
{
    bool found = false;
    int level = tree->selected->level;
    tree_entry *p = tree->selected;

    for (p = list_entry(p->list.prev, tree_entry, list);
            &p->list != &tree->entries;
            p = list_entry(p->list.prev, tree_entry, list)) {

        if (p->level < level) {
            tree_set_selected (tree, p, true);
            found = true;
            break;
        }
    }

    return found;
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
            ev_help_t event_data = { NULL, "[DOM Tree]" };
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
        dlg_run_done (DIALOG (WIDGET (tree)->owner));
        //dlg_stop (DIALOG (WIDGET (tree)->owner));
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
        return tree_execute_cmd (tree, command);
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
        const char *title = _("DOM tree");
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
        if (p->normalized_text)
            g_free (p->normalized_text);
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
        buttonbar_clear_label (b, 8, w);
        buttonbar_clear_label (b, 9, w);
        buttonbar_clear_label (b, 10, w);
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

    tree = g_new0 (WDOMTree, 1);

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

static pchtml_action_t
my_tree_walker(pcdom_node_t *node, void *ctx)
{
    tree_entry *entry;
    struct my_tree_walker_ctxt *ctxt = ctx;
    int level;
    GString *string = NULL;

    switch (node->type) {
    case PCDOM_NODE_TYPE_DOCUMENT_TYPE:
        entry = g_new0 (tree_entry, 1);
        entry->level = 0;    /* always be zero */
        entry->is_close_tag = 0;
        entry->is_self_close = 0;
        entry->node = node;

        my_tree_insert_entry (ctxt, entry);
        return PCHTML_ACTION_NEXT;

    case PCDOM_NODE_TYPE_TEXT:
    case PCDOM_NODE_TYPE_COMMENT:
    case PCDOM_NODE_TYPE_CDATA_SECTION:
        if (node->type == PCDOM_NODE_TYPE_TEXT) {
            pcdom_text_t *text;

            text = pcdom_interface_text (node);
            string = g_string_new_len (
                    (const gchar *)text->char_data.data.data,
                    text->char_data.data.length);

            dom_text_normalize(string);
            if (string->len == 0) {
                g_string_free (string, TRUE);
                return PCHTML_ACTION_NEXT;
            }
        }
        else if (node->type == PCDOM_NODE_TYPE_COMMENT) {
            pcdom_comment_t *comment;

            comment = pcdom_interface_comment (node);
            string = g_string_new_len (
                    (const gchar *)comment->char_data.data.data,
                    comment->char_data.data.length);

            dom_text_normalize(string);
        }
        else {
            pcdom_cdata_section_t *cdata_section;

            cdata_section = pcdom_interface_cdata_section (node);
            string = g_string_new_len (
                    (const gchar *)cdata_section->text.char_data.data.data,
                    cdata_section->text.char_data.data.length);

            dom_text_normalize (string);
        }

        assert (string != NULL);

        entry = g_new0 (tree_entry, 1);
        entry->level = node_to_level (node);
        entry->is_close_tag = 0;
        entry->is_self_close = 0;
        entry->node = node;
        entry->normalized_text = g_string_free (string, FALSE);

        my_tree_insert_entry (ctxt, entry);
        return PCHTML_ACTION_NEXT;

    case PCDOM_NODE_TYPE_ELEMENT:
        level = node_to_level (node);
        /* we unfold the `html`, `head` and `body` elements initially */
        if ((ctxt->is_first_time && level < 2) ||
                (node->flags & NF_UNFOLDED)) {
            node->flags |= NF_UNFOLDED;

            entry = g_new0 (tree_entry, 1);
            entry->level = level;
            entry->is_close_tag = 0;
            entry->is_self_close = pchtml_html_node_is_void(node);
            entry->node = node;

            my_tree_insert_entry (ctxt, entry);

            /* insert a close tag entry */
            if (!entry->is_self_close && node->first_child != NULL) {
                tree_entry *close_entry;

                close_entry = g_new0 (tree_entry, 1);
                close_entry->level = level;
                close_entry->is_close_tag = 1;
                close_entry->is_self_close = 0;
                close_entry->node = node;

                /* insert the close tag entry after the open tag entry */
                list_add (&close_entry->list, &entry->list);

                // walk to the children
                return PCHTML_ACTION_OK;
            }
        }
        else {
            entry = g_new0 (tree_entry, 1);
            entry->level = level;
            entry->is_close_tag = 0;
            entry->is_self_close = pchtml_html_node_is_void(node);
            entry->node = node;

            my_tree_insert_entry (ctxt, entry);
        }

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
        .is_first_time      = false,
        .tree               = tree,
        .last               = NULL,
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
        return true;
    }

    return false;
}
