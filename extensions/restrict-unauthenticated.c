/*
 * restrict unauthenticated users from doing anything as channel op
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

static void hack_channel_access(void *data);

mapi_hfn_list_av1 restrict_unauthenticated_hfnlist[] = {
	{ "get_channel_access", (hookfn) hack_channel_access },
	{ NULL, NULL }
};

static void
hack_channel_access(void *vdata)
{
	hook_data_channel_approval *data = (hook_data_channel_approval *) vdata;

	if (!MyClient(data->client))
		return;

	if (EmptyString(data->client->user->suser))
		data->approved = 0;
}

DECLARE_MODULE_AV1(restrict_unauthenticated, NULL, NULL, NULL, NULL,
			restrict_unauthenticated_hfnlist, "$Revision: 3526 $");
