/*
 * Remote oper up notices.
 */

using namespace ircd;

static const char sno_desc[] =
	"Adds server notices for remote oper up";

static void h_sgo_umode_changed(void *);

mapi_hfn_list_av1 sgo_hfnlist[] = {
	{ "umode_changed", (hookfn) h_sgo_umode_changed },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(sno_globaloper, NULL, NULL, NULL, NULL, sgo_hfnlist, NULL, NULL, sno_desc);

static void
h_sgo_umode_changed(void *vdata)
{
	hook_data_umode_changed *data = (hook_data_umode_changed *)vdata;
	client::client *source_p = data->client;

	if (my_connect(*source_p) || !has_sent_eob(*source_p->servptr))
		return;

	if (!(data->oldumodes & umode::OPER) && is(*source_p, umode::OPER))
		sendto_realops_snomask_from(SNO_GENERAL, L_ALL, source_p->servptr,
				"%s (%s@%s) is now an operator",
				source_p->name, source_p->username, source_p->host);
}
