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
#ifdef __cplusplus
extern "C" {
#endif

typedef struct rb_dictionary rb_dictionary;
typedef struct rb_dictionary_element rb_dictionary_element;
typedef struct rb_dictionary_iter rb_dictionary_iter;

struct rb_dictionary;

#ifndef __cplusplus
	// This comparator could be based on a union of function types emulating a
	// quasi-template, shutting up a lot of warnings. For now it gets The Treatment.
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wstrict-prototypes"
		typedef int (*DCF)(/* const void *a, const void *b */);
	#pragma GCC diagnostic pop
#else
	typedef int (*DCF)(const void *a, const void *b);
#endif

struct rb_dictionary_element
{
	rb_dictionary_element *left, *right, *prev, *next;
	void *data;
	const void *key;
	int position;
};

struct rb_dictionary_iter
{
	rb_dictionary_element *cur, *next;
};

/*
 * this is a convenience macro for inlining iteration of dictionaries.
 */
#define RB_DICTIONARY_FOREACH(element, state, dict) for (rb_dictionary_foreach_start((dict), (state)); (element = rb_dictionary_foreach_cur((dict), (state))); rb_dictionary_foreach_next((dict), (state)))

/*
 * rb_dictionary_create_named() creates a new dictionary tree which has a name.
 * name is the name, compare_cb is the comparator.
 */
extern rb_dictionary *rb_dictionary_create(const char *name, DCF compare_cb);

/*
 * rb_dictionary_set_comparator_func() resets the comparator used for lookups and
 * insertions in the DTree structure.
 */
extern void rb_dictionary_set_comparator_func(rb_dictionary *dict,
	DCF compare_cb);

/*
 * rb_dictionary_get_comparator_func() returns the comparator used for lookups and
 * insertions in the DTree structure.
 */
extern DCF rb_dictionary_get_comparator_func(rb_dictionary *dict);

/*
 * rb_dictionary_get_linear_index() returns the linear index of an object in the
 * DTree structure.
 */
extern int rb_dictionary_get_linear_index(rb_dictionary *dict, const void *key);

/*
 * rb_dictionary_destroy() destroys all entries in a dtree, and also optionally calls
 * a defined callback function to destroy any data attached to it.
 */
extern void rb_dictionary_destroy(rb_dictionary *dtree,
	void (*destroy_cb)(rb_dictionary_element *delem, void *privdata),
	void *privdata);

/*
 * rb_dictionary_foreach() iterates all entries in a dtree, and also optionally calls
 * a defined callback function to use any data attached to it.
 *
 * To shortcircuit iteration, return non-zero from the callback function.
 */
extern void rb_dictionary_foreach(rb_dictionary *dtree,
	int (*foreach_cb)(rb_dictionary_element *delem, void *privdata),
	void *privdata);

/*
 * rb_dictionary_search() iterates all entries in a dtree, and also optionally calls
 * a defined callback function to use any data attached to it.
 *
 * When the object is found, a non-NULL is returned from the callback, which results
 * in that object being returned to the user.
 */
extern void *rb_dictionary_search(rb_dictionary *dtree,
	void *(*foreach_cb)(rb_dictionary_element *delem, void *privdata),
	void *privdata);

/*
 * rb_dictionary_foreach_start() begins an iteration over all items
 * keeping state in the given struct. If there is only one iteration
 * in progress at a time, it is permitted to remove the current element
 * of the iteration (but not any other element).
 */
extern void rb_dictionary_foreach_start(rb_dictionary *dtree,
	rb_dictionary_iter *state);

/*
 * rb_dictionary_foreach_cur() returns the current element of the iteration,
 * or NULL if there are no more elements.
 */
extern void *rb_dictionary_foreach_cur(rb_dictionary *dtree,
	rb_dictionary_iter *state);

/*
 * rb_dictionary_foreach_next() moves to the next element.
 */
extern void rb_dictionary_foreach_next(rb_dictionary *dtree,
	rb_dictionary_iter *state);

/*
 * rb_dictionary_add() adds a key->value entry to the dictionary tree.
 */
extern rb_dictionary_element *rb_dictionary_add(rb_dictionary *dtree, const void *key, void *data);

/*
 * rb_dictionary_find() returns a rb_dictionary_element container from a dtree for key 'key'.
 */
extern rb_dictionary_element *rb_dictionary_find(rb_dictionary *dtree, const void *key);

/*
 * rb_dictionary_find() returns data from a dtree for key 'key'.
 */
extern void *rb_dictionary_retrieve(rb_dictionary *dtree, const void *key);

/*
 * rb_dictionary_delete() deletes a key->value entry from the dictionary tree.
 */
extern void *rb_dictionary_delete(rb_dictionary *dtree, const void *key);

/*
 * rb_dictionary_size() returns the number of elements in a dictionary tree.
 */
extern unsigned int rb_dictionary_size(rb_dictionary *dtree);

void rb_dictionary_stats(rb_dictionary *dict, void (*cb)(const char *line, void *privdata), void *privdata);
void rb_dictionary_stats_walk(void (*cb)(const char *line, void *privdata), void *privdata);

#ifndef _WIN32

#define RB_POINTER_TO_INT(x)		((int32_t) (long) (x))
#define RB_INT_TO_POINTER(x)		((void *) (long) (int32_t) (x))

#define RB_POINTER_TO_UINT(x)		((uint32_t) (unsigned long) (x))
#define RB_UINT_TO_POINTER(x)		((void *) (unsigned long) (uint32_t) (x))

#else

#define RB_POINTER_TO_INT(x)		((int32_t) (unsigned long long) (x))
#define RB_INT_TO_POINTER(x)		((void *) (unsigned long long) (int32_t) (x))

#define RB_POINTER_TO_UINT(x)		((uint32_t) (unsigned long long) (x))
#define RB_UINT_TO_POINTER(x)		((void *) (unsigned long long) (uint32_t) (x))

#endif

static inline int rb_int32cmp(const void *a, const void *b)
{
	return RB_POINTER_TO_INT(b) - RB_POINTER_TO_INT(a);
}

static inline int rb_uint32cmp(const void *a, const void *b)
{
	return RB_POINTER_TO_UINT(b) - RB_POINTER_TO_UINT(a);
}

#ifdef __cplusplus
} // extern "C"
#endif
#endif
