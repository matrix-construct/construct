/*
 *  sslproc.h: An interface to the ratbox ssld helper daemon
 *  Copyright (C) 2007 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2007 ircd-ratbox development team
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

#ifndef INCLUDED_sslproc_h
#define INCLUDED_sslproc_h

struct _ssl_ctl;
typedef struct _ssl_ctl ssl_ctl_t;

enum ssld_status {
	SSLD_ACTIVE,
	SSLD_SHUTDOWN,
	SSLD_DEAD,
};

void init_ssld(void);
void restart_ssld(void);
int start_ssldaemon(int count);
ssl_ctl_t *start_ssld_accept(rb_fde_t *sslF, rb_fde_t *plainF, uint32_t id);
ssl_ctl_t *start_ssld_connect(rb_fde_t *sslF, rb_fde_t *plainF, uint32_t id);
void start_zlib_session(void *data);
void ssld_update_config(void);
void ssld_decrement_clicount(ssl_ctl_t *ctl);
int get_ssld_count(void);
void ssld_foreach_info(void (*func)(void *data, pid_t pid, int cli_count, enum ssld_status status, const char *version), void *data);

#endif

