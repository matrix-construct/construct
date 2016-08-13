/*
 * restrict unauthenticated users from doing anything as channel op
 */

#include <ircd/stdinc.h>
#include <ircd/modules.h>
#include <ircd/hook.h>
#include <ircd/client.h>
#include <ircd/ircd.h>
#include <ircd/send.h>
#include <ircd/hash.h>
#include <ircd/s_conf.h>
#include <ircd/s_user.h>
#include <ircd/s_serv.h>
#include <ircd/numeric.h>
#include <ircd/privilege.h>
#include <ircd/s_newconf.h>

using namespace ircd;

static const char restrict_desc[] =
	"Restrict unautenticated users from doing anything as channel ops";

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

DECLARE_MODULE_AV2(restrict_unauthenticated, NULL, NULL, NULL, NULL,
			restrict_unauthenticated_hfnlist, NULL, NULL, restrict_desc);
