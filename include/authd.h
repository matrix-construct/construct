/*
 *  charybdis
 *  authd.h: A header with the authd functions.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2012 ircd-ratbox development team
 *  Copyright (C) 2016 William Pitcock <nenolod@dereferenced.org>
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
 */

#ifndef CHARYBDIS_AUTHD_H
#define CHARYBDIS_AUTHD_H

#include "stdinc.h"
#include "rb_dictionary.h"
#include "client.h"

struct blacklist_stats
{
	uint8_t iptype;
	unsigned int hits;
};

extern rb_helper *authd_helper;

extern rb_dictionary *bl_stats;

void init_authd(void);
void configure_authd(void);
void restart_authd(void);
void rehash_authd(void);
void check_authd(void);

void authd_initiate_client(struct Client *);
void authd_decide_client(struct Client *client_p, const char *ident, const char *host, bool accept, char cause, const char *data, const char *reason);
void authd_abort_client(struct Client *);
const char *get_provider_string(char cause);

void add_blacklist(const char *host, const char *reason, uint8_t iptype, rb_dlink_list *filters);
void del_blacklist(const char *host);
void del_blacklist_all(void);
void set_authd_timeout(const char *key, int timeout);

#endif
