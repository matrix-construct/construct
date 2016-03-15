/*
 * charybdis: an advanced ircd.
 * rb_dictionary.h: Dictionary-based storage.
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

#ifndef __RB_DICTIONARY_H__
#define __RB_DICTIONARY_H__

struct Dictionary; /* defined in src/dictionary.c */

typedef int (*DCF)(/* const void *a, const void *b */);

struct DictionaryElement
{
	struct DictionaryElement *left, *right, *prev, *next;
	void *data;
	const void *key;
	int position;
};

struct DictionaryIter
{
	struct DictionaryElement *cur, *next;
};

/*
 * this is a convenience macro for inlining iteration of dictionaries.
 */
#define DICTIONARY_FOREACH(element, state, dict) for (rb_dictionary_foreach_start((dict), (state)); (element = rb_dictionary_foreach_cur((dict), (state))); rb_dictionary_foreach_next((dict), (state)))

/*
 * rb_dictionary_create_named() creates a new dictionary tree which has a name.
 * name is the name, compare_cb is the comparator.
 */
extern struct Dictionary *rb_dictionary_create(const char *name, DCF compare_cb);

/*
 * rb_dictionary_set_comparator_func() resets the comparator used for lookups and
 * insertions in the DTree structure.
 */
extern void rb_dictionary_set_comparator_func(struct Dictionary *dict,
	DCF compare_cb);

/*
 * rb_dictionary_get_comparator_func() returns the comparator used for lookups and
 * insertions in the DTree structure.
 */
extern DCF rb_dictionary_get_comparator_func(struct Dictionary *dict);

/*
 * rb_dictionary_get_linear_index() returns the linear index of an object in the
 * DTree structure.
 */
extern int rb_dictionary_get_linear_index(struct Dictionary *dict, const void *key);

/*
 * rb_dictionary_destroy() destroys all entries in a dtree, and also optionally calls
 * a defined callback function to destroy any data attached to it.
 */
extern void rb_dictionary_destroy(struct Dictionary *dtree,
	void (*destroy_cb)(struct DictionaryElement *delem, void *privdata),
	void *privdata);

/*
 * rb_dictionary_foreach() iterates all entries in a dtree, and also optionally calls
 * a defined callback function to use any data attached to it.
 *
 * To shortcircuit iteration, return non-zero from the callback function.
 */
extern void rb_dictionary_foreach(struct Dictionary *dtree,
	int (*foreach_cb)(struct DictionaryElement *delem, void *privdata),
	void *privdata);

/*
 * rb_dictionary_search() iterates all entries in a dtree, and also optionally calls
 * a defined callback function to use any data attached to it.
 *
 * When the object is found, a non-NULL is returned from the callback, which results
 * in that object being returned to the user.
 */
extern void *rb_dictionary_search(struct Dictionary *dtree,
	void *(*foreach_cb)(struct DictionaryElement *delem, void *privdata),
	void *privdata);

/*
 * rb_dictionary_foreach_start() begins an iteration over all items
 * keeping state in the given struct. If there is only one iteration
 * in progress at a time, it is permitted to remove the current element
 * of the iteration (but not any other element).
 */
extern void rb_dictionary_foreach_start(struct Dictionary *dtree,
	struct DictionaryIter *state);

/*
 * rb_dictionary_foreach_cur() returns the current element of the iteration,
 * or NULL if there are no more elements.
 */
extern void *rb_dictionary_foreach_cur(struct Dictionary *dtree,
	struct DictionaryIter *state);

/*
 * rb_dictionary_foreach_next() moves to the next element.
 */
extern void rb_dictionary_foreach_next(struct Dictionary *dtree,
	struct DictionaryIter *state);

/*
 * rb_dictionary_add() adds a key->value entry to the dictionary tree.
 */
extern struct DictionaryElement *rb_dictionary_add(struct Dictionary *dtree, const void *key, void *data);

/*
 * rb_dictionary_find() returns a struct DictionaryElement container from a dtree for key 'key'.
 */
extern struct DictionaryElement *rb_dictionary_find(struct Dictionary *dtree, const void *key);

/*
 * rb_dictionary_find() returns data from a dtree for key 'key'.
 */
extern void *rb_dictionary_retrieve(struct Dictionary *dtree, const void *key);

/*
 * rb_dictionary_delete() deletes a key->value entry from the dictionary tree.
 */
extern void *rb_dictionary_delete(struct Dictionary *dtree, const void *key);

/*
 * rb_dictionary_size() returns the number of elements in a dictionary tree.
 */
extern unsigned int rb_dictionary_size(struct Dictionary *dtree);

void rb_dictionary_stats(struct Dictionary *dict, void (*cb)(const char *line, void *privdata), void *privdata);
void rb_dictionary_stats_walk(void (*cb)(const char *line, void *privdata), void *privdata);

#define RB_POINTER_TO_INT(x)		((int32_t) (long) (x))
#define RB_INT_TO_POINTER(x)		((void *) (long) (int32_t) (x))

#define RB_POINTER_TO_UINT(x)		((uint32_t) (unsigned long) (x))
#define RB_UINT_TO_POINTER(x)		((void *) (unsigned long) (uint32_t) (x))

#define RB_POINTER_TO_LONG(x)		((int64_t) (unsigned long long) (x))
#define RB_LONG_TO_POINTER(x)		((void *) (unsigned long long) (int64_t) (x))

#define RB_POINTER_TO_ULONG(x)		((uint64_t) (unsigned long long) (x))
#define RB_ULONG_TO_POINTER(x)		((void *) (unsigned long long) (uint64_t) (x))

static inline int rb_int32cmp(const void *a, const void *b)
{
	return RB_POINTER_TO_INT(b) - RB_POINTER_TO_INT(a);
}

static inline int rb_uint32cmp(const void *a, const void *b)
{
	return RB_POINTER_TO_UINT(b) - RB_POINTER_TO_UINT(a);
}

static inline int rb_int64cmp(const void *a, const void *b)
{
	return RB_POINTER_TO_LONG(b) - RB_POINTER_TO_LONG(a);
}

static inline int rb_uint64cmp(const void *a, const void *b)
{
	return RB_POINTER_TO_ULONG(b) - RB_POINTER_TO_ULONG(a);
}

#endif
