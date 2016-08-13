/*
 * This module restricts channel creation to opered up users
 * only. This module could be useful for running private chat
 * systems, or if a network gets droneflood problems. It will
 * return ERR_NEEDREGGEDNICK on failure.
 *    -- nenolod
 */

#include <ircd/stdinc.h>
#include <ircd/modules.h>
#include <ircd/client.h>
#include <ircd/hook.h>
#include <ircd/ircd.h>
#include <ircd/send.h>
#include <ircd/s_conf.h>
#include <ircd/snomask.h>
#include <ircd/numeric.h>

using namespace ircd;

static const char restrict_desc[] = "Restricts channel creation to IRC operators";

static void h_can_create_channel_authenticated(hook_data_client_approval *);

mapi_hfn_list_av1 restrict_hfnlist[] = {
	{ "can_create_channel", (hookfn) h_can_create_channel_authenticated },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(createoperonly, NULL, NULL, NULL, NULL, restrict_hfnlist, NULL, NULL, restrict_desc);

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
