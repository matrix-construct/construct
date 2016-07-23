/*
 *  ircd-ratbox: A slightly useful ircd.
 *  parse.h: A header for the message parser.
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
 */

#ifndef INCLUDED_parse_h_h
#define INCLUDED_parse_h_h

#include <rb/dictionary.h>

struct Message;
struct Client;
struct MsgBuf;

extern void parse(struct Client *, char *, char *);
extern void handle_encap(struct MsgBuf *, struct Client *, struct Client *,
		         const char *, int, const char *parv[]);
extern void clear_hash_parse(void);
extern void mod_add_cmd(struct Message *msg);
extern void mod_del_cmd(struct Message *msg);
extern char *reconstruct_parv(int parc, const char *parv[]);

extern rb_dictionary *alias_dict;
extern rb_dictionary *cmd_dict;

#endif /* INCLUDED_parse_h_h */
