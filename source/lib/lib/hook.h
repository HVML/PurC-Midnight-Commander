/** \file lib/hook.h
 *  \brief Header: hooks
 */

#ifndef MC_HOOK_H
#define MC_HOOK_H

#include "lib/global.h"

/*** typedefs(not structures) and defined constants **********************************************/

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct hook_t
{
    void (*hook_fn) (void *, void *);
    void *hook_data;
    struct hook_t *next;
} hook_t;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

void add_hook (hook_t ** hook_list, void (*hook_fn) (void *, void *), void *data);
void execute_hooks (hook_t * hook_list, void *info);
void delete_hook (hook_t ** hook_list, void (*hook_fn) (void *, void *));
gboolean hook_present (hook_t * hook_list, void (*hook_fn) (void *, void *));

/*** inline functions **************************************************/

#endif /* MC_HOOK_H */
