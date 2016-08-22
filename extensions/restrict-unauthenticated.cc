/*
 * restrict unauthenticated users from doing anything as channel op
 */

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

	if (suser(user(*data->client)).empty())
		data->approved = 0;
}

DECLARE_MODULE_AV2(restrict_unauthenticated, NULL, NULL, NULL, NULL,
			restrict_unauthenticated_hfnlist, NULL, NULL, restrict_desc);
