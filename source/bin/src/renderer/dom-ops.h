/**
 * \file dom-ops.h
 * \brief Header: DOM operations
 */

#ifndef MC_DOM_OPS_H
#define MC_DOM_OPS_H

#include <purc/purc-dom.h>
#include <purc/purc-html.h>

#include "lib/sorted-array.h"

#include "dom-tree.h"

#define NF_UNFOLDED         0x0001
#define NF_DIRTY            0x0002

struct my_dom_user_data {
    struct sorted_array *sa;    // handle to element map
    char                *title; // the title
    WDOMTree            *tree;  // the DOMTree widget
};

bool dom_prepare_user_data(pcdom_document_t *dom_doc, bool with_handle);

bool dom_cleanup_user_data(pcdom_document_t *dom_doc);

bool dom_merge_hvml_handle_map(pcdom_document_t *dom_doc,
        pcdom_node_t *subtree);

bool dom_subtract_hvml_handle_map(pcdom_document_t *dom_doc,
        pcdom_node_t *subtree);

pcdom_element_t *dom_get_element_by_handle(pcdom_document_t *dom_doc,
        uintptr_t handle);

char *dom_set_title(pcdom_document_t *dom_doc, const char *title);

pcdom_node_t *dom_parse_fragment(pcdom_document_t *dom_doc,
        pcdom_element_t *parent, const char *fragment, size_t length);

pcdom_node_t *dom_clone_subtree(pcdom_document_t *dom_doc,
        pcdom_node_t *subtree, uint64_t handle_msb);

void dom_append_subtree_to_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree);

void dom_prepend_subtree_to_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree);

void dom_insert_subtree_before_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree);

void dom_insert_subtree_after_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree);

void dom_displace_subtree_of_element(pcdom_document_t *dom_doc,
        pcdom_element_t *element, pcdom_node_t *subtree);

void dom_destroy_subtree(pcdom_node_t *subtree);

void dom_erase_element(pcdom_document_t *dom_doc, pcdom_element_t *element);

void dom_clear_element(pcdom_document_t *dom_doc, pcdom_element_t *element);

bool dom_update_element(pcdom_document_t *dom_doc, pcdom_element_t *element,
        const char* property, const char* content, size_t sz_cnt);

bool dom_remove_element_attr(pcdom_document_t *dom_doc, pcdom_element_t *element,
        const char* property);

/*** inline functions */

#endif /* MC_DOM_OPS_H */

