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
 *  $Id: rb_tools.h 26170 2008-10-26 20:59:07Z androsyn $
 */

#ifndef RB_LIB_H
# error "Do not use tools.h directly"
#endif

#ifndef __TOOLS_H__
#define __TOOLS_H__

size_t rb_strlcpy(char *dst, const char *src, size_t siz);
size_t rb_strlcat(char *dst, const char *src, size_t siz);
size_t rb_strnlen(const char *s, size_t count);
char *rb_basename(const char *);
char *rb_dirname(const char *);

int rb_string_to_array(char *string, char **parv, int maxpara);

/*
 * double-linked-list stuff
 */
typedef struct _rb_dlink_node rb_dlink_node;
typedef struct _rb_dlink_list rb_dlink_list;

struct _rb_dlink_node
{
	void *data;
	rb_dlink_node *prev;
	rb_dlink_node *next;

};

struct _rb_dlink_list
{
	rb_dlink_node *head;
	rb_dlink_node *tail;
	unsigned long length;
};

rb_dlink_node *rb_make_rb_dlink_node(void);
void rb_free_rb_dlink_node(rb_dlink_node *lp);
void rb_init_rb_dlink_nodes(size_t dh_size);

/* This macros are basically swiped from the linux kernel
 * they are simple yet effective
 */

/*
 * Walks forward of a list.  
 * pos is your node
 * head is your list head
 */
#define RB_DLINK_FOREACH(pos, head) for (pos = (head); pos != NULL; pos = pos->next)

/*
 * Walks forward of a list safely while removing nodes 
 * pos is your node
 * n is another list head for temporary storage
 * head is your list head
 */
#define RB_DLINK_FOREACH_SAFE(pos, n, head) for (pos = (head), n = pos ? pos->next : NULL; pos != NULL; pos = n, n = pos ? pos->next : NULL)

#define RB_DLINK_FOREACH_PREV(pos, head) for (pos = (head); pos != NULL; pos = pos->prev)


/* Returns the list length */
#define rb_dlink_list_length(list) (list)->length

#define rb_dlinkAddAlloc(data, list) rb_dlinkAdd(data, rb_make_rb_dlink_node(), list)
#define rb_dlinkAddTailAlloc(data, list) rb_dlinkAddTail(data, rb_make_rb_dlink_node(), list)
#define rb_dlinkDestroy(node, list) do { rb_dlinkDelete(node, list); rb_free_rb_dlink_node(node); } while(0)


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
		rb_free_rb_dlink_node(ptr);
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

#endif /* __TOOLS_H__ */
