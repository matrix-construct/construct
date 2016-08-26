/*
 * Remote client nick change notices.
 */

using namespace ircd;

static const char sno_desc[] =
	"Adds server notices for remote nick changes";

static int _modinit(void);
static void h_gnc_nick_change(hook_data *data);

mapi_hfn_list_av1 gcn_hfnlist[] = {
	{ "remote_nick_change", (hookfn) h_gnc_nick_change },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(globalnickchange, _modinit, NULL, NULL, NULL, gcn_hfnlist, NULL, NULL, sno_desc);

static int
_modinit(void)
{
	/* show the fact that we are showing user information in /version */
	opers_see_all_users = true;

	return 0;
}

static void
h_gnc_nick_change(hook_data *data)
{
	client::client *source_p = data->client;
	const char *oldnick = (const char *)data->arg1;
	const char *newnick = (const char *)data->arg2;

	sendto_realops_snomask_from(sno::NCHANGE, L_ALL, source_p->servptr,
				"Nick change: From %s to %s [%s@%s]",
				oldnick, newnick, source_p->username, source_p->host);
}
