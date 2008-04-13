/*
 *  ircd-ratbox: A slightly useful ircd.
 *  hash.h: A header for the ircd hashtable code.
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
 *  $Id: hash.h 3177 2007-02-01 00:19:14Z jilles $
 */

#ifndef INCLUDED_hash_h
#define INCLUDED_hash_h

struct Dictionary;

extern rb_dlink_list *clientTable;
extern rb_dlink_list *channelTable;
extern rb_dlink_list *idTable;
extern rb_dlink_list *resvTable;
extern rb_dlink_list *hostTable;
extern rb_dlink_list *helpTable;

extern struct Dictionary *nd_dict;

/* Magic value for FNV hash functions */
#define FNV1_32_INIT 0x811c9dc5UL

/* Client hash table size, used in hash.c/s_debug.c */
#define U_MAX_BITS 17
#define U_MAX 131072 /* 2^17 */

/* Client fd hash table size, used in hash.c */
#define CLI_FD_MAX 4096

/* Channel hash table size, hash.c/s_debug.c */
#define CH_MAX_BITS 16
#define CH_MAX 65536 /* 2^16 */

/* hostname hash table size */
#define HOST_MAX_BITS 17
#define HOST_MAX 131072 /* 2^17 */

/* RESV/XLINE hash table size, used in hash.c */
#define R_MAX_BITS 10
#define R_MAX 1024 /* 2^10 */


#define HASH_WALK(i, max, ptr, table) for (i = 0; i < max; i++) { RB_DLINK_FOREACH(ptr, table[i].head)
#define HASH_WALK_SAFE(i, max, ptr, nptr, table) for (i = 0; i < max; i++) { RB_DLINK_FOREACH_SAFE(ptr, nptr, table[i].head)
#define HASH_WALK_END }

struct Client;
struct Channel;
struct ConfItem;
struct cachefile;
struct nd_entry;

extern u_int32_t fnv_hash_upper(const unsigned char *s, int bits);
extern u_int32_t fnv_hash(const unsigned char *s, int bits);
extern u_int32_t fnv_hash_len(const unsigned char *s, int bits, int len);
extern u_int32_t fnv_hash_upper_len(const unsigned char *s, int bits, int len);

extern void init_hash(void);

extern void add_to_client_hash(const char *name, struct Client *client);
extern void del_from_client_hash(const char *name, struct Client *client);
extern struct Client *find_client(const char *name);
extern struct Client *find_named_client(const char *name);
extern struct Client *find_server(struct Client *source_p, const char *name);

extern void add_to_id_hash(const char *, struct Client *);
extern void del_from_id_hash(const char *name, struct Client *client);
extern struct Client *find_id(const char *name);

extern struct Channel *get_or_create_channel(struct Client *client_p, const char *chname, int *isnew);
extern void del_from_channel_hash(const char *name, struct Channel *chan);
extern struct Channel *find_channel(const char *name);

extern void add_to_hostname_hash(const char *, struct Client *);
extern void del_from_hostname_hash(const char *, struct Client *);
extern rb_dlink_node *find_hostname(const char *);

extern void add_to_resv_hash(const char *name, struct ConfItem *aconf);
extern void del_from_resv_hash(const char *name, struct ConfItem *aconf);
extern struct ConfItem *hash_find_resv(const char *name);
extern void clear_resv_hash(void);

void add_to_cli_fd_hash(struct Client *client_p);
void del_from_cli_fd_hash(struct Client *client_p);
struct Client *find_cli_fd_hash(int fd);

extern void hash_stats(struct Client *);

#endif /* INCLUDED_hash_h */
