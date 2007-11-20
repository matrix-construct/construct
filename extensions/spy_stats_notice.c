/*
 *  ircd-ratbox: A slightly useful ircd.
 *  spy_stats_notice.c: Sends a notice when someone uses STATS.
 *
 *  Copyright (C) 2002 by the past and present ircd coders, and others.
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
 *  $Id: spy_stats_notice.c 498 2006-01-15 16:40:33Z jilles $
 */
#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"

void show_stats(hook_data_int *);

mapi_hfn_list_av1 stats_hfnlist[] = {
	{"doing_stats", (hookfn) show_stats},
	{NULL, NULL}
};

DECLARE_MODULE_AV1(stats_spy, NULL, NULL, NULL, NULL, stats_hfnlist, "$Revision: 498 $");

void
show_stats(hook_data_int *data)
{
	char statchar = (char) data->arg2;

	if(statchar == 'L' || statchar == 'l')
	{
		const char *name = data->arg1;

		if(!EmptyString(name))
			sendto_realops_snomask(SNO_SPY, L_ALL,
					"STATS %c requested by %s (%s@%s) [%s] on %s",
					statchar, data->client->name,
					data->client->username,
					data->client->host,
					data->client->servptr->name, name);
		else
			sendto_realops_snomask(SNO_SPY, L_ALL,
					"STATS %c requested by %s (%s@%s) [%s]",
					statchar, data->client->name,
					data->client->username,
					data->client->host, data->client->servptr->name);
	}
	else
	{
		sendto_realops_snomask(SNO_SPY, L_ALL,
				"STATS %c requested by %s (%s@%s) [%s]",
				statchar, data->client->name, data->client->username,
				data->client->host, data->client->servptr->name);
	}
}
