/*
 * charybdis: an advanced ircd.
 * rb_radixtree.c: Dictionary-based information storage.
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

#include <librb_config.h>
#include <rb_lib.h>
#include <rb_radixtree.h>

rb_dlink_list radixtree_list = {NULL, NULL, 0};

/*
 * Patricia tree.
 *
 * A radix trie that avoids one-way branching and redundant nodes.
 *
 * To find a node, the tree is traversed starting from the root. The
 * nibnum in each node indicates which nibble of the key needs to be
 * tested, and the appropriate branch is taken.
 *
 * The nibnum values are strictly increasing while going down the tree.
 *
 * -- jilles
 */

union rb_radixtree_elem;

struct rb_radixtree
{
	void (*canonize_cb)(char *key);
	union rb_radixtree_elem *root;

	unsigned int count;
	char *id;

	rb_dlink_node node;
};

#define POINTERS_PER_NODE 16
#define NIBBLE_VAL(key, nibnum) (((key)[(nibnum) / 2] >> ((nibnum) & 1 ? 0 : 4)) & 0xF)

struct rb_radixtree_node
{
	/* nibble to test (nibble NUM%2 of byte NUM/2) */
	int nibnum;

	/* branches of the tree */
	union rb_radixtree_elem *down[POINTERS_PER_NODE];
	union rb_radixtree_elem *parent;

	char parent_val;
};

struct rb_radixtree_leaf
{
	/* -1 to indicate this is a leaf, not a node */
	int nibnum;

	/* data associated with the key */
	void *data;

	/* key (canonized copy) */
	char *key;
	union rb_radixtree_elem *parent;

	char parent_val;
};

union rb_radixtree_elem
{
	int nibnum;
	struct rb_radixtree_node node;

	struct rb_radixtree_leaf leaf;
};

#define IS_LEAF(elem) ((elem)->nibnum == -1)

/* Preserve compatibility with the old mowgli_patricia.h */
#define STATE_CUR(state) ((state)->pspare[0])
#define STATE_NEXT(state) ((state)->pspare[1])

/*
 * first_leaf()
 *
 * Find the smallest leaf hanging off a subtree.
 *
 * Inputs:
 *     - element (may be leaf or node) heading subtree
 *
 * Outputs:
 *     - lowest leaf in subtree
 *
 * Side Effects:
 *     - none
 */
static union rb_radixtree_elem *
first_leaf(union rb_radixtree_elem *delem)
{
	int val;

	while (!IS_LEAF(delem))
	{
		for (val = 0; val < POINTERS_PER_NODE; val++)
			if (delem->node.down[val] != NULL)
			{
				delem = delem->node.down[val];
				break;
			}
	}

	return delem;
}

/*
 * rb_radixtree_create_named(const char *name,
 *     void (*canonize_cb)(char *key))
 *
 * Dictionary object factory.
 *
 * Inputs:
 *     - patricia name
 *     - function to use for canonizing keys (for example, use
 *       a function that makes the string upper case to create
 *       a patricia with case-insensitive matching)
 *
 * Outputs:
 *     - on success, a new patricia object.
 *
 * Side Effects:
 *     - if services runs out of memory and cannot allocate the object,
 *       the program will abort.
 */
struct rb_radixtree *
rb_radixtree_create(const char *name, void (*canonize_cb)(char *key))
{
	struct rb_radixtree *dtree = (struct rb_radixtree *) rb_malloc(sizeof(struct rb_radixtree));

	dtree->canonize_cb = canonize_cb;
	dtree->id = rb_strdup(name);
	dtree->root = NULL;

	rb_dlinkAdd(dtree, &dtree->node, &radixtree_list);

	return dtree;
}

/*
 * rb_radixtree_destroy(struct rb_radixtree *dtree,
 *     void (*destroy_cb)(const char *key, void *data, void *privdata),
 *     void *privdata);
 *
 * Recursively destroys all nodes in a patricia tree.
 *
 * Inputs:
 *     - patricia tree object
 *     - optional iteration callback
 *     - optional opaque/private data to pass to callback
 *
 * Outputs:
 *     - nothing
 *
 * Side Effects:
 *     - on success, a dtree and optionally it's children are destroyed.
 *
 * Notes:
 *     - if this is called without a callback, the objects bound to the
 *       DTree will not be destroyed.
 */
void
rb_radixtree_destroy(struct rb_radixtree *dtree, void (*destroy_cb)(const char *key, void *data, void *privdata), void *privdata)
{
	struct rb_radixtree_iteration_state state;
	union rb_radixtree_elem *delem;

	void *entry;

	lrb_assert(dtree != NULL);

	RB_RADIXTREE_FOREACH(entry, &state, dtree)
	{
		delem = STATE_CUR(&state);

		if (destroy_cb != NULL)
			(*destroy_cb)(delem->leaf.key, delem->leaf.data,
				      privdata);

		rb_radixtree_delete(dtree, delem->leaf.key);
	}

	rb_dlinkDelete(&dtree->node, &radixtree_list);
	rb_free(dtree->id);
	rb_free(dtree);
}

/*
 * rb_radixtree_foreach(struct rb_radixtree *dtree,
 *     int (*foreach_cb)(const char *key, void *data, void *privdata),
 *     void *privdata);
 *
 * Iterates over all entries in a DTree.
 *
 * Inputs:
 *     - patricia tree object
 *     - optional iteration callback
 *     - optional opaque/private data to pass to callback
 *
 * Outputs:
 *     - nothing
 *
 * Side Effects:
 *     - on success, a dtree is iterated
 */
void
rb_radixtree_foreach(struct rb_radixtree *dtree, int (*foreach_cb)(const char *key, void *data, void *privdata), void *privdata)
{
	union rb_radixtree_elem *delem, *next;

	int val;

	lrb_assert(dtree != NULL);

	delem = dtree->root;

	if (delem == NULL)
		return;

	/* Only one element in the tree */
	if (IS_LEAF(delem))
	{
		if (foreach_cb != NULL)
			(*foreach_cb)(delem->leaf.key, delem->leaf.data, privdata);

		return;
	}

	val = 0;

	do
	{
		do
			next = delem->node.down[val++];
		while (next == NULL && val < POINTERS_PER_NODE);

		if (next != NULL)
		{
			if (IS_LEAF(next))
			{
				if (foreach_cb != NULL)
					(*foreach_cb)(next->leaf.key, next->leaf.data, privdata);
			}
			else
			{
				delem = next;
				val = 0;
			}
		}

		while (val >= POINTERS_PER_NODE)
		{
			val = delem->node.parent_val;
			delem = delem->node.parent;

			if (delem == NULL)
				break;

			val++;
		}
	} while (delem != NULL);
}

/*
 * rb_radixtree_search(struct rb_radixtree *dtree,
 *     void *(*foreach_cb)(const char *key, void *data, void *privdata),
 *     void *privdata);
 *
 * Searches all entries in a DTree using a custom callback.
 *
 * Inputs:
 *     - patricia tree object
 *     - optional iteration callback
 *     - optional opaque/private data to pass to callback
 *
 * Outputs:
 *     - on success, the requested object
 *     - on failure, NULL.
 *
 * Side Effects:
 *     - a dtree is iterated until the requested conditions are met
 */
void *
rb_radixtree_search(struct rb_radixtree *dtree, void *(*foreach_cb)(const char *key, void *data, void *privdata), void *privdata)
{
	union rb_radixtree_elem *delem, *next;

	int val;
	void *ret = NULL;

	lrb_assert(dtree != NULL);

	delem = dtree->root;

	if (delem == NULL)
		return NULL;

	/* Only one element in the tree */
	if (IS_LEAF(delem))
	{
		if (foreach_cb != NULL)
			return (*foreach_cb)(delem->leaf.key, delem->leaf.data, privdata);

		return NULL;
	}

	val = 0;

	for (;;)
	{
		do
			next = delem->node.down[val++];
		while (next == NULL && val < POINTERS_PER_NODE);

		if (next != NULL)
		{
			if (IS_LEAF(next))
			{
				if (foreach_cb != NULL)
					ret = (*foreach_cb)(next->leaf.key, next->leaf.data, privdata);

				if (ret != NULL)
					break;
			}
			else
			{
				delem = next;
				val = 0;
			}
		}

		while (val >= POINTERS_PER_NODE)
		{
			val = delem->node.parent_val;
			delem = delem->node.parent;

			if (delem == NULL)
				break;

			val++;
		}
	}

	return ret;
}

/*
 * rb_radixtree_foreach_start(struct rb_radixtree *dtree,
 *     struct rb_radixtree_iteration_state *state);
 *
 * Initializes a static DTree iterator.
 *
 * Inputs:
 *     - patricia tree object
 *     - static DTree iterator
 *
 * Outputs:
 *     - nothing
 *
 * Side Effects:
 *     - the static iterator, &state, is initialized.
 */
void
rb_radixtree_foreach_start(struct rb_radixtree *dtree, struct rb_radixtree_iteration_state *state)
{
	if (dtree == NULL)
		return;

	lrb_assert(state != NULL);

	if (dtree->root != NULL)
		STATE_NEXT(state) = first_leaf(dtree->root);
	else
		STATE_NEXT(state) = NULL;

	STATE_CUR(state) = STATE_NEXT(state);

	if (STATE_NEXT(state) == NULL)
		return;

	/* make STATE_CUR point to first item and STATE_NEXT point to
	 * second item */
	rb_radixtree_foreach_next(dtree, state);
}

/*
 * rb_radixtree_foreach_cur(struct rb_radixtree *dtree,
 *     struct rb_radixtree_iteration_state *state);
 *
 * Returns the data from the current node being iterated by the
 * static iterator.
 *
 * Inputs:
 *     - patricia tree object
 *     - static DTree iterator
 *
 * Outputs:
 *     - reference to data in the current dtree node being iterated
 *
 * Side Effects:
 *     - none
 */
void *
rb_radixtree_foreach_cur(struct rb_radixtree *dtree, struct rb_radixtree_iteration_state *state)
{
	if (dtree == NULL)
		return NULL;

	lrb_assert(state != NULL);

	return STATE_CUR(state) != NULL ?
	       ((struct rb_radixtree_leaf *) STATE_CUR(state))->data : NULL;
}

/*
 * rb_radixtree_foreach_next(struct rb_radixtree *dtree,
 *     struct rb_radixtree_iteration_state *state);
 *
 * Advances a static DTree iterator.
 *
 * Inputs:
 *     - patricia tree object
 *     - static DTree iterator
 *
 * Outputs:
 *     - nothing
 *
 * Side Effects:
 *     - the static iterator, &state, is advanced to a new DTree node.
 */
void
rb_radixtree_foreach_next(struct rb_radixtree *dtree, struct rb_radixtree_iteration_state *state)
{
	struct rb_radixtree_leaf *leaf;

	union rb_radixtree_elem *delem, *next;

	int val;

	if (dtree == NULL)
		return;

	lrb_assert(state != NULL);

	if (STATE_CUR(state) == NULL)
		return;

	STATE_CUR(state) = STATE_NEXT(state);

	if (STATE_NEXT(state) == NULL)
		return;

	leaf = STATE_NEXT(state);
	delem = leaf->parent;
	val = leaf->parent_val;

	while (delem != NULL)
	{
		do
			next = delem->node.down[val++];
		while (next == NULL && val < POINTERS_PER_NODE);

		if (next != NULL)
		{
			if (IS_LEAF(next))
			{
				/* We will find the original leaf first. */
				if (&next->leaf != leaf)
				{
					if (strcmp(next->leaf.key, leaf->key) < 0)
					{
						STATE_NEXT(state) = NULL;
						return;
					}

					STATE_NEXT(state) = next;
					return;
				}
			}
			else
			{
				delem = next;
				val = 0;
			}
		}

		while (val >= POINTERS_PER_NODE)
		{
			val = delem->node.parent_val;
			delem = delem->node.parent;

			if (delem == NULL)
				break;

			val++;
		}
	}

	STATE_NEXT(state) = NULL;
}

/*
 * rb_radixtree_elem_find(struct rb_radixtree *dtree, const char *key)
 *
 * Looks up a DTree node by name.
 *
 * Inputs:
 *     - patricia tree object
 *     - name of node to lookup
 *     - whether to do a direct or fuzzy match
 *
 * Outputs:
 *     - on success, the dtree node requested
 *     - on failure, NULL
 *
 * Side Effects:
 *     - none
 */
struct rb_radixtree_leaf *
rb_radixtree_elem_find(struct rb_radixtree *dict, const char *key, int fuzzy)
{
	char ckey_store[256];

	char *ckey_buf = NULL;
	const char *ckey;
	union rb_radixtree_elem *delem;

	int val, keylen;

	lrb_assert(dict != NULL);
	lrb_assert(key != NULL);

	keylen = strlen(key);

	if (dict->canonize_cb == NULL)
	{
		ckey = key;
	}
	else
	{
		if (keylen >= (int) sizeof(ckey_store))
		{
			ckey_buf = rb_strdup(key);
			dict->canonize_cb(ckey_buf);
			ckey = ckey_buf;
		}
		else
		{
			rb_strlcpy(ckey_store, key, sizeof ckey_store);
			dict->canonize_cb(ckey_store);
			ckey = ckey_store;
		}
	}

	delem = dict->root;

	while (delem != NULL && !IS_LEAF(delem))
	{
		if (delem->nibnum / 2 < keylen)
			val = NIBBLE_VAL(ckey, delem->nibnum);
		else
			val = 0;

		delem = delem->node.down[val];
	}

	/* Now, if the key is in the tree, delem contains it. */
	if ((delem != NULL) && !fuzzy && strcmp(delem->leaf.key, ckey))
		delem = NULL;

	if (ckey_buf != NULL)
		rb_free(ckey_buf);

	return &delem->leaf;
}

/*
 * rb_radixtree_foreach_start_from(struct rb_radixtree *dtree, struct rb_radixtree_iteration_state *state, const char *key)
 *
 * Starts iteration from a specified key, by wrapping rb_radixtree_elem_find().
 *
 * Inputs:
 *     - patricia tree object
 *     - iterator
 *     - key to start from
 *
 * Outputs:
 *     - none
 *
 * Side Effects:
 *     - the iterator's state is initialized at a specific point
 */
void
rb_radixtree_foreach_start_from(struct rb_radixtree *dtree, struct rb_radixtree_iteration_state *state, const char *key)
{
	lrb_assert(dtree != NULL);
	lrb_assert(state != NULL);

	if (key != NULL)
	{
		STATE_CUR(state) = NULL;
		STATE_NEXT(state) = rb_radixtree_elem_find(dtree, key, 1);

		/* make STATE_CUR point to selected item and STATE_NEXT point to
		 * next item in the tree */
		rb_radixtree_foreach_next(dtree, state);
	}
	else
		rb_radixtree_foreach_start(dtree, state);
}

/*
 * rb_radixtree_add(struct rb_radixtree *dtree, const char *key, void *data)
 *
 * Creates a new DTree node and binds data to it.
 *
 * Inputs:
 *     - patricia tree object
 *     - name for new DTree node
 *     - data to bind to the new DTree node
 *
 * Outputs:
 *     - on success, TRUE
 *     - on failure, FALSE
 *
 * Side Effects:
 *     - data is inserted into the DTree.
 */
struct rb_radixtree_leaf *
rb_radixtree_elem_add(struct rb_radixtree *dict, const char *key, void *data)
{
	char *ckey;

	union rb_radixtree_elem *delem, *prev, *newnode;

	union rb_radixtree_elem **place1;

	int val, keylen;
	int i, j;

	lrb_assert(dict != NULL);
	lrb_assert(key != NULL);
	lrb_assert(data != NULL);

	keylen = strlen(key);
	ckey = rb_strdup(key);

	if (ckey == NULL)
		return NULL;

	if (dict->canonize_cb != NULL)
		dict->canonize_cb(ckey);

	prev = NULL;
	val = POINTERS_PER_NODE + 2;	/* trap value */
	delem = dict->root;

	while (delem != NULL && !IS_LEAF(delem))
	{
		prev = delem;

		if (delem->nibnum / 2 < keylen)
			val = NIBBLE_VAL(ckey, delem->nibnum);
		else
			val = 0;

		delem = delem->node.down[val];
	}

	/* Now, if the key is in the tree, delem contains it. */
	if ((delem != NULL) && !strcmp(delem->leaf.key, ckey))
	{
		rb_free(ckey);
		return NULL;
	}

	if ((delem == NULL) && (prev != NULL))
		/* Get a leaf to compare with. */
		delem = first_leaf(prev);

	if (delem == NULL)
	{
		lrb_assert(prev == NULL);
		lrb_assert(dict->count == 0);
		place1 = &dict->root;
		*place1 = rb_malloc(sizeof(struct rb_radixtree_leaf));
		lrb_assert(*place1 != NULL);
		(*place1)->nibnum = -1;
		(*place1)->leaf.data = data;
		(*place1)->leaf.key = ckey;
		(*place1)->leaf.parent = prev;
		(*place1)->leaf.parent_val = val;
		dict->count++;
		return &(*place1)->leaf;
	}

	/* Find the first nibble where they differ. */
	for (i = 0; NIBBLE_VAL(ckey, i) == NIBBLE_VAL(delem->leaf.key, i); i++)
		;

	/* Find where to insert the new node. */
	while (prev != NULL && prev->nibnum > i)
	{
		val = prev->node.parent_val;
		prev = prev->node.parent;
	}

	if ((prev == NULL) || (prev->nibnum < i))
	{
		/* Insert new node below prev */
		newnode = rb_malloc(sizeof(struct rb_radixtree_node));
		lrb_assert(newnode != NULL);
		newnode->nibnum = i;
		newnode->node.parent = prev;
		newnode->node.parent_val = val;

		for (j = 0; j < POINTERS_PER_NODE; j++)
			newnode->node.down[j] = NULL;

		if (prev == NULL)
		{
			newnode->node.down[NIBBLE_VAL(delem->leaf.key, i)] = dict->root;

			if (IS_LEAF(dict->root))
			{
				dict->root->leaf.parent = newnode;
				dict->root->leaf.parent_val = NIBBLE_VAL(delem->leaf.key, i);
			}
			else
			{
				lrb_assert(dict->root->nibnum > i);
				dict->root->node.parent = newnode;
				dict->root->node.parent_val = NIBBLE_VAL(delem->leaf.key, i);
			}

			dict->root = newnode;
		}
		else
		{
			newnode->node.down[NIBBLE_VAL(delem->leaf.key, i)] = prev->node.down[val];

			if (IS_LEAF(prev->node.down[val]))
			{
				prev->node.down[val]->leaf.parent = newnode;
				prev->node.down[val]->leaf.parent_val = NIBBLE_VAL(delem->leaf.key, i);
			}
			else
			{
				prev->node.down[val]->node.parent = newnode;
				prev->node.down[val]->node.parent_val = NIBBLE_VAL(delem->leaf.key, i);
			}

			prev->node.down[val] = newnode;
		}
	}
	else
	{
		/* This nibble is already checked. */
		lrb_assert(prev->nibnum == i);
		newnode = prev;
	}

	val = NIBBLE_VAL(ckey, i);
	place1 = &newnode->node.down[val];
	lrb_assert(*place1 == NULL);
	*place1 = rb_malloc(sizeof(struct rb_radixtree_leaf));
	lrb_assert(*place1 != NULL);
	(*place1)->nibnum = -1;
	(*place1)->leaf.data = data;
	(*place1)->leaf.key = ckey;
	(*place1)->leaf.parent = newnode;
	(*place1)->leaf.parent_val = val;
	dict->count++;
	return &(*place1)->leaf;
}

int
rb_radixtree_add(struct rb_radixtree *dict, const char *key, void *data)
{
	return (rb_radixtree_elem_add(dict, key, data) != NULL);
}

/*
 * rb_radixtree_delete(struct rb_radixtree *dtree, const char *key)
 *
 * Deletes data from a patricia tree.
 *
 * Inputs:
 *     - patricia tree object
 *     - name of DTree node to delete
 *
 * Outputs:
 *     - on success, the remaining data that needs to be rb_freed
 *     - on failure, NULL
 *
 * Side Effects:
 *     - data is removed from the DTree.
 *
 * Notes:
 *     - the returned data needs to be rb_freed/released manually!
 */
void *
rb_radixtree_delete(struct rb_radixtree *dict, const char *key)
{
	void *data;
	struct rb_radixtree_leaf *leaf;

	leaf = rb_radixtree_elem_find(dict, key, 0);

	if (leaf == NULL)
		return NULL;

	data = leaf->data;
	rb_radixtree_elem_delete(dict, leaf);
	return data;
}

void
rb_radixtree_elem_delete(struct rb_radixtree *dict, struct rb_radixtree_leaf *leaf)
{
	union rb_radixtree_elem *delem, *prev, *next;

	int val, i, used;

	lrb_assert(dict != NULL);
	lrb_assert(leaf != NULL);

	delem = (union rb_radixtree_elem *) leaf;

	val = delem->leaf.parent_val;
	prev = delem->leaf.parent;

	rb_free(delem->leaf.key);
	rb_free(delem);

	if (prev != NULL)
	{
		prev->node.down[val] = NULL;

		/* Leaf is gone, now consider the node it was in. */
		delem = prev;

		used = -1;

		for (i = 0; i < POINTERS_PER_NODE; i++)
			if (delem->node.down[i] != NULL)
				used = used == -1 ? i : -2;

		lrb_assert(used == -2 || used >= 0);

		if (used >= 0)
		{
			/* Only one pointer in this node, remove it.
			 * Replace the pointer that pointed to it by
			 * the sole pointer in it.
			 */
			next = delem->node.down[used];
			val = delem->node.parent_val;
			prev = delem->node.parent;

			if (prev != NULL)
				prev->node.down[val] = next;
			else
				dict->root = next;

			if (IS_LEAF(next))
				next->leaf.parent = prev, next->leaf.parent_val = val;
			else
				next->node.parent = prev, next->node.parent_val = val;

			rb_free(delem);
		}
	}
	else
	{
		/* This was the last leaf. */
		dict->root = NULL;
	}

	dict->count--;

	if (dict->count == 0)
	{
		lrb_assert(dict->root == NULL);
		dict->root = NULL;
	}
}

/*
 * rb_radixtree_retrieve(struct rb_radixtree *dtree, const char *key)
 *
 * Retrieves data from a patricia.
 *
 * Inputs:
 *     - patricia tree object
 *     - name of node to lookup
 *
 * Outputs:
 *     - on success, the data bound to the DTree node.
 *     - on failure, NULL
 *
 * Side Effects:
 *     - none
 */
void *
rb_radixtree_retrieve(struct rb_radixtree *dtree, const char *key)
{
	struct rb_radixtree_leaf *delem = rb_radixtree_elem_find(dtree, key, 0);

	if (delem != NULL)
		return delem->data;

	return NULL;
}

const char *
rb_radixtree_elem_get_key(struct rb_radixtree_leaf *leaf)
{
	lrb_assert(leaf != NULL);

	return leaf->key;
}

void
rb_radixtree_elem_set_data(struct rb_radixtree_leaf *leaf, void *data)
{
	lrb_assert(leaf != NULL);

	leaf->data = data;
}

void *
rb_radixtree_elem_get_data(struct rb_radixtree_leaf *leaf)
{
	lrb_assert(leaf != NULL);

	return leaf->data;
}

/*
 * rb_radixtree_size(struct rb_radixtree *dict)
 *
 * Returns the size of a patricia.
 *
 * Inputs:
 *     - patricia tree object
 *
 * Outputs:
 *     - size of patricia
 *
 * Side Effects:
 *     - none
 */
unsigned int
rb_radixtree_size(struct rb_radixtree *dict)
{
	lrb_assert(dict != NULL);

	return dict->count;
}

/* returns the sum of the depths of the subtree rooted in delem at depth depth */
/* there is no need for this to be recursive, but it is easier... */
static int
stats_recurse(union rb_radixtree_elem *delem, int depth, int *pmaxdepth)
{
	int result = 0;
	int val;
	union rb_radixtree_elem *next;

	if (depth > *pmaxdepth)
		*pmaxdepth = depth;

	if (depth == 0)
	{
		if (IS_LEAF(delem))
			lrb_assert(delem->leaf.parent == NULL);

		else
			lrb_assert(delem->node.parent == NULL);
	}

	if (IS_LEAF(delem))
		return depth;

	for (val = 0; val < POINTERS_PER_NODE; val++)
	{
		next = delem->node.down[val];

		if (next == NULL)
			continue;

		result += stats_recurse(next, depth + 1, pmaxdepth);

		if (IS_LEAF(next))
		{
			lrb_assert(next->leaf.parent == delem);
			lrb_assert(next->leaf.parent_val == val);
		}
		else
		{
			lrb_assert(next->node.parent == delem);
			lrb_assert(next->node.parent_val == val);
			lrb_assert(next->node.nibnum > delem->node.nibnum);
		}
	}

	return result;
}

/*
 * rb_radixtree_stats(struct rb_radixtree *dict, void (*cb)(const char *line, void *privdata), void *privdata)
 *
 * Returns the size of a patricia.
 *
 * Inputs:
 *     - patricia tree object
 *     - callback
 *     - data for callback
 *
 * Outputs:
 *     - none
 *
 * Side Effects:
 *     - callback called with stats text
 */
void
rb_radixtree_stats(struct rb_radixtree *dict, void (*cb)(const char *line, void *privdata), void *privdata)
{
	char str[256];
	int sum, maxdepth;

	lrb_assert(dict != NULL);

	maxdepth = 0;
	if (dict->count > 0)
	{
		sum = stats_recurse(dict->root, 0, &maxdepth);
		snprintf(str, sizeof str, "%-30s %-15s %-10d %-10d %-10d %-10d", dict->id, "RADIX", dict->count, sum, sum / dict->count, maxdepth);
	}
	else
	{
		snprintf(str, sizeof str, "%-30s %-15s %-10s %-10s %-10s %-10s", dict->id, "RADIX", "0", "0", "0", "0");
	}

	cb(str, privdata);
	return;
}

void
rb_radixtree_stats_walk(void (*cb)(const char *line, void *privdata), void *privdata)
{
	rb_dlink_node *ptr;

	RB_DLINK_FOREACH(ptr, radixtree_list.head)
	{
		rb_radixtree_stats(ptr->data, cb, privdata);
	}
}

