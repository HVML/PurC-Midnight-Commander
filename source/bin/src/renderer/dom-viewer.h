/**
 * \file dom-viewer.h
 * \brief Header: internal DOM viewer
 */

#ifndef MC_DOM_VIEWER_H
#define MC_DOM_VIEWER_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

struct WDOMView;
typedef struct WDOMView WDOMView;

/*** global variables defined in .c file *********************************************************/

extern bool domview_mouse_move_pages;

/*** declarations of public functions ************************************************************/

/* Creates a new WDOMView object with the given properties. Caveat: the
 * origin is in y-x order, while the extent is in x-y order. */
extern WDOMView *domview_new (int y, int x, int lines, int cols, bool is_panel);

struct pcedom_document;

/* Shows DOM in the internal DOM viewer */
extern bool domview_viewer (struct pcedom_document * edom_doc);

extern bool domview_load (WDOMView * view, struct pcedom_document * edom_doc);

/*** inline functions ****************************************************************************/

#endif /* MC_DOM_VIEWER_H */
