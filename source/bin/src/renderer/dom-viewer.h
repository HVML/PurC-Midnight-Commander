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
    const char *     file_window;       /* current file or window */
    pcdom_document_t *dom_doc;          /* current DOM document */

    WDialog         *dlg;
    WHLine          *caption;
    WDOMTree        *dom_tree;
    WEleAttrs       *ele_attrs;
    WDOMContent     *dom_cnt;
} WDOMViewInfo;

/*** declarations of public functions */

/* Show DOM viewer. Return @false when there is no any file or window */
extern bool
domview_show(void);

/* Load HTML from a file and show the DOM Document */
extern bool
domview_load_html(const vfs_path_t * file_vpath);

/* Attach a DOM Document created by a remote endpoint */
extern bool
domview_attach_window_dom(const char *endpoint,
        const char* win_id, const char *title, pcdom_document_t *dom_doc);

/* Detach a DOM Document created by a remote endpoint */
extern bool
domview_detach_window_dom(const char *endpoint, const char* win_id);

/* Reload a DOM Document changed by a remote endpoint */
extern bool
domview_reload_window_dom(const char *endpoint, const char* win_id,
        pcdom_element_t *element);

/*** inline functions */

#endif /* MC_DOM_VIEWER_H */

