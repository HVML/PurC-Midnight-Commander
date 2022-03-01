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

struct my_tree_walker_ctxt {
    bool add_or_remove;
    struct sorted_array *sa;
};

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
                if (sorted_array_remove(ctxt->sa, handle)) {
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

bool
dom_build_hvml_handle_map(pcdom_document_t *dom_doc)
{
    struct sorted_array *sa;

    assert(dom_doc->user == NULL);
    sa = sorted_array_create(SAFLAG_DEFAULT, SA_INITIAL_SIZE, NULL, NULL);
    if (sa == NULL) {
        return false;
    }

    struct my_tree_walker_ctxt ctxt = {
        .add_or_remove  = true,
        .sa             = sa,
    };

    pcdom_node_simple_walk(&dom_doc->node, my_tree_walker, &ctxt);
    dom_doc->user = sa;
    return true;
}

bool
dom_merge_hvml_handle_map(pcdom_document_t *dom_doc, pcdom_node_t *subtree)
{
    struct sorted_array *sa;
    sa = dom_doc->user;
    if (sa == NULL) {
        return false;
    }

    struct my_tree_walker_ctxt ctxt = {
        .add_or_remove  = true,
        .sa             = sa,
    };

    pcdom_node_simple_walk(subtree, my_tree_walker, &ctxt);
    return true;
}

bool
dom_subtract_hvml_handle_map(pcdom_document_t *dom_doc, pcdom_node_t *subtree)
{
    struct sorted_array *sa;
    sa = dom_doc->user;
    if (sa == NULL) {
        return false;
    }

    struct my_tree_walker_ctxt ctxt = {
        .add_or_remove  = false,
        .sa             = sa,
    };

    pcdom_node_simple_walk(subtree, my_tree_walker, &ctxt);
    return true;
}

bool
dom_destroy_hvml_handle_map(pcdom_document_t *dom_doc)
{
    struct sorted_array *sa;

    sa = dom_doc->user;
    if (sa == NULL) {
        return false;
    }

    sorted_array_destroy(sa);
    dom_doc->user = NULL;
    return true;
}

pcdom_element_t *
dom_get_element_by_handle(pcdom_document_t *dom_doc, uint64_t handle)
{
    void* data;
    struct sorted_array *sa = dom_doc->user;

    assert(sa);

    if (sorted_array_find(sa, handle, &data))
        return (pcdom_element_t *)data;

    return NULL;
}

pcdom_node_t *
dom_parse_fragment(pcdom_document_t *dom_doc,
        const char *fragment, size_t length)
{
    return NULL;
}

pcdom_node_t *
dom_clone_subtree(pcdom_document_t *dom_doc, pcdom_node_t *subtree,
        uint64_t handle_msb)
{
    return NULL;
}

void
dom_append_subtree_to_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree)
{
    return;
}

void
dom_prepend_subtree_to_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree)
{
}

void
dom_insert_subtree_before_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree)
{
}

void
dom_insert_subtree_after_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree)
{
}

void
dom_displace_subtree_of_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree)
{
}

void
dom_destroy_subtree(pcdom_node_t *subtree)
{
    return;
}

void
dom_erase_element(pcdom_document_t *dom_doc, pcdom_element_t *element)
{
}

void
dom_clear_element(pcdom_document_t *dom_doc, pcdom_element_t *element)
{
}

bool
dom_update_element(pcdom_document_t *dom_doc, pcdom_element_t *element,
        const char* property, const char* content, size_t sz_cnt)
{
    return false;
}

