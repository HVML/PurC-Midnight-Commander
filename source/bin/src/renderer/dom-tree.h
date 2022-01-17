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
 * \file dom-tree.h
 * \brief Header: DOM tree widget
 */

#ifndef MC_DOM_TREE_H
#define MC_DOM_TREE_H

#include <purc/purc-dom.h>
#include <purc/purc-html.h>

#include "lib/global.h"

/*** typedefs(not structures) and defined constants */

/*** enums */

/*** structures declarations (and typedefs of structures) */

typedef struct WDOMTree WDOMTree;

/*** global variables defined in .c file */

/*** declarations of public functions */

WDOMTree *dom_tree_new (int y, int x, int lines, int cols, bool is_panel);

bool dom_tree_load (WDOMTree *tree, pcdom_document_t *doc);

WDOMTree *find_dom_tree (const WDialog * h);

/*** inline functions */

#endif /* MC_DOM_TREE_H */
