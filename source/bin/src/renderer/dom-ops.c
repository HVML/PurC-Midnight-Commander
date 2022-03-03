/*
   DOM operations for the PurC Midnight Commander

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
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "lib/sorted-array.h"
#include "lib/hiboxcompat.h"

#include "dom-ops.h"

#define SA_INITIAL_SIZE        128

static uint64_t get_hvml_handle(pcdom_node_t *node)
{
    pcdom_element_t *element = (pcdom_element_t *)node;
    pcdom_attr_t *attr = pcdom_element_first_attribute(element);

    while (attr) {
        const char *str;
        size_t sz;

        str = (const char *)pcdom_attr_local_name(attr, &sz);
        if (strcasecmp(str, "hvml:handle") == 0) {
            str = (const char *)pcdom_attr_value(attr, &sz);
            return (uint64_t)strtoull(str, NULL, 16);
        }

        attr = pcdom_element_next_attribute (attr);
    }

    return 0;
}

struct my_tree_walker_ctxt {
    bool add_or_remove;
    struct sorted_array *sa;
};

static pchtml_action_t
my_tree_walker(pcdom_node_t *node, void *ctx)
{
    struct my_tree_walker_ctxt *ctxt = ctx;
    uint64_t handle;

    switch (node->type) {
    case PCDOM_NODE_TYPE_DOCUMENT_TYPE:
        return PCHTML_ACTION_NEXT;

    case PCDOM_NODE_TYPE_TEXT:
    case PCDOM_NODE_TYPE_COMMENT:
    case PCDOM_NODE_TYPE_CDATA_SECTION:
        return PCHTML_ACTION_NEXT;

    case PCDOM_NODE_TYPE_ELEMENT:
        handle = get_hvml_handle(node);
        if (handle) {
            if (ctxt->add_or_remove) {
                if (sorted_array_add(ctxt->sa, handle, node)) {
                    ULOG_WARN("Failed to store handle/node pair\n");
                }
            }
            else {
                if (!sorted_array_remove(ctxt->sa, handle)) {
                    ULOG_WARN("Failed to remove handle/node pair\n");
                }
            }
        }

        if (node->first_child != NULL) {
            return PCHTML_ACTION_OK;
        }

        /* walk to the siblings. */
        return PCHTML_ACTION_NEXT;

    default:
        /* ignore any unknown node types */
        break;
    }

    return PCHTML_ACTION_NEXT;
}

static bool
dom_build_hvml_handle_map(pcdom_document_t *dom_doc)
{
    struct sorted_array *sa;
    struct my_dom_user_data *user = dom_doc->user;

    assert(user && user->sa == NULL);
    sa = sorted_array_create(SAFLAG_DEFAULT, SA_INITIAL_SIZE, NULL, NULL);
    if (sa == NULL) {
        return false;
    }

    struct my_tree_walker_ctxt ctxt = {
        .add_or_remove  = true,
        .sa             = sa,
    };

    pcdom_node_simple_walk(&dom_doc->node, my_tree_walker, &ctxt);
    user->sa = sa;
    return true;
}

static bool
dom_destroy_hvml_handle_map(pcdom_document_t *dom_doc)
{
    struct my_dom_user_data *user = dom_doc->user;

    assert(user);

    if (user->sa == NULL) {
        return false;
    }

    sorted_array_destroy(user->sa);
    user->sa = NULL;
    return true;
}

pcdom_element_t *
dom_get_element_by_handle(pcdom_document_t *dom_doc, uint64_t handle)
{
    void* data;
    struct my_dom_user_data *user = dom_doc->user;

    if (handle == 0) {
        return dom_doc->element;
    }

    assert(user && user->sa);

    if (sorted_array_find(user->sa, handle, &data))
        return (pcdom_element_t *)data;

    return NULL;
}

char *dom_set_title(pcdom_document_t *dom_doc, const char *title)
{
    struct my_dom_user_data *user = dom_doc->user;

    assert(user);

    if (user->title)
        free(user->title);
    user->title = strdup(title);

    return user->title;
}

bool dom_prepare_user_data(pcdom_document_t *dom_doc, bool with_handle)
{
    if (dom_doc->user == NULL) {
        dom_doc->user = calloc(1, sizeof(struct my_dom_user_data));
        if (dom_doc->user && with_handle)
            dom_build_hvml_handle_map(dom_doc);
    }
    else
        return false;

    return dom_doc->user != NULL;
}

bool dom_cleanup_user_data(pcdom_document_t *dom_doc)
{
    struct my_dom_user_data *user = dom_doc->user;

    if (user == NULL) {
        return false;
    }

    if (user->sa) {
        dom_destroy_hvml_handle_map(dom_doc);
    }

    if (user->title)
        free(user->title);

    free(user);
    dom_doc->user = NULL;
    return true;
}

bool
dom_merge_hvml_handle_map(pcdom_document_t *dom_doc, pcdom_node_t *subtree)
{
    struct my_dom_user_data *user = dom_doc->user;

    assert(user && user->sa);

    struct my_tree_walker_ctxt ctxt = {
        .add_or_remove  = true,
        .sa             = user->sa,
    };

    pcdom_node_simple_walk(subtree, my_tree_walker, &ctxt);
    return true;
}

bool
dom_subtract_hvml_handle_map(pcdom_document_t *dom_doc, pcdom_node_t *subtree)
{
    struct sorted_array *sa;
    struct my_dom_user_data *user = dom_doc->user;

    assert(user && user->sa);
    sa = user->sa;

    struct my_tree_walker_ctxt ctxt = {
        .add_or_remove  = false,
        .sa             = sa,
    };

    pcdom_node_simple_walk(subtree, my_tree_walker, &ctxt);
    return true;
}

pcdom_node_t *
dom_parse_fragment(pcdom_document_t *dom_doc,
        pcdom_element_t *parent, const char *fragment, size_t length)
{
    unsigned int status;
    pchtml_html_document_t *html_doc;
    pcdom_node_t *root = NULL;

    html_doc = (pchtml_html_document_t *)dom_doc;
    status = pchtml_html_document_parse_fragment_chunk_begin(html_doc, parent);
    if (status)
        goto failed;

    status = pchtml_html_document_parse_fragment_chunk(html_doc,
            (const unsigned char*)"<div>", 5);
    if (status)
        goto failed;

    status = pchtml_html_document_parse_fragment_chunk(html_doc,
            (const unsigned char*)fragment, length);
    if (status)
        goto failed;

    status = pchtml_html_document_parse_fragment_chunk(html_doc,
            (const unsigned char*)"</div>", 6);
    if (status)
        goto failed;

    root = pchtml_html_document_parse_fragment_chunk_end(html_doc);

failed:
    return root;
}

pcdom_node_t *
dom_clone_subtree(pcdom_document_t *dom_doc, pcdom_node_t *subtree,
        uint64_t handle_msb)
{
    ULOG_ERR("Unexpected call to %s\n", __func__);
    assert(0);
    return NULL;
}

void
dom_append_subtree_to_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree)
{
    pcdom_node_t *parent = pcdom_interface_node(element);

    if (subtree && subtree->first_child) {
        pcdom_node_t *div;
        div = subtree->first_child;

        dom_merge_hvml_handle_map(dom_doc, div);
        while (div->first_child) {
            pcdom_node_t *child = div->first_child;
            pcdom_node_remove(child);
            pcdom_node_append_child(parent, child);
        }
    }

    if (subtree)
        pcdom_node_destroy_deep(subtree);
}

void
dom_prepend_subtree_to_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree)
{
    pcdom_node_t *parent = pcdom_interface_node(element);

    if (subtree && subtree->first_child) {
        pcdom_node_t *div;
        div = subtree->first_child;

        dom_merge_hvml_handle_map(dom_doc, div);
        while (div->last_child) {
            pcdom_node_t *child = div->last_child;
            pcdom_node_remove(child);
            pcdom_node_prepend_child(parent, child);
        }
    }

    if (subtree)
        pcdom_node_destroy_deep(subtree);
}

void
dom_insert_subtree_before_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree)
{
    pcdom_node_t *to = pcdom_interface_node(element);

    if (subtree && subtree->first_child) {
        pcdom_node_t *div;
        div = subtree->first_child;

        dom_merge_hvml_handle_map(dom_doc, div);
        while (div->last_child) {
            pcdom_node_t *child = div->last_child;
            pcdom_node_remove(child);
            pcdom_node_insert_before(to, child);
        }
    }

    if (subtree)
        pcdom_node_destroy_deep(subtree);
}

void
dom_insert_subtree_after_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree)
{
    pcdom_node_t *to = pcdom_interface_node(element);

    if (subtree && subtree->first_child) {
        pcdom_node_t *div;
        div = subtree->first_child;

        dom_merge_hvml_handle_map(dom_doc, div);
        while (div->first_child) {
            pcdom_node_t *child = div->first_child;
            pcdom_node_remove(child);
            pcdom_node_insert_after(to, child);
        }
    }

    if (subtree)
        pcdom_node_destroy_deep(subtree);
}

void
dom_displace_subtree_of_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree)
{
    pcdom_node_t *parent = pcdom_interface_node(element);

    dom_subtract_hvml_handle_map(dom_doc, parent);
    while (parent->first_child != NULL) {
        pcdom_node_destroy_deep(parent->first_child);
    }

    if (subtree && subtree->first_child) {
        pcdom_node_t *div;
        div = subtree->first_child;

        dom_merge_hvml_handle_map(dom_doc, div);
        while (div->first_child) {
            pcdom_node_t *child = div->first_child;
            pcdom_node_remove(child);
            pcdom_node_append_child(parent, child);
        }
    }

    if (subtree)
        pcdom_node_destroy_deep(subtree);
}

void
dom_destroy_subtree(pcdom_node_t *subtree)
{
    pcdom_node_destroy_deep(subtree);
}

void
dom_erase_element(pcdom_document_t *dom_doc, pcdom_element_t *element)
{
    pcdom_node_t *node = pcdom_interface_node(element);
    uint64_t handle;

    handle = get_hvml_handle(node);

    dom_subtract_hvml_handle_map(dom_doc, node);
    pcdom_node_destroy_deep(node);

    if (handle) {
        struct my_dom_user_data *user = dom_doc->user;
        if (!sorted_array_remove(user->sa, handle)) {
            ULOG_WARN("Failed to store handle/node pair\n");
        }
    }
}

void
dom_clear_element(pcdom_document_t *dom_doc, pcdom_element_t *element)
{
    pcdom_node_t *parent = pcdom_interface_node(element);
    dom_subtract_hvml_handle_map(dom_doc, parent);
    while (parent->first_child != NULL) {
        pcdom_node_destroy_deep(parent->first_child);
    }
}

bool
dom_update_element(pcdom_document_t *dom_doc, pcdom_element_t *element,
        const char* property, const char* content, size_t sz_cnt)
{
    bool retv;

    if (strcmp(property, "textContent") == 0) {
        pcdom_node_t *parent = pcdom_interface_node(element);
        pcdom_text_t *text_node;

        text_node = pcdom_document_create_text_node(dom_doc,
            (const unsigned char *)content, sz_cnt);
        if (text_node) {
            dom_subtract_hvml_handle_map(dom_doc, parent);
            pcdom_node_replace_all(parent, pcdom_interface_node(text_node));
        }

        retv = text_node ? true : false;
    }
    else if (strncmp(property, "attr.", 5) == 0) {
        property += 5;
        pcdom_attr_t *attr;

        attr = pcdom_element_set_attribute(element,
                (const unsigned char*)property, strlen(property),
                (const unsigned char*)content, sz_cnt);
        retv = attr ? true : false;
    }
    else {
        retv = false;
    }

    return retv;
}

bool
dom_remove_element_attr(pcdom_document_t *dom_doc, pcdom_element_t *element,
        const char* property)
{
    bool retv = false;

    if (strncmp(property, "attr.", 5) == 0) {
        property += 5;

        if (pcdom_element_remove_attribute(element,
                (const unsigned char*)property,
                strlen(property)) == PURC_ERROR_OK)
            retv = true;
    }

    return retv;
}

