/*
   Functions to handle DOM text .

   Copyright (C) 2022
   Beijing FMSoft Technologies Co., Ltd.

   Written by:
   Vincent Wei <vincent@minigui.org>, 2022

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

#include <config.h>

#include "lib/global.h"
#include "lib/tty/tty.h"
#include "lib/skin.h"
#include "lib/util.h"           /* is_printable() */

#include "dom-text.h"

/*** global variables ********************************************************/

/*** file scope macro definitions ********************************************/

#define UNICHAR_REPLACEMENT             0xFFFD
#define UTF8LEN_REPLACEMENT             3

#define UNICHAR_HORIZ_ELLIPSIS          0x2026
#define UTF8LEN_HORIZ_ELLIPSIS          3

/*** file scope type declarations ********************************************/

/*** file scope variables ****************************************************/

/*** file scope functions ****************************************************/

static inline int
dom_text_wcwidth (int c)
{
    if (g_unichar_iswide (c))
        return 2;
    if (g_unichar_iszerowidth (c))
        return 0;

    return 1;
}

static inline gboolean
dom_text_ismark (int c)
{
    return g_unichar_ismark (c);
}

/* actually is_non_spacing_mark_or_enclosing_mark */
static inline gboolean
dom_text_is_non_spacing_mark (int c)
{
    GUnicodeType type;

    type = g_unichar_type (c);
    return type == G_UNICODE_NON_SPACING_MARK || type == G_UNICODE_ENCLOSING_MARK;
}

static inline gboolean
dom_text_isprint (int c)
{
    return g_unichar_isprint (c);
}

gboolean
dom_text_normalize (GString *string)
{
    const gchar *text, *tail;
    gunichar c;
    gchar utf8buf[UTF8_CHAR_LEN + 1];
    gint utf8_len;

    /* trim all whitespaces at the head. */
    gsize len_to_erase = 0;
    text = string->str;
    while (*text) {
        c = g_utf8_get_char_validated (text, -1);
        if (c < 0) {
            goto invalid;
        }
        else if (g_unichar_isspace (c)) {
            utf8_len = g_unichar_to_utf8 (c, utf8buf);
            len_to_erase += utf8_len;
            text += utf8_len;
        }
        else
            break;
    }

    if (len_to_erase > 0) {
        string = g_string_erase (string, 0, len_to_erase);
    }

    if (string->len == 0) {
        return TRUE;
    }

    /* trim all whitespaces at the head. */
    tail = string->str + string->len - 1;
    gsize len_to_truncate = 0;
    while (tail > string->str) {
        tail = g_utf8_find_prev_char (string->str, tail);
        if (tail == NULL) {
            break;
        }

        c = g_utf8_get_char_validated (tail, -1);
        if (c < 0) {
            goto invalid;
        }
        else if (g_unichar_isspace (c)) {
            utf8_len = g_unichar_to_utf8 (c, utf8buf);
            len_to_truncate += utf8_len;
        }
        else
            break;
    }

    if (len_to_truncate > 0) {
        string = g_string_truncate (string, string->len - len_to_truncate);
    }

    if (string->len == 0) {
        return TRUE;
    }

    /*
       replace all whitespaces with ' ';
       replace all non-mark and non-printable characters
       with U+FFFD REPLACEMENT CHARACTER
    */
    text = string->str;
    while (*text) {
        c = g_utf8_get_char_validated (text, -1);

        if (c < 0) {
            goto invalid;
        }

        utf8_len = g_unichar_to_utf8 (c, utf8buf);
        if (g_unichar_isspace (c)) {
            gssize pos = text - string->str;

            g_string_erase (string, pos, utf8_len);
            g_string_insert_c (string, pos, ' ');
            text += 1;
        }
        else if (!g_unichar_ismark (c) && !g_unichar_isprint (c)) {
            gssize pos = text - string->str;

            g_string_erase (string, pos, utf8_len);
            g_string_insert_unichar (string, pos, UNICHAR_REPLACEMENT);
            text += UTF8LEN_REPLACEMENT;
        }
        else {
            text += utf8_len;
        }
    }

    /* merge all consecutive whitespaces to one space. */
    text = string->str;
    bool found_space = false;
    size_t pos = 0;
    while (string->str[pos]) {
        if (*text == ' ') {
            if (found_space) {
                g_string_erase (string, pos, 1);
            }
            else
                found_space = true;
        }
        else {
            found_space = false;
        }

        pos++;
    }

    return TRUE;

invalid:
    g_string_assign (string, "");
    return FALSE;
}

gboolean
dom_text_truncate_with_ellipsis (GString *string, unsigned int max_chars)
{
    if (string->len <= max_chars)
        return TRUE;

    const gchar *text = string->str;
    gunichar c;
    gchar utf8buf[UTF8_CHAR_LEN + 1];
    gint utf8_len;
    unsigned int nr_chars = 0;

    while (*text) {
        c = g_utf8_get_char_validated (text, -1);

        if (c < 0) {
            goto invalid;
        }

        utf8_len = g_unichar_to_utf8 (c, utf8buf);
        if (!g_unichar_ismark (c)) {
            nr_chars++;
        }

        text += utf8_len;

        if (nr_chars == max_chars) {
            gssize pos = text - string->str;

            g_string_truncate (string, pos);
            g_string_append_unichar (string, UNICHAR_HORIZ_ELLIPSIS);
            break;
        }
    }

    return TRUE;

invalid:
    return FALSE;
}

gboolean
dom_text_display_normalized_nowrap (const char* text, unsigned int flags,
        int y, int x, unsigned int width)
{
    return TRUE;
}

