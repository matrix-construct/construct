using namespace ircd;

static const char chm_adminonly_desc[] =
	"Enables channel mode +A that blocks non-admins from joining a channel";

static void h_can_join(hook_data_channel *);

mapi_hfn_list_av1 adminonly_hfnlist[] = {
	{ "can_join", (hookfn) h_can_join },
	{ NULL, NULL }
};

static chan::mode::type mymode;

static int
_modinit(void)
{
	using namespace chan::mode;

	mymode = add('A', category::D, functor::staff);
	if (mymode == 0)
		return -1;

	return 0;
}

static void
_moddeinit(void)
{
	chan::mode::orphan('A');
}

DECLARE_MODULE_AV2(chm_adminonly, _modinit, _moddeinit, NULL, NULL, adminonly_hfnlist, NULL, NULL, chm_adminonly_desc);

static void
h_can_join(hook_data_channel *data)
{
	struct Client *source_p = data->client;
	struct Channel *chptr = data->chptr;

	if((chptr->mode.mode & mymode) && !IsAdmin(source_p)) {
		sendto_one_numeric(source_p, 519, "%s :Cannot join channel (+A) - you are not an IRC server administrator", chptr->chname);
		data->approved = chan::mode::ERR_CUSTOM;
	}
}

