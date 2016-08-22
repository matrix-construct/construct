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

#pragma once
#define HAVE_IRCD_PARSE_H

#ifdef __cplusplus
namespace ircd {

struct Message;
struct MsgBuf;
struct alias_entry;

extern void parse(client::client *, char *, char *);
extern void handle_encap(struct MsgBuf *, client::client *, client::client *,
		         const char *, int, const char *parv[]);
extern void mod_add_cmd(struct Message *msg);
extern void mod_del_cmd(struct Message *msg);
extern char *reconstruct_parv(int parc, const char *parv[]);

extern std::map<std::string, std::shared_ptr<alias_entry>, case_insensitive_less> alias_dict;
extern std::map<std::string, Message *, case_insensitive_less> cmd_dict;

}      // namespace ircd
#endif // __cplusplus
