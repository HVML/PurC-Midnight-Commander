/**
 * \file dom-ops.h
 * \brief Header: DOM operations
 */

#ifndef MC_DOM_OPS_H
#define MC_DOM_OPS_H

#include <purc/purc-dom.h>
#include <purc/purc-html.h>

/* build the map from HVML handles to elements */
bool dom_build_hvml_handle_map(pcdom_document_t *dom_doc);

bool dom_merge_hvml_handle_map(pcdom_document_t *dom_doc,
        pcdom_node_t *subtree);

bool dom_subtract_hvml_handle_map(pcdom_document_t *dom_doc,
        pcdom_node_t *subtree);

bool dom_destroy_hvml_handle_map(pcdom_document_t *dom_doc);

pcdom_element_t *dom_get_element_by_handle(pcdom_document_t *dom_doc,
        uintptr_t handle);

pcdom_node_t *dom_parse_fragment(pcdom_document_t *dom_doc,
        const char *fragment, size_t length);

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

/*** inline functions */

#endif /* MC_DOM_OPS_H */

