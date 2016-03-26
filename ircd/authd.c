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
#include <rb_lib.h>
#include <client.h>
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
		snprintf(fullpath, sizeof(fullpath), "%s%cauthd%s", ircd_paths[IRCD_PATH_LIBEXEC], RB_PATH_SEPARATOR, suffix);

		if(access(fullpath, X_OK) == -1)
		{
			snprintf(fullpath, sizeof(fullpath), "%s%cbin%cauthd%s",
				 ConfigFileEntry.dpath, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR, suffix);
			if(access(fullpath, X_OK) == -1)
			{
				ilog(L_MAIN,
				     "Unable to execute authd in %s or %s/bin",
				     ircd_paths[IRCD_PATH_LIBEXEC], ConfigFileEntry.dpath);
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						       "Unable to execute authd in %s or %s/bin",
						       ircd_paths[IRCD_PATH_LIBEXEC], ConfigFileEntry.dpath);
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
		case 'W':
			if(parc != 3)
			{
				ilog(L_MAIN, "authd sent a result with wrong number of arguments: got %d", parc);
				restart_authd();
				return;
			}

			switch(*parv[2])
			{
			case 'D':
				sendto_realops_snomask(SNO_DEBUG, L_ALL, "authd debug: %s", parv[3]);
				break;
			case 'I':
				sendto_realops_snomask(SNO_GENERAL, L_ALL, "authd info: %s", parv[3]);
				inotice("authd info: %s", parv[3]);
				break;
			case 'W':
				sendto_realops_snomask(SNO_GENERAL, L_ALL, "authd WARNING: %s", parv[3]);
				iwarn("authd warning: %s", parv[3]);
				break;
			case 'C':
				sendto_realops_snomask(SNO_GENERAL, L_ALL, "authd CRITICAL: %s", parv[3]);
				ierror("authd critical: %s", parv[3]);
				break;
			default:
				sendto_realops_snomask(SNO_GENERAL, L_ALL, "authd sent us an unknown oper notice type (%s): %s", parv[2], parv[3]);
				ilog(L_MAIN, "authd unknown oper notice type (%s): %s", parv[2], parv[3]);
				break;
			}

			/* NOTREACHED */
			break;
		case 'X':
		case 'Y':
		case 'Z':
			if(parc < 3)
			{
				ilog(L_MAIN, "authd sent a result with wrong number of arguments: got %d", parc);
				restart_authd();
				return;
			}

			/* Select by type */
			switch(*parv[2])
			{
			case 'D':
				/* parv[0] conveys status */
				if(parc < 4)
				{
					ilog(L_MAIN, "authd sent a result with wrong number of arguments: got %d", parc);
					restart_authd();
					return;
				}
				dns_stats_results_callback(parv[1], parv[0], parc - 3, (const char **)&parv[3]);
				break;
			default:
				break;
			}
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
