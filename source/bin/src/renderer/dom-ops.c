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
#include <errno.h>
#include <stdbool.h>
#include <assert.h>

#include "dom-ops.h"

bool
dom_build_hvml_handle_map(pcdom_document_t *dom_doc)
{
    return false;
}

pcdom_element_t *
dom_get_element_by_handle(pcdom_document_t *dom_doc, uintptr_t handle)
{
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

