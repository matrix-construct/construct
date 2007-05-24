/*
 * +j snomask: Channel join notices.
 *    --nenolod
 *
 * To be discussed:
 *    + part notices?
 *
 * $Id: sno_channeljoin.c 3478 2007-05-24 15:10:06Z jilles $
 */

#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"

static void
show_channeljoin(hook_data_channel_activity *info)
{
	sendto_realops_snomask(snomask_modes['j'], L_ALL,
		"%s (%s@%s) has joined channel %s", info->client->name,
		info->client->username, info->client->host, info->chptr->chname);
}

mapi_hfn_list_av1 channeljoin_hfnlist[] = {
        {"channel_join", (hookfn) show_channeljoin},
        {NULL, NULL}
};

static int
init(void)
{
	snomask_modes['j'] = find_snomask_slot();

	return 0;
}

static void
fini(void)
{
	snomask_modes['j'] = 0;
}

DECLARE_MODULE_AV1(sno_channeljoin, init, fini, NULL, NULL, channeljoin_hfnlist, "$Revision: 3478 $");
