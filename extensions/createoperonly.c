/*
 * This module restricts channel creation to opered up users
 * only. This module could be useful for running private chat
 * systems, or if a network gets droneflood problems. It will
 * return ERR_NEEDREGGEDNICK on failure.
 *    -- nenolod
 *
 * $Id: createoperonly.c 3476 2007-05-24 04:28:36Z nenolod $
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "hook.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "snomask.h"
#include "numeric.h"

static void h_can_create_channel_authenticated(hook_data_client_approval *);

mapi_hfn_list_av1 restrict_hfnlist[] = {
	{ "can_create_channel", (hookfn) h_can_create_channel_authenticated },
	{ NULL, NULL }
};

DECLARE_MODULE_AV1(createoperonly, NULL, NULL, NULL, NULL, restrict_hfnlist, "$Revision: 3476 $");

static void
h_can_create_channel_authenticated(hook_data_client_approval *data)
{
	struct Client *source_p = data->client;

	if (!IsOper(source_p))
	{
		sendto_one_notice(source_p, ":*** Channel creation is restricted to network staff only.");
		data->approved = ERR_NEEDREGGEDNICK;
	}
}
