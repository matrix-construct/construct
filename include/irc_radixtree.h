/*
 * charybdis: an advanced ircd.
 * irc_radixtree.h: Dictionary-based storage.
 *
 * Copyright (c) 2007-2016 William Pitcock <nenolod -at- dereferenced.org>
 * Copyright (c) 2007-2016 Jilles Tjoelker <jilles -at- stack.nl>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __irc_radixtree_H__
#define __irc_radixtree_H__

struct irc_radixtree;/* defined in ircd/irc_radixtree.c */

struct irc_radixtree_leaf;	/* defined in ircd/irc_radixtree.c */

/*
 * struct irc_radixtree_iteration_state, private.
 */
struct irc_radixtree_iteration_state
{
	struct irc_radixtree_leaf *cur, *next;
	void *pspare[4];
	int ispare[4];
};

/*
 * this is a convenience macro for inlining iteration of dictionaries.
 */
#define IRC_RADIXTREE_FOREACH(element, state, dict) \
	for (irc_radixtree_foreach_start((dict), (state)); (element = irc_radixtree_foreach_cur((dict), (state))); irc_radixtree_foreach_next((dict), (state)))

/*
 * irc_radixtree_create() creates a new patricia tree of the defined resolution.
 * compare_cb is the canonizing function.
 */

extern struct irc_radixtree *irc_radixtree_create(const char *name, void (*canonize_cb)(char *key));

/*
 * irc_radixtree_shutdown() deallocates all heaps used in patricia trees. This is
 * useful on embedded devices with little memory, and/or when you know you won't need
 * any more patricia trees.
 */
extern void irc_radixtree_shutdown(void);

/*
 * irc_radixtree_destroy() destroys all entries in a dtree, and also optionally calls
 * a defined callback function to destroy any data attached to it.
 */
extern void irc_radixtree_destroy(struct irc_radixtree *dtree, void (*destroy_cb)(const char *key, void *data, void *privdata), void *privdata);

/*
 * irc_radixtree_foreach() iterates all entries in a dtree, and also optionally calls
 * a defined callback function to use any data attached to it.
 *
 * To shortcircuit iteration, return non-zero from the callback function.
 */
extern void irc_radixtree_foreach(struct irc_radixtree *dtree, int (*foreach_cb)(const char *key, void *data, void *privdata), void *privdata);

/*
 * irc_radixtree_search() iterates all entries in a dtree, and also optionally calls
 * a defined callback function to use any data attached to it.
 *
 * When the object is found, a non-NULL is returned from the callback, which results
 * in that object being returned to the user.
 */
extern void *irc_radixtree_search(struct irc_radixtree *dtree, void *(*foreach_cb)(const char *key, void *data, void *privdata), void *privdata);

/*
 * irc_radixtree_foreach_start() begins an iteration over all items
 * keeping state in the given struct. If there is only one iteration
 * in progress at a time, it is permitted to remove the current element
 * of the iteration (but not any other element).
 */
extern void irc_radixtree_foreach_start(struct irc_radixtree *dtree, struct irc_radixtree_iteration_state *state);

/*
 * irc_radixtree_foreach_cur() returns the current element of the iteration,
 * or NULL if there are no more elements.
 */
extern void *irc_radixtree_foreach_cur(struct irc_radixtree *dtree, struct irc_radixtree_iteration_state *state);

/*
 * irc_radixtree_foreach_next() moves to the next element.
 */
extern void irc_radixtree_foreach_next(struct irc_radixtree *dtree, struct irc_radixtree_iteration_state *state);

/*
 * irc_radixtree_add() adds a key->value entry to the patricia tree.
 */
extern int irc_radixtree_add(struct irc_radixtree *dtree, const char *key, void *data);

/*
 * irc_radixtree_find() returns data from a dtree for key 'key'.
 */
extern void *irc_radixtree_retrieve(struct irc_radixtree *dtree, const char *key);

/*
 * irc_radixtree_delete() deletes a key->value entry from the patricia tree.
 */
extern void *irc_radixtree_delete(struct irc_radixtree *dtree, const char *key);

/* Low-level functions */
struct irc_radixtree_leaf *irc_radixtree_elem_add(struct irc_radixtree *dtree, const char *key, void *data);
struct irc_radixtree_leaf *irc_radixtree_elem_find(struct irc_radixtree *dtree, const char *key);
void irc_radixtree_elem_delete(struct irc_radixtree *dtree, struct irc_radixtree_leaf *elem);
const char *irc_radixtree_elem_get_key(struct irc_radixtree_leaf *elem);
void irc_radixtree_elem_set_data(struct irc_radixtree_leaf *elem, void *data);
void *irc_radixtree_elem_get_data(struct irc_radixtree_leaf *elem);

unsigned int irc_radixtree_size(struct irc_radixtree *dict);
void irc_radixtree_stats(struct irc_radixtree *dict, void (*cb)(const char *line, void *privdata), void *privdata);
void irc_radixtree_stats_walk(void (*cb)(const char *line, void *privdata), void *privdata);

void irc_radixtree_strcasecanon(char *key);
void irc_radixtree_irccasecanon(char *key);

#endif
