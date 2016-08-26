/*
 * Do not allow operators to be kicked from +M channels.
 *     -- kaniini
 */

using namespace ircd;

static const char chm_operpeace_desc[] =
	"Adds channel mode +M which prohibits operators from being kicked";

static void hdl_can_kick(hook_data_channel_approval *);

mapi_hfn_list_av1 chm_operpeace_hfnlist[] = {
	{ "can_kick", (hookfn) hdl_can_kick },
	{ NULL, NULL }
};

static chan::mode::type mymode;

static int
_modinit(void)
{
	using namespace chan::mode;

	mymode = add('M', category::D, functor::hidden);
	if (mymode == 0)
		return -1;

	return 0;
}

static void
_moddeinit(void)
{
	chan::mode::orphan('M');
}

DECLARE_MODULE_AV2(chm_operpeace, _modinit, _moddeinit, NULL, NULL, chm_operpeace_hfnlist, NULL, NULL, chm_operpeace_desc);

static void
hdl_can_kick(hook_data_channel_approval *data)
{
	client::client *source_p = data->client;
	client::client *who = data->target;
	const auto &chptr(data->chptr);

	if(is(*source_p, umode::OPER))
		return;

	if((chptr->mode.mode & mymode) && is(*who, umode::OPER))
	{
		sendto_realops_snomask(sno::GENERAL, L_NETWIDE, "%s attempted to kick %s from %s (which is +M)",
			source_p->name, who->name, chptr->name.c_str());
		sendto_one_numeric(source_p, ERR_ISCHANSERVICE, "%s %s :Cannot kick IRC operators from that channel.",
			who->name, chptr->name.c_str());
		data->approved = 0;
	}
}
