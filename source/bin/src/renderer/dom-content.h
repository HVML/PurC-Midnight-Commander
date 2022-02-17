/*
   DOM content widget for the PurC Midnight Commander

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
 * \file dom-content.h
 * \brief Header: DOM content
 */

#ifndef MC_DOM_CONTENT_H
#define MC_DOM_CONTENT_H

/*** typedefs(not structures) and defined constants */

/*** enums */

/*** structures declarations (and typedefs of structures) */

struct viewport
{
    unsigned int top, left;
    unsigned int height, width;
};

typedef struct
{
    off_t offset;               /* The file offset at which this is the state. */
    off_t unwrapped_column;     /* Columns if the paragraph wasn't wrapped, */
    /* used for positioning TABs in wrapped lines */
    gboolean nroff_underscore_is_underlined;    /* whether _\b_ is underlined rather than bold */
    gboolean print_lonely_combining;    /* whether lonely combining marks are printed on a dotted circle */
} domcnt_formatter_state_t;

typedef struct
{
    gboolean wrap;              /* Wrap text lines to fit them on the screen */
    gboolean nroff;             /* Nroff-style highlighting */
} domcnt_mode_flags_t;

struct WDOMContent
{
    Widget widget;
    const char *title;
    const char *show_eof;

    gchar *text;
    gsize text_len;

    struct viewport data_area;  /* Where the text is displayed */

    domcnt_mode_flags_t mode_flags;

    ssize_t force_max;          /* Force a max offset, or -1 */

    off_t dpy_start;            /* Offset of the displayed data (start of the paragraph in non-hex mode) */
    off_t dpy_end;              /* Offset after the displayed data */
    off_t dpy_text_column;      /* Number of skipped columns in non-wrap text mode */
    off_t dpy_paragraph_skip_lines;     /* Extra lines to skip in wrap mode */
    gboolean dpy_wrap_dirty;    /* dpy_state_top needs to be recomputed */

    domcnt_formatter_state_t dpy_state_top;       /* Parser-formatter state at the topmost visible line in wrap mode */
    domcnt_formatter_state_t dpy_state_bottom;    /* Parser-formatter state after the bottomvisible line in wrap mode */
};

typedef struct WDOMContent WDOMContent;

/*** global variables defined in .c file */

/*** declarations of public functions */

WDOMContent *dom_content_new (int y, int x, int lines, int cols,
        const char* title, const char *show_eof);
bool dom_content_load (WDOMContent *domcnt, GString *string);

off_t domcnt_bol (WDOMContent * view, off_t current, off_t limit);
off_t domcnt_eol (WDOMContent * view, off_t current);

void domcnt_display_text (WDOMContent * view);
void domcnt_text_move_down (WDOMContent * view, off_t lines);
void domcnt_text_move_up (WDOMContent * view, off_t lines);
void domcnt_text_moveto_bol (WDOMContent * view);
void domcnt_text_moveto_eol (WDOMContent * view);
void domcnt_formatter_state_init (domcnt_formatter_state_t * state, off_t offset);

/*** inline functions */

#endif /* MC_DOM_CONTENT_H */
