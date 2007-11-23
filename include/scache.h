/*
 *  ircd-ratbox: A slightly useful ircd.
 *  scache.h: A header for the servername cache functions.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id: scache.h 6 2005-09-10 01:02:21Z nenolod $
 */

#ifndef INCLUDED_scache_h
#define INCLUDED_scache_h

struct Client;
struct scache_entry;

extern void clear_scache_hash_table(void);
extern struct scache_entry *scache_connect(const char *name, const char *info, int hidden);
extern void scache_split(struct scache_entry *ptr);
extern const char *scache_get_name(struct scache_entry *ptr);
extern void scache_send_flattened_links(struct Client *source_p);
extern void scache_send_missing(struct Client *source_p);
extern void count_scache(size_t *, size_t *);

#endif
