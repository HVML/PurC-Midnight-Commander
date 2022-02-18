/*
   Functions to handle DOM text.

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
 * \file dom-text.h
 * \brief Header: DOM text
 */

#ifndef MC_DOM_TEXT_H
#define MC_DOM_TEXT_H

/*** typedefs(not structures) and defined constants */
#define DOMTEXT_OVERFLOW_ELLIPSIZE_MASK     0x000F
#define DOMTEXT_OVERFLOW_ELLIPSIZE_NONE     0x0000
#define DOMTEXT_OVERFLOW_ELLIPSIZE_STGART   0x0001
#define DOMTEXT_OVERFLOW_ELLIPSIZE_MIDDLE   0x0002
#define DOMTEXT_OVERFLOW_ELLIPSIZE_END      0x0003

#define DOMTEXT_ALIGN_MASK                  0x00F0
#define DOMTEXT_ALIGN_LEFT                  0x0000
#define DOMTEXT_ALIGN_RIGHT                 0x0010
#define DOMTEXT_ALIGN_CENTER                0x0020

/*** enums */

/*** structures declarations (and typedefs of structures) */

/*** global variables defined in .c file */

/*** declarations of public functions */

gboolean dom_text_normalize (GString *text);
gboolean dom_text_display_normalized_nowrap (const char* text,
        unsigned int flags, int y, int x, unsigned int width);

/*** inline functions */

#endif /* MC_DOM_TEXT_H */
