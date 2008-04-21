/*
 * charybdis: an advanced ircd.
 * irc_dictionary.h: Dictionary-based storage.
 *
 * Copyright (c) 2007 William Pitcock <nenolod -at- sacredspiral.co.uk>
 * Copyright (c) 2007 Jilles Tjoelker <jilles -at- stack.nl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
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

#ifndef __IRC_DICTIONARY_H__
#define __IRC_DICTIONARY_H__

struct Dictionary; /* defined in src/dictionary.c */

typedef int (*DCF)(const char *a, const char *b);

struct DictionaryElement
{
	struct DictionaryElement *left, *right, *prev, *next;
	void *data;
	const char *key;
	int position;
};

struct DictionaryIter
{
	struct DictionaryElement *cur, *next;
};

/*
 * this is a convenience macro for inlining iteration of dictionaries.
 */
#define DICTIONARY_FOREACH(element, state, dict) for (irc_dictionary_foreach_start((dict), (state)); (element = irc_dictionary_foreach_cur((dict), (state))); irc_dictionary_foreach_next((dict), (state)))

/*
 * irc_dictionary_create() creates a new dictionary tree.
 * compare_cb is the comparison function, typically strcmp, strcasecmp or
 * irccasecmp.
 */
extern struct Dictionary *irc_dictionary_create(DCF compare_cb);

/*
 * irc_dictionary_create_named() creates a new dictionary tree which has a name.
 * name is the name, compare_cb is the comparator.
 */
extern struct Dictionary *irc_dictionary_create_named(const char *name, DCF compare_cb);

/*
 * irc_dictionary_set_comparator_func() resets the comparator used for lookups and
 * insertions in the DTree structure.
 */
extern void irc_dictionary_set_comparator_func(struct Dictionary *dict,
	DCF compare_cb);

/*
 * irc_dictionary_get_comparator_func() returns the comparator used for lookups and
 * insertions in the DTree structure.
 */
extern DCF irc_dictionary_get_comparator_func(struct Dictionary *dict);

/*
 * irc_dictionary_get_linear_index() returns the linear index of an object in the
 * DTree structure.
 */
extern int irc_dictionary_get_linear_index(struct Dictionary *dict, const char *key);

/*
 * irc_dictionary_destroy() destroys all entries in a dtree, and also optionally calls
 * a defined callback function to destroy any data attached to it.
 */
extern void irc_dictionary_destroy(struct Dictionary *dtree,
	void (*destroy_cb)(struct DictionaryElement *delem, void *privdata),
	void *privdata);

/*
 * irc_dictionary_foreach() iterates all entries in a dtree, and also optionally calls
 * a defined callback function to use any data attached to it.
 *
 * To shortcircuit iteration, return non-zero from the callback function.
 */
extern void irc_dictionary_foreach(struct Dictionary *dtree,
	int (*foreach_cb)(struct DictionaryElement *delem, void *privdata),
	void *privdata);

/*
 * irc_dictionary_search() iterates all entries in a dtree, and also optionally calls
 * a defined callback function to use any data attached to it.
 *
 * When the object is found, a non-NULL is returned from the callback, which results
 * in that object being returned to the user.
 */
extern void *irc_dictionary_search(struct Dictionary *dtree,
	void *(*foreach_cb)(struct DictionaryElement *delem, void *privdata),
	void *privdata);

/*
 * irc_dictionary_foreach_start() begins an iteration over all items
 * keeping state in the given struct. If there is only one iteration
 * in progress at a time, it is permitted to remove the current element
 * of the iteration (but not any other element).
 */
extern void irc_dictionary_foreach_start(struct Dictionary *dtree,
	struct DictionaryIter *state);

/*
 * irc_dictionary_foreach_cur() returns the current element of the iteration,
 * or NULL if there are no more elements.
 */
extern void *irc_dictionary_foreach_cur(struct Dictionary *dtree,
	struct DictionaryIter *state);

/*
 * irc_dictionary_foreach_next() moves to the next element.
 */
extern void irc_dictionary_foreach_next(struct Dictionary *dtree,
	struct DictionaryIter *state);

/*
 * irc_dictionary_add() adds a key->value entry to the dictionary tree.
 */
extern struct DictionaryElement *irc_dictionary_add(struct Dictionary *dtree, const char *key, void *data);

/*
 * irc_dictionary_find() returns a struct DictionaryElement container from a dtree for key 'key'.
 */
extern struct DictionaryElement *irc_dictionary_find(struct Dictionary *dtree, const char *key);

/*
 * irc_dictionary_find() returns data from a dtree for key 'key'.
 */
extern void *irc_dictionary_retrieve(struct Dictionary *dtree, const char *key);

/*
 * irc_dictionary_delete() deletes a key->value entry from the dictionary tree.
 */
extern void *irc_dictionary_delete(struct Dictionary *dtree, const char *key);

/*
 * irc_dictionary_size() returns the number of elements in a dictionary tree.
 */
extern unsigned int irc_dictionary_size(struct Dictionary *dtree);

void irc_dictionary_stats(struct Dictionary *dict, void (*cb)(const char *line, void *privdata), void *privdata);

#endif
