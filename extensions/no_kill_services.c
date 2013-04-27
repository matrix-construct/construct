/*
 * Stop services kills
 * Well, it won't stop them all, unless this is loaded on all servers.
 *
 * Copyright (C) 2013 Elizabeth Myers. All rights reserved.
 * Licensed under the WTFPLv2
 */

#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "hash.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_serv.h"
#include "numeric.h"
#include "privilege.h"
#include "s_newconf.h"

static void block_services_kill(void *data);

mapi_hfn_list_av1 no_kill_services_hfnlist[] = {
	{ "can_kill", (hookfn) block_services_kill },
	{ NULL, NULL }
};

static void
block_services_kill(void *vdata)
{
	hook_data_client_approval *data = (hook_data_client_approval *) vdata;

	if (!MyClient(data->client))
		return;

	if (!data->approved)
		return;

	if (IsService(data->target))
	{
		sendto_one_numeric(data->client, ERR_ISCHANSERVICE,
				"KILL %s :Cannot kill a network service",
				data->target->name);
		data->approved = 0;
	}
}

DECLARE_MODULE_AV1(no_kill_services, NULL, NULL, NULL, NULL,
			no_kill_services_hfnlist, "Charybdis 3.4+");
