using namespace ircd;

static const char chm_sslonly_desc[] =
	"Adds channel mode +S that bans non-SSL users from joing a channel";

static void h_can_join(hook_data_channel *);

mapi_hfn_list_av1 sslonly_hfnlist[] = {
	{ "can_join", (hookfn) h_can_join },
	{ NULL, NULL }
};

static chan::mode::type mymode;

static int
_modinit(void)
{
	using namespace chan::mode;

	mymode = add('S', category::D, functor::simple);
	if (mymode == 0)
		return -1;

	return 0;
}

static void
_moddeinit(void)
{
	chan::mode::orphan('S');
}

DECLARE_MODULE_AV2(chm_sslonly, _modinit, _moddeinit, NULL, NULL, sslonly_hfnlist, NULL, NULL, chm_sslonly_desc);

static void
h_can_join(hook_data_channel *data)
{
	struct Client *source_p = data->client;
	struct Channel *chptr = data->chptr;

	if((chptr->mode.mode & mymode) && !IsSSLClient(source_p)) {
		/* XXX This is equal to ERR_THROTTLE */
		sendto_one_numeric(source_p, 480, "%s :Cannot join channel (+S) - SSL/TLS required", chptr->chname);
		data->approved = chan::mode::ERR_CUSTOM;
	}
}

