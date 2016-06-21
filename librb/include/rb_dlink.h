/*
 *  ircd-ratbox: A slightly useful ircd.
 *  tools.h: Header for the various tool functions.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#ifndef RB_LIB_H
	#error "Do not use rb_dlink.h directly"
#endif

#ifndef __DLINK_H__
#define __DLINK_H__


typedef struct rb_dlink_node
{
	void *data;
	struct rb_dlink_node *prev;
	struct rb_dlink_node *next;
}
rb_dlink_node;

typedef struct rb_dlink_list
{
	rb_dlink_node *head;
	rb_dlink_node *tail;
	unsigned long length;
}
rb_dlink_list;


// Utils
#define rb_dlink_list_length(list) (list)->length

// XXX I'd like to remove this interface, it's not safe.
rb_dlink_node *rb_make_rb_dlink_node(void);
void rb_free_rb_dlink_node(rb_dlink_node *lp);
void rb_init_rb_dlink_nodes(size_t dh_size);
#define rb_dlinkAddAlloc(data, list) rb_dlinkAdd(data, rb_make_rb_dlink_node(), list)
#define rb_dlinkAddTailAlloc(data, list) rb_dlinkAddTail(data, rb_make_rb_dlink_node(), list)
#define rb_dlinkDestroy(node, list) do { rb_dlinkDelete(node, list); rb_free_rb_dlink_node(node); } while(0)

// Vintage
static void rb_dlinkAdd(void *data, rb_dlink_node *m, rb_dlink_list *list);
static void rb_dlinkAddBefore(rb_dlink_node *b, void *data, rb_dlink_node *m, rb_dlink_list *list);
static void rb_dlinkAddTail(void *data, rb_dlink_node *m, rb_dlink_list *list);
static rb_dlink_node *rb_dlinkFindDelete(void *data, rb_dlink_list *list);
static int rb_dlinkFindDestroy(void *data, rb_dlink_list *list);
static rb_dlink_node *rb_dlinkFind(void *data, rb_dlink_list *list);
static void rb_dlinkMoveNode(rb_dlink_node *m, rb_dlink_list *oldlist, rb_dlink_list *newlist);
static void rb_dlinkMoveTail(rb_dlink_node *m, rb_dlink_list *list);
static void rb_dlinkDelete(rb_dlink_node *m, rb_dlink_list *list);
static void rb_dlinkMoveList(rb_dlink_list *from, rb_dlink_list *to);

/* This macros are basically swiped from the linux kernel
 * they are simple yet effective
 */
// Usage: rb_dlink_node *n; RB_DLINK_FOREACH(n, list.head) { ... }
#define RB_DLINK_FOREACH(node, head)         for(node = (head); node != NULL; node = node->next)
#define RB_DLINK_FOREACH_PREV(node, head)    for(node = (head); node != NULL; node = node->prev)

// Allows for modifying the list during iteration.
// Usage: rb_dlink_node *cur, *nxt; RB_DLINK_FOREACH_SAFE(cur, nxt, list.head) { ... }
#define RB_DLINK_FOREACH_SAFE(cur, nxt, head) \
	for(cur = (head), nxt = cur? cur->next : NULL; cur != NULL; cur = nxt, nxt = cur? cur->next : NULL)

#define RB_DLINK_FOREACH_PREV_SAFE(cur, pre, head) \
	for(cur = (head), pre = cur? cur->prev : NULL; cur != NULL; cur = pre, pre = cur? cur->prev : NULL)


/*
 * dlink_ routines are stolen from squid, except for rb_dlinkAddBefore,
 * which is mine.
 *   -- adrian
 */

static inline void
rb_dlinkMoveNode(rb_dlink_node *m, rb_dlink_list *oldlist, rb_dlink_list *newlist)
{
	/* Assumption: If m->next == NULL, then list->tail == m
	 *      and:   If m->prev == NULL, then list->head == m
	 */
	assert(m != NULL);
	assert(oldlist != NULL);
	assert(newlist != NULL);

	if(m->next)
		m->next->prev = m->prev;
	else
		oldlist->tail = m->prev;

	if(m->prev)
		m->prev->next = m->next;
	else
		oldlist->head = m->next;

	m->prev = NULL;
	m->next = newlist->head;
	if(newlist->head != NULL)
		newlist->head->prev = m;
	else if(newlist->tail == NULL)
		newlist->tail = m;
	newlist->head = m;

	oldlist->length--;
	newlist->length++;
}

static inline void
rb_dlinkAdd(void *data, rb_dlink_node *m, rb_dlink_list *list)
{
	assert(data != NULL);
	assert(m != NULL);
	assert(list != NULL);

	m->data = data;
	m->prev = NULL;
	m->next = list->head;

	/* Assumption: If list->tail != NULL, list->head != NULL */
	if(list->head != NULL)
		list->head->prev = m;
	else if(list->tail == NULL)
		list->tail = m;

	list->head = m;
	list->length++;
}

static inline void
rb_dlinkAddBefore(rb_dlink_node *b, void *data, rb_dlink_node *m, rb_dlink_list *list)
{
	assert(b != NULL);
	assert(data != NULL);
	assert(m != NULL);
	assert(list != NULL);

	/* Shortcut - if its the first one, call rb_dlinkAdd only */
	if(b == list->head)
	{
		rb_dlinkAdd(data, m, list);
	}
	else
	{
		m->data = data;
		b->prev->next = m;
		m->prev = b->prev;
		b->prev = m;
		m->next = b;
		list->length++;
	}
}

static inline void
rb_dlinkMoveTail(rb_dlink_node *m, rb_dlink_list *list)
{
	if(list->tail == m)
		return;

	/* From here assume that m->next != NULL as that can only
	 * be at the tail and assume that the node is on the list
	 */

	m->next->prev = m->prev;

	if(m->prev != NULL)
		m->prev->next = m->next;
	else
		list->head = m->next;

	list->tail->next = m;
	m->prev = list->tail;
	m->next = NULL;
	list->tail = m;
}

static inline void
rb_dlinkAddTail(void *data, rb_dlink_node *m, rb_dlink_list *list)
{
	assert(m != NULL);
	assert(list != NULL);
	assert(data != NULL);

	m->data = data;
	m->next = NULL;
	m->prev = list->tail;

	/* Assumption: If list->tail != NULL, list->head != NULL */
	if(list->tail != NULL)
		list->tail->next = m;
	else if(list->head == NULL)
		list->head = m;

	list->tail = m;
	list->length++;
}

/* Execution profiles show that this function is called the most
 * often of all non-spontaneous functions. So it had better be
 * efficient. */
static inline void
rb_dlinkDelete(rb_dlink_node *m, rb_dlink_list *list)
{
	assert(m != NULL);
	assert(list != NULL);
	/* Assumption: If m->next == NULL, then list->tail == m
	 *      and:   If m->prev == NULL, then list->head == m
	 */
	if(m->next)
		m->next->prev = m->prev;
	else
		list->tail = m->prev;

	if(m->prev)
		m->prev->next = m->next;
	else
		list->head = m->next;

	m->next = m->prev = NULL;
	list->length--;
}

static inline rb_dlink_node *
rb_dlinkFindDelete(void *data, rb_dlink_list *list)
{
	rb_dlink_node *m;
	assert(list != NULL);
	assert(data != NULL);
	RB_DLINK_FOREACH(m, list->head)
	{
		if(m->data != data)
			continue;

		if(m->next)
			m->next->prev = m->prev;
		else
			list->tail = m->prev;

		if(m->prev)
			m->prev->next = m->next;
		else
			list->head = m->next;

		m->next = m->prev = NULL;
		list->length--;
		return m;
	}
	return NULL;
}

static inline int
rb_dlinkFindDestroy(void *data, rb_dlink_list *list)
{
	void *ptr;

	assert(list != NULL);
	assert(data != NULL);
	ptr = rb_dlinkFindDelete(data, list);

	if(ptr != NULL)
	{
		rb_free_rb_dlink_node((rb_dlink_node *)ptr);
		return 1;
	}
	return 0;
}

/*
 * rb_dlinkFind
 * inputs	- list to search
 *		- data
 * output	- pointer to link or NULL if not found
 * side effects	- Look for ptr in the linked listed pointed to by link.
 */
static inline rb_dlink_node *
rb_dlinkFind(void *data, rb_dlink_list *list)
{
	rb_dlink_node *ptr;
	assert(list != NULL);
	assert(data != NULL);

	RB_DLINK_FOREACH(ptr, list->head)
	{
		if(ptr->data == data)
			return (ptr);
	}
	return (NULL);
}

static inline void
rb_dlinkMoveList(rb_dlink_list *from, rb_dlink_list *to)
{
	assert(from != NULL);
	assert(to != NULL);

	/* There are three cases */
	/* case one, nothing in from list */
	if(from->head == NULL)
		return;

	/* case two, nothing in to list */
	if(to->head == NULL)
	{
		to->head = from->head;
		to->tail = from->tail;
		from->head = from->tail = NULL;
		to->length = from->length;
		from->length = 0;
		return;
	}

	/* third case play with the links */
	from->tail->next = to->head;
	to->head->prev = from->tail;
	to->head = from->head;
	from->head = from->tail = NULL;
	to->length += from->length;
	from->length = 0;
}

#endif /* __DLINK_H__ */
