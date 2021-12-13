/*
 * sorted_array - simple sorted array
 *
 * Copyright (C) 2021 FMSoft <https://www.fmsoft.cn>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef __MC_LIB_SORTED_ARRAY_H
#define __MC_LIB_SORTED_ARRAY_H

#include <stdbool.h>

struct sorted_array;

typedef void (*sacb_free)(void *sortv, void *data);
typedef int  (*sacb_compare)(const void *sortv1, const void *sortv2);

#define SAFLAG_ORDER_ASC            0x0000
#define SAFLAG_ORDER_DESC           0x0001
#define SAFLAG_DUPLCATE_SORTV       0x0002

#define SAFLAG_DEFAULT              0x0000

#ifdef __cplusplus
extern "C" {
#endif

/* create an empty sorted array; free_fn can be NULL */
struct sorted_array *
sorted_array_create(unsigned int flags, size_t sz_init,
        sacb_free free_fn, sacb_compare cmp_fn);

/* destroy a sorted array */
void sorted_array_destroy(struct sorted_array *sa);

/* add a new member with the sort value and the data. */
int sorted_array_add(struct sorted_array *sa, void *sortv, void *data);

/* remove one member which has the same sort value. */
bool sorted_array_remove(struct sorted_array *sa, const void* sortv);

/* find the first member which has the same sort value. */
bool sorted_array_find(struct sorted_array *sa,
        const void *sortv, void **data);

/* retrieve the number of the members of the sorted array */
size_t sorted_array_count(struct sorted_array *sa);

/* retrieve the member by the index and return the sort value;
   data can be NULL. */
const void* sorted_array_get(struct sorted_array *sa, size_t idx, void **data);

/* delete the member by the index */
void sorted_array_delete(struct sorted_array *sa, size_t idx);

#ifdef __cplusplus
}
#endif

#endif  /* __MC_LIB_SORTED_ARRAY_H */