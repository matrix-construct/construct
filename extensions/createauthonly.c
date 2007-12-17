/*
 * This module restricts channel creation to authenticated users
 * and IRC operators only. This module could be useful for
 * running private chat systems, or if a network gets droneflood
 * problems. It will return ERR_NEEDREGGEDNICK on failure.
 *    -- nenolod
 *
 * $Id: createauthonly.c 833 2006-02-15 00:27:59Z jilles $
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

DECLARE_MODULE_AV1(createauthonly, NULL, NULL, NULL, NULL, restrict_hfnlist, "$Revision: 833 $");

static void
h_can_create_channel_authenticated(hook_data_client_approval *data)
{
	struct Client *source_p = data->client;

	if (*source_p->user->suser == '\0' && !IsOper(source_p))
		data->approved = ERR_NEEDREGGEDNICK;
}
