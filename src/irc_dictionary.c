/*
 * charybdis: an advanced ircd
 * irc_dictionary.c: Dictionary-based information storage.
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

#include "stdinc.h"
#include "match.h"
#include "client.h"
#include "setup.h"
#include "irc_dictionary.h"

static rb_bh *elem_heap = NULL;

struct Dictionary
{
	DCF compare_cb;
	struct DictionaryElement *root, *head, *tail;
	unsigned int count;
	char *id;
	unsigned int dirty:1;
};

/*
 * irc_dictionary_create(DCF compare_cb)
 *
 * Dictionary object factory.
 *
 * Inputs:
 *     - function to use for comparing two entries in the dtree
 *
 * Outputs:
 *     - on success, a new dictionary object.
 *
 * Side Effects:
 *     - if services runs out of memory and cannot allocate the object,
 *       the program will abort.
 */
struct Dictionary *irc_dictionary_create(DCF compare_cb)
{
	struct Dictionary *dtree = (struct Dictionary *) rb_malloc(sizeof(struct Dictionary));

	dtree->compare_cb = compare_cb;

	if (!elem_heap)
		elem_heap = rb_bh_create(sizeof(struct DictionaryElement), 1024, "dictionary_elem_heap");

	return dtree;
}

/*
 * irc_dictionary_create_named(const char *name, 
 *     DCF compare_cb)
 *
 * Dictionary object factory.
 *
 * Inputs:
 *     - dictionary name
 *     - function to use for comparing two entries in the dtree
 *
 * Outputs:
 *     - on success, a new dictionary object.
 *
 * Side Effects:
 *     - if services runs out of memory and cannot allocate the object,
 *       the program will abort.
 */
struct Dictionary *irc_dictionary_create_named(const char *name,
	DCF compare_cb)
{
	struct Dictionary *dtree = (struct Dictionary *) rb_malloc(sizeof(struct Dictionary));

	dtree->compare_cb = compare_cb;
	dtree->id = rb_strdup(name);

	if (!elem_heap)
		elem_heap = rb_bh_create(sizeof(struct DictionaryElement), 1024, "dictionary_elem_heap");

	return dtree;
}

/*
 * irc_dictionary_set_comparator_func(struct Dictionary *dict,
 *     DCF compare_cb)
 *
 * Resets the comparator function used by the dictionary code for
 * updating the DTree structure.
 *
 * Inputs:
 *     - dictionary object
 *     - new comparator function (passed as functor)
 *
 * Outputs:
 *     - nothing
 *
 * Side Effects:
 *     - the dictionary comparator function is reset.
 */
void irc_dictionary_set_comparator_func(struct Dictionary *dict,
	DCF compare_cb)
{
	s_assert(dict != NULL);
	s_assert(compare_cb != NULL);

	dict->compare_cb = compare_cb;
}

/*
 * irc_dictionary_get_comparator_func(struct Dictionary *dict)
 *
 * Returns the current comparator function used by the dictionary.
 *
 * Inputs:
 *     - dictionary object
 *
 * Outputs:
 *     - comparator function (returned as functor)
 *
 * Side Effects:
 *     - none
 */
DCF
irc_dictionary_get_comparator_func(struct Dictionary *dict)
{
	s_assert(dict != NULL);

	return dict->compare_cb;
}

/*
 * irc_dictionary_get_linear_index(struct Dictionary *dict,
 *     const char *key)
 *
 * Gets a linear index number for key.
 *
 * Inputs:
 *     - dictionary tree object
 *     - pointer to data
 *
 * Outputs:
 *     - position, from zero.
 *
 * Side Effects:
 *     - rebuilds the linear index if the tree is marked as dirty.
 */
int
irc_dictionary_get_linear_index(struct Dictionary *dict, const char *key)
{
	struct DictionaryElement *elem;

	s_assert(dict != NULL);
	s_assert(key != NULL);

	elem = irc_dictionary_find(dict, key);
	if (elem == NULL)
		return -1;

	if (!dict->dirty)
		return elem->position;
	else
	{
		struct DictionaryElement *delem;
		int i;

		for (delem = dict->head, i = 0; delem != NULL; delem = delem->next, i++)
			delem->position = i;

		dict->dirty = FALSE;
	}

	return elem->position;
}

/*
 * irc_dictionary_retune(struct Dictionary *dict, const char *key)
 *
 * Retunes the tree, self-optimizing for the element which belongs to key.
 *
 * Inputs:
 *     - node to begin search from
 *
 * Outputs:
 *     - none
 *
 * Side Effects:
 *     - a new root node is nominated.
 */
static void
irc_dictionary_retune(struct Dictionary *dict, const char *key)
{
	struct DictionaryElement n, *tn, *left, *right, *node;
	int ret;

	s_assert(dict != NULL);

	if (dict->root == NULL)
		return;

	/*
	 * we initialize n with known values, since it's on stack
	 * memory. otherwise the dict would become corrupted.
	 *
 	 * n is used for temporary storage while the tree is retuned.
	 *    -nenolod
	 */
	n.left = n.right = NULL;
	left = right = &n;

	/* this for(;;) loop is the main workhorse of the rebalancing */
	for (node = dict->root; ; )
	{
		if ((ret = dict->compare_cb(key, node->key)) == 0)
			break;

		if (ret < 0)
		{
			if (node->left == NULL)
				break;

			if ((ret = dict->compare_cb(key, node->left->key)) < 0)
			{
				tn = node->left;
				node->left = tn->right;
				tn->right = node;
				node = tn;

				if (node->left == NULL)
					break;
			}

			right->left = node;
			right = node;
			node = node->left;
		}
		else
		{
			if (node->right == NULL)
				break;

			if ((ret = dict->compare_cb(key, node->right->key)) > 0)
			{
				tn = node->right;
				node->right = tn->left;
				tn->left = node;
				node = tn;

				if (node->right == NULL)
					break;
			}

			left->right = node;
			left = node;
			node = node->right;
		}
	}

	left->right = node->left;
	right->left = node->right;

	node->left = n.right;
	node->right = n.left;

	dict->root = node;
}

/*
 * irc_dictionary_link(struct Dictionary *dict,
 *     struct DictionaryElement *delem)
 *
 * Links a dictionary tree element to the dictionary.
 *
 * When we add new nodes to the tree, it becomes the
 * next nominated root. This is perhaps not a wise
 * optimization because of automatic retuning, but
 * it keeps the code simple.
 *
 * Inputs:
 *     - dictionary tree
 *     - dictionary tree element
 *
 * Outputs:
 *     - nothing
 *
 * Side Effects:
 *     - a node is linked to the dictionary tree
 */
static void
irc_dictionary_link(struct Dictionary *dict,
	struct DictionaryElement *delem)
{
	s_assert(dict != NULL);
	s_assert(delem != NULL);

	dict->dirty = TRUE;

	dict->count++;

	if (dict->root == NULL)
	{
		delem->left = delem->right = NULL;
		delem->next = delem->prev = NULL;
		dict->head = dict->tail = dict->root = delem;
	}
	else
	{
		int ret;

		irc_dictionary_retune(dict, delem->key);

		if ((ret = dict->compare_cb(delem->key, dict->root->key)) < 0)
		{
			delem->left = dict->root->left;
			delem->right = dict->root;
			dict->root->left = NULL;

			if (dict->root->prev)
				dict->root->prev->next = delem;
			else
				dict->head = delem;

			delem->prev = dict->root->prev;
			delem->next = dict->root;
			dict->root->prev = delem;
			dict->root = delem;
		}
		else if (ret > 0)
		{
			delem->right = dict->root->right;
			delem->left = dict->root;
			dict->root->right = NULL;

			if (dict->root->next)
				dict->root->next->prev = delem;
			else
				dict->tail = delem;

			delem->next = dict->root->next;
			delem->prev = dict->root;
			dict->root->next = delem;
			dict->root = delem;
		}
		else
		{
			dict->root->key = delem->key;
			dict->root->data = delem->data;
			dict->count--;

			rb_bh_free(elem_heap, delem);
		}
	}
}

/*
 * irc_dictionary_unlink_root(struct Dictionary *dict)
 *
 * Unlinks the root dictionary tree element from the dictionary.
 *
 * Inputs:
 *     - dictionary tree
 *
 * Outputs:
 *     - nothing
 *
 * Side Effects:
 *     - the root node is unlinked from the dictionary tree
 */
static void
irc_dictionary_unlink_root(struct Dictionary *dict)
{
	struct DictionaryElement *delem, *nextnode, *parentofnext;

	dict->dirty = TRUE;

	delem = dict->root;
	if (delem == NULL)
		return;

	if (dict->root->left == NULL)
		dict->root = dict->root->right;
	else if (dict->root->right == NULL)
		dict->root = dict->root->left;
	else
	{
		/* Make the node with the next highest key the new root.
		 * This node has a NULL left pointer. */
		nextnode = delem->next;
		s_assert(nextnode->left == NULL);
		if (nextnode == delem->right)
		{
			dict->root = nextnode;
			dict->root->left = delem->left;
		}
		else
		{
			parentofnext = delem->right;
			while (parentofnext->left != NULL && parentofnext->left != nextnode)
				parentofnext = parentofnext->left;
			s_assert(parentofnext->left == nextnode);
			parentofnext->left = nextnode->right;
			dict->root = nextnode;
			dict->root->left = delem->left;
			dict->root->right = delem->right;
		}
	}

	/* linked list */
	if (delem->prev != NULL)
		delem->prev->next = delem->next;

	if (dict->head == delem)
		dict->head = delem->next;

	if (delem->next)
		delem->next->prev = delem->prev;

	if (dict->tail == delem)
		dict->tail = delem->prev;

	dict->count--;
}

/*
 * irc_dictionary_destroy(struct Dictionary *dtree,
 *     void (*destroy_cb)(dictionary_elem_t *delem, void *privdata),
 *     void *privdata);
 *
 * Recursively destroys all nodes in a dictionary tree.
 *
 * Inputs:
 *     - dictionary tree object
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
void irc_dictionary_destroy(struct Dictionary *dtree,
	void (*destroy_cb)(struct DictionaryElement *delem, void *privdata),
	void *privdata)
{
	struct DictionaryElement *n, *tn;

	s_assert(dtree != NULL);

	RB_DLINK_FOREACH_SAFE(n, tn, dtree->head)
	{
		if (destroy_cb != NULL)
			(*destroy_cb)(n, privdata);

		rb_bh_free(elem_heap, n);
	}

	rb_free(dtree);
}

/*
 * irc_dictionary_foreach(struct Dictionary *dtree,
 *     void (*destroy_cb)(dictionary_elem_t *delem, void *privdata),
 *     void *privdata);
 *
 * Iterates over all entries in a DTree.
 *
 * Inputs:
 *     - dictionary tree object
 *     - optional iteration callback
 *     - optional opaque/private data to pass to callback
 *
 * Outputs:
 *     - nothing
 *
 * Side Effects:
 *     - on success, a dtree is iterated
 */
void irc_dictionary_foreach(struct Dictionary *dtree,
	int (*foreach_cb)(struct DictionaryElement *delem, void *privdata),
	void *privdata)
{
	struct DictionaryElement *n, *tn;

	s_assert(dtree != NULL);

	RB_DLINK_FOREACH_SAFE(n, tn, dtree->head)
	{
		/* delem_t is a subclass of node_t. */
		struct DictionaryElement *delem = (struct DictionaryElement *) n;

		if (foreach_cb != NULL)
			(*foreach_cb)(delem, privdata);
	}
}

/*
 * irc_dictionary_search(struct Dictionary *dtree,
 *     void (*destroy_cb)(struct DictionaryElement *delem, void *privdata),
 *     void *privdata);
 *
 * Searches all entries in a DTree using a custom callback.
 *
 * Inputs:
 *     - dictionary tree object
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
void *irc_dictionary_search(struct Dictionary *dtree,
	void *(*foreach_cb)(struct DictionaryElement *delem, void *privdata),
	void *privdata)
{
	struct DictionaryElement *n, *tn;
	void *ret = NULL;

	s_assert(dtree != NULL);

	RB_DLINK_FOREACH_SAFE(n, tn, dtree->head)
	{
		/* delem_t is a subclass of node_t. */
		struct DictionaryElement *delem = (struct DictionaryElement *) n;

		if (foreach_cb != NULL)
			ret = (*foreach_cb)(delem, privdata);

		if (ret)
			break;
	}

	return ret;
}

/*
 * irc_dictionary_foreach_start(struct Dictionary *dtree,
 *     struct DictionaryIter *state);
 *
 * Initializes a static DTree iterator.
 *
 * Inputs:
 *     - dictionary tree object
 *     - static DTree iterator
 *
 * Outputs:
 *     - nothing
 *
 * Side Effects:
 *     - the static iterator, &state, is initialized.
 */
void irc_dictionary_foreach_start(struct Dictionary *dtree,
	struct DictionaryIter *state)
{
	s_assert(dtree != NULL);
	s_assert(state != NULL);

	state->cur = NULL;
	state->next = NULL;

	/* find first item */
	state->cur = dtree->head;

	if (state->cur == NULL)
		return;

	/* make state->cur point to first item and state->next point to
	 * second item */
	state->next = state->cur;
	irc_dictionary_foreach_next(dtree, state);
}

/*
 * irc_dictionary_foreach_cur(struct Dictionary *dtree,
 *     struct DictionaryIter *state);
 *
 * Returns the data from the current node being iterated by the
 * static iterator.
 *
 * Inputs:
 *     - dictionary tree object
 *     - static DTree iterator
 *
 * Outputs:
 *     - reference to data in the current dtree node being iterated
 *
 * Side Effects:
 *     - none
 */
void *irc_dictionary_foreach_cur(struct Dictionary *dtree,
	struct DictionaryIter *state)
{
	s_assert(dtree != NULL);
	s_assert(state != NULL);

	return state->cur != NULL ? state->cur->data : NULL;
}

/*
 * irc_dictionary_foreach_next(struct Dictionary *dtree,
 *     struct DictionaryIter *state);
 *
 * Advances a static DTree iterator.
 *
 * Inputs:
 *     - dictionary tree object
 *     - static DTree iterator
 *
 * Outputs:
 *     - nothing
 *
 * Side Effects:
 *     - the static iterator, &state, is advanced to a new DTree node.
 */
void irc_dictionary_foreach_next(struct Dictionary *dtree,
	struct DictionaryIter *state)
{
	s_assert(dtree != NULL);
	s_assert(state != NULL);

	if (state->cur == NULL)
	{
		ilog(L_MAIN, "irc_dictionary_foreach_next(): called again after iteration finished on dtree<%p>", (void *)dtree);
		return;
	}

	state->cur = state->next;

	if (state->next == NULL)
		return;

	state->next = state->next->next;
}

/*
 * irc_dictionary_find(struct Dictionary *dtree, const char *key)
 *
 * Looks up a DTree node by name.
 *
 * Inputs:
 *     - dictionary tree object
 *     - name of node to lookup
 *
 * Outputs:
 *     - on success, the dtree node requested
 *     - on failure, NULL
 *
 * Side Effects:
 *     - none
 */
struct DictionaryElement *irc_dictionary_find(struct Dictionary *dict, const char *key)
{
	s_assert(dict != NULL);
	s_assert(key != NULL);

	/* retune for key, key will be the tree's root if it's available */
	irc_dictionary_retune(dict, key);

	if (dict->root && !dict->compare_cb(key, dict->root->key))
		return dict->root;

	return NULL;
}

/*
 * irc_dictionary_add(struct Dictionary *dtree, const char *key, void *data)
 *
 * Creates a new DTree node and binds data to it.
 *
 * Inputs:
 *     - dictionary tree object
 *     - name for new DTree node
 *     - data to bind to the new DTree node
 *
 * Outputs:
 *     - on success, a new DTree node
 *     - on failure, NULL
 *
 * Side Effects:
 *     - data is inserted into the DTree.
 */
struct DictionaryElement *irc_dictionary_add(struct Dictionary *dict, const char *key, void *data)
{
	struct DictionaryElement *delem;

	s_assert(dict != NULL);
	s_assert(key != NULL);
	s_assert(data != NULL);
	s_assert(irc_dictionary_find(dict, key) == NULL);

	delem = rb_bh_alloc(elem_heap);
	delem->key = key;
	delem->data = data;

	/* TBD: is this needed? --nenolod */
	if (delem->key == NULL)
	{
		rb_bh_free(elem_heap, delem);
		return NULL;
	}

	irc_dictionary_link(dict, delem);

	return delem;
}

/*
 * irc_dictionary_delete(struct Dictionary *dtree, const char *key)
 *
 * Deletes data from a dictionary tree.
 *
 * Inputs:
 *     - dictionary tree object
 *     - name of DTree node to delete
 *
 * Outputs:
 *     - on success, the remaining data that needs to be mowgli_freed
 *     - on failure, NULL
 *
 * Side Effects:
 *     - data is removed from the DTree.
 *
 * Notes:
 *     - the returned data needs to be mowgli_freed/released manually!
 */
void *irc_dictionary_delete(struct Dictionary *dtree, const char *key)
{
	struct DictionaryElement *delem = irc_dictionary_find(dtree, key);
	void *data;

	if (delem == NULL)
		return NULL;

	data = delem->data;

	irc_dictionary_unlink_root(dtree);
	rb_bh_free(elem_heap, delem);	

	return data;
}

/*
 * irc_dictionary_retrieve(struct Dictionary *dtree, const char *key)
 *
 * Retrieves data from a dictionary.
 *
 * Inputs:
 *     - dictionary tree object
 *     - name of node to lookup
 *
 * Outputs:
 *     - on success, the data bound to the DTree node.
 *     - on failure, NULL
 *
 * Side Effects:
 *     - none
 */
void *irc_dictionary_retrieve(struct Dictionary *dtree, const char *key)
{
	struct DictionaryElement *delem = irc_dictionary_find(dtree, key);

	if (delem != NULL)
		return delem->data;

	return NULL;
}

/*
 * irc_dictionary_size(struct Dictionary *dict)
 *
 * Returns the size of a dictionary.
 *
 * Inputs:
 *     - dictionary tree object
 *
 * Outputs:
 *     - size of dictionary
 *
 * Side Effects:
 *     - none
 */
unsigned int irc_dictionary_size(struct Dictionary *dict)
{
	s_assert(dict != NULL);

	return dict->count;
}

/* returns the sum of the depths of the subtree rooted in delem at depth depth */
static int
stats_recurse(struct DictionaryElement *delem, int depth, int *pmaxdepth)
{
	int result;

	if (depth > *pmaxdepth)
		*pmaxdepth = depth;
	result = depth;
	if (delem->left)
		result += stats_recurse(delem->left, depth + 1, pmaxdepth);
	if (delem->right)
		result += stats_recurse(delem->right, depth + 1, pmaxdepth);
	return result;
}

/*
 * irc_dictionary_stats(struct Dictionary *dict, void (*cb)(const char *line, void *privdata), void *privdata)
 *
 * Returns the size of a dictionary.
 *
 * Inputs:
 *     - dictionary tree object
 *     - callback
 *     - data for callback
 *
 * Outputs:
 *     - none
 *
 * Side Effects:
 *     - callback called with stats text
 */
void irc_dictionary_stats(struct Dictionary *dict, void (*cb)(const char *line, void *privdata), void *privdata)
{
	char str[256];
	int sum, maxdepth;

	s_assert(dict != NULL);

	if (dict->id != NULL)
		rb_snprintf(str, sizeof str, "Dictionary stats for %s (%d)",
				dict->id, dict->count);
	else
		rb_snprintf(str, sizeof str, "Dictionary stats for <%p> (%d)",
				(void *)dict, dict->count);
	cb(str, privdata);
	maxdepth = 0;
	sum = stats_recurse(dict->root, 0, &maxdepth);
	rb_snprintf(str, sizeof str, "Depth sum %d Avg depth %d Max depth %d", sum, sum / dict->count, maxdepth);
	cb(str, privdata);
	return;
}
