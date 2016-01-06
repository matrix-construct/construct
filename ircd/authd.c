/*
 *  authd.c: An interface to authd.
 *  (based somewhat on ircd-ratbox dns.c)
 *
 *  Copyright (C) 2005 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2005-2012 ircd-ratbox development team
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

#include <stdinc.h>
#include <ratbox_lib.h>
#include <struct.h>
#include <ircd_defs.h>
#include <parse.h>
#include <authd.h>
#include <match.h>
#include <logger.h>
#include <s_conf.h>
#include <client.h>
#include <send.h>
#include <numeric.h>
#include <msg.h>
#include <dns.h>

static int start_authd(void);
static void parse_authd_reply(rb_helper * helper);
static void restart_authd_cb(rb_helper * helper);

rb_helper *authd_helper;
static char *authd_path;

static int
start_authd(void)
{
	char fullpath[PATH_MAX + 1];
#ifdef _WIN32
	const char *suffix = ".exe";
#else
	const char *suffix = "";
#endif
	if(authd_path == NULL)
	{
		snprintf(fullpath, sizeof(fullpath), "%s/authd%s", PKGLIBEXECDIR, suffix);

		if(access(fullpath, X_OK) == -1)
		{
			snprintf(fullpath, sizeof(fullpath), "%s/libexec/charybdis/authd%s",
				 ConfigFileEntry.dpath, suffix);
			if(access(fullpath, X_OK) == -1)
			{
				ilog(L_MAIN,
				     "Unable to execute authd in %s or %s/libexec/charybdis",
				     PKGLIBEXECDIR, ConfigFileEntry.dpath);
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						       "Unable to execute authd in %s or %s/libexec/charybdis",
						       PKGLIBEXECDIR, ConfigFileEntry.dpath);
				return 1;
			}

		}

		authd_path = rb_strdup(fullpath);
	}

	authd_helper = rb_helper_start("authd", authd_path, parse_authd_reply, restart_authd_cb);

	if(authd_helper == NULL)
	{
		ilog(L_MAIN, "Unable to start authd helper: %s", strerror(errno));
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "Unable to start authd helper: %s", strerror(errno));
		return 1;
	}
	ilog(L_MAIN, "authd helper started");
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "authd helper started");
	rb_helper_run(authd_helper);
	return 0;
}

static void
parse_authd_reply(rb_helper * helper)
{
	ssize_t len;
	int parc;
	char dnsBuf[READBUF_SIZE];

	char *parv[MAXPARA + 1];
	while((len = rb_helper_read(helper, dnsBuf, sizeof(dnsBuf))) > 0)
	{
		parc = rb_string_to_array(dnsBuf, parv, MAXPARA+1); 

		switch (*parv[0])
		{
		case 'E':
			if(parc != 5)
			{
				ilog(L_MAIN, "authd sent a result with wrong number of arguments: got %d", parc);
				restart_authd();
				return;
			}
			dns_results_callback(parv[1], parv[2], parv[3], parv[4]);
			break;
		default:
			break;
		}
	}
}

void
init_authd(void)
{
	if(start_authd())
	{
		ilog(L_MAIN, "Unable to start authd helper: %s", strerror(errno));
		exit(0);
	}
}

static void
restart_authd_cb(rb_helper * helper)
{
	ilog(L_MAIN, "authd: restart_authd_cb called, authd died?");
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "authd: restart_authd_cb called, authd died?");
	if(helper != NULL)
	{
		rb_helper_close(helper);
		authd_helper = NULL;
	}
	start_authd();
}

void
restart_authd(void)
{
	restart_authd_cb(authd_helper);
}

void
rehash_authd(void)
{
	rb_helper_write(authd_helper, "R");
}

void
check_authd(void)
{
	if(authd_helper == NULL)
		restart_authd();
}
