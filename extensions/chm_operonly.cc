using namespace ircd;

static const char chm_operonly_desc[] =
	"Adds channel mode +O which makes a channel operator-only";

static void h_can_join(hook_data_channel *);

mapi_hfn_list_av1 operonly_hfnlist[] = {
	{ "can_join", (hookfn) h_can_join },
	{ NULL, NULL }
};

static chan::mode::type mymode;

static int
_modinit(void)
{
	using namespace chan::mode;

	mymode = add('O', category::D, functor::staff);
	if (mymode == 0)
		return -1;

	return 0;
}


static void
_moddeinit(void)
{
	chan::mode::orphan('O');
}

DECLARE_MODULE_AV2(chm_operonly, _modinit, _moddeinit, NULL, NULL, operonly_hfnlist, NULL, NULL, chm_operonly_desc);

static void
h_can_join(hook_data_channel *data)
{
	client::client *source_p = data->client;
	const auto &chptr(data->chptr);

	if((chptr->mode.mode & mymode) && !is(*source_p, umode::OPER)) {
		sendto_one_numeric(source_p, 520, "%s :Cannot join channel (+O) - you are not an IRC operator", chptr->name.c_str());
		data->approved = chan::mode::ERR_CUSTOM;
	}
}

