/*
 * Channel creation notices
 */

using namespace ircd;

static const char sno_desc[] =
	"Adds server notice mask +l that allows operators to receive channel creation notices";

static int _modinit(void);
static void _moddeinit(void);
static void h_scc_channel_join(void *);

mapi_hfn_list_av1 scc_hfnlist[] = {
	{ "channel_join", (hookfn) h_scc_channel_join },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(sno_channelcreate, _modinit, _moddeinit, NULL, NULL, scc_hfnlist, NULL, NULL, sno_desc);

static int
_modinit(void)
{
	/* add the snomask to the available slot */
	snomask_modes['l'] = find_snomask_slot();

	return 0;
}

static void
_moddeinit(void)
{
	/* disable the snomask and remove it from the available list */
	snomask_modes['l'] = 0;
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
		sendto_realops_snomask(snomask_modes['l'], L_NETWIDE, "%s is creating new channel %s",
					source_p->name, chptr->name.c_str());
	}
}
