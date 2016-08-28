/*
 * Channel creation notices
 */

using namespace ircd;

static const char sno_desc[] =
	"Adds server notice mask +l that allows operators to receive channel creation notices";

static int _modinit(void);
static void h_scc_channel_join(void *);

mapi_hfn_list_av1 scc_hfnlist[] = {
	{ "channel_join", (hookfn) h_scc_channel_join },
	{ NULL, NULL }
};

sno::mode SNO_CHANNELCREATE { 'l' };

DECLARE_MODULE_AV2(sno_channelcreate, _modinit, nullptr, NULL, NULL, scc_hfnlist, NULL, NULL, sno_desc);

static int
_modinit(void)
{
	return 0;
}

static void
h_scc_channel_join(void *vdata)
{
	hook_data_channel_activity *data = (hook_data_channel_activity *)vdata;
	const auto chptr(data->chptr);
	client::client *source_p = data->client;

	/* If they just joined a channel, and it only has one member, then they just created it. */
	if(size(chptr->members) == 1 && is_chanop(get(chptr->members, *source_p, std::nothrow)))
	{
		sendto_realops_snomask(SNO_CHANNELCREATE, L_NETWIDE, "%s is creating new channel %s",
					source_p->name, chptr->name.c_str());
	}
}
