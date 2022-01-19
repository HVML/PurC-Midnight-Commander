/**
 * \file dom-viewer.h
 * \brief Header: internal DOM viewer
 */

#ifndef MC_DOM_VIEWER_H
#define MC_DOM_VIEWER_H

#include <purc/purc-dom.h>

#include "lib/global.h"

#include "src/viewer/mcviewer.h"

#include "dom-tree.h"
#include "dom-ele-attrs.h"

/*** typedefs(not structures) and defined constants */

/*** enums */

/*** structures declarations (and typedefs of structures) */

typedef struct WDOMViewInfo {
    pcdom_document_t *doc;
    WDOMTree *dom_tree;
    WEleAttrs *ele_attrs;
    WView *txt_view;
} WDOMViewInfo;

/*** global variables defined in .c file */

/*** declarations of public functions */

/* Shows DOM in the internal DOM viewer */
extern bool domview_viewer (pcdom_document_t *dom_doc);

/*** inline functions */

#endif /* MC_DOM_VIEWER_H */

