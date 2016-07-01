/*
 * charybdis: an advanced ircd.
 * privilege.h: Dynamic privileges API.
 *
 * Copyright (c) 2008 William Pitcock <nenolod@dereferenced.org>
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

#ifndef __CHARYBDIS_PRIVILEGE_H
#define __CHARYBDIS_PRIVILEGE_H

#include "stdinc.h"

enum {
	PRIV_NEEDOPER = 1
};
typedef unsigned int PrivilegeFlags;

struct PrivilegeSet {
	unsigned int status;	/* If CONF_ILLEGAL, delete when no refs */
	int refs;
	char *name;
	char *privs;
	PrivilegeFlags flags;
	rb_dlink_node node;
};

int privilegeset_in_set(struct PrivilegeSet *set, const char *priv);
struct PrivilegeSet *privilegeset_set_new(const char *name, const char *privs, PrivilegeFlags flags);
struct PrivilegeSet *privilegeset_extend(struct PrivilegeSet *parent, const char *name, const char *privs, PrivilegeFlags flags);
struct PrivilegeSet *privilegeset_get(const char *name);
struct PrivilegeSet *privilegeset_ref(struct PrivilegeSet *set);
void privilegeset_unref(struct PrivilegeSet *set);
void privilegeset_mark_all_illegal(void);
void privilegeset_delete_all_illegal(void);
void privilegeset_report(struct Client *source_p);

#endif
