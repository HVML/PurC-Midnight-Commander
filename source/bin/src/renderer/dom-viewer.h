/**
 * \file dom-viewer.h
 * \brief Header: internal DOM viewer
 */

#ifndef MC_DOM_VIEWER_H
#define MC_DOM_VIEWER_H

#include <purc/purc-dom.h>
#include <purc/purc-html.h>

#include "lib/global.h"
#include "lib/widget.h"
#include "lib/list.h"

#include "dom-tree.h"
#include "dom-ele-attrs.h"
#include "dom-content.h"

/*** typedefs(not structures) and defined constants */

/*** enums */

/*** structures declarations (and typedefs of structures) */

typedef struct WDOMViewInfo {
    const char *     file_runner;   /* current file runner name */
    pcdom_document_t *dom_doc;          /* current DOM document */

    WDialog         *dlg;
    WHLine          *caption;
    WDOMTree        *dom_tree;
    WEleAttrs       *ele_attrs;
    WDOMContent     *dom_cnt;
} WDOMViewInfo;

/*** declarations of public functions */

/* Load HTML from a file and show DOM in the internal DOM viewer */
extern bool domview_load_html (const vfs_path_t * file_vpath);

/*** inline functions */

#endif /* MC_DOM_VIEWER_H */

