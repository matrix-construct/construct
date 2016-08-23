/*
 * Shows notices if remote clients exit with "Bad user info" or
 * ConfigFileEntry.kline_reason.
 * Assumes client_exit is enabled so users can't fake these reasons,
 * and kline_reason is enabled and the same everywhere.
 * Yes, this is a hack, but it is simple and avoids sending
 * more data across servers -- jilles
 */

using namespace ircd;

static const char sno_desc[] =
	"Adds server notices for global XLINEs, KLINEs, and DLINEs";

static void h_gla_client_exit(hook_data_client_exit *);

mapi_hfn_list_av1 gla_hfnlist[] = {
	{ "client_exit", (hookfn) h_gla_client_exit },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(globallineactive, NULL, NULL, NULL, NULL, gla_hfnlist, NULL, NULL, sno_desc);

static void
h_gla_client_exit(hook_data_client_exit *hdata)
{
	client::client *source_p;

	source_p = hdata->target;

	if (my_connect(*source_p) || !is_client(*source_p))
		return;
	if (!strcmp(hdata->comment, "Bad user info"))
	{
		sendto_realops_snomask_from(SNO_GENERAL, L_ALL, source_p->servptr,
				"XLINE active for %s[%s@%s]",
				source_p->name, source_p->username, source_p->host);
	}
	else if (ConfigFileEntry.kline_reason != NULL &&
			!strcmp(hdata->comment, ConfigFileEntry.kline_reason))
	{
		sendto_realops_snomask_from(SNO_GENERAL, L_ALL, source_p->servptr,
				"K/DLINE active for %s[%s@%s]",
				source_p->name, source_p->username, source_p->host);
	}
	else if (!strncmp(hdata->comment, "Temporary K-line ", 17))
	{
		sendto_realops_snomask_from(SNO_GENERAL, L_ALL, source_p->servptr,
				"K/DLINE active for %s[%s@%s]",
				source_p->name, source_p->username, source_p->host);
	}
	else if (!strncmp(hdata->comment, "Temporary D-line ", 17))
	{
		sendto_realops_snomask_from(SNO_GENERAL, L_ALL, source_p->servptr,
				"K/DLINE active for %s[%s@%s]",
				source_p->name, source_p->username, source_p->host);
	}
}
