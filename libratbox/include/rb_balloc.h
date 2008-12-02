/*
 *  ircd-ratbox: A slightly useful ircd.
 *  balloc.h: The ircd block allocator header.
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
 *  $Id: rb_balloc.h 26092 2008-09-19 15:13:52Z androsyn $
 */

#ifndef RB_LIB_H
# error "Do not use balloc.h directly"
#endif

#ifndef INCLUDED_balloc_h
#define INCLUDED_balloc_h


struct rb_bh;
typedef struct rb_bh rb_bh;
typedef void rb_bh_usage_cb (size_t bused, size_t bfree, size_t bmemusage, size_t heapalloc,
			     const char *desc, void *data);


int rb_bh_free(rb_bh *, void *);
void *rb_bh_alloc(rb_bh *);

rb_bh *rb_bh_create(size_t elemsize, int elemsperblock, const char *desc);
int rb_bh_destroy(rb_bh *bh);
int rb_bh_gc(rb_bh *bh);
void rb_init_bh(void);
void rb_bh_usage(rb_bh *bh, size_t *bused, size_t *bfree, size_t *bmemusage, const char **desc);
void rb_bh_usage_all(rb_bh_usage_cb *cb, void *data);
void rb_bh_total_usage(size_t *total_alloc, size_t *total_used);

#endif /* INCLUDED_balloc_h */
