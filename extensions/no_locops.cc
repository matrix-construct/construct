/*
 * Disable LOCOPS (by disallowing any local user setting +l).
 * -- jilles
 */

using namespace ircd;

static const char no_locops_desc[] = "Disables local operators";

static void h_nl_umode_changed(hook_data_umode_changed *);

mapi_hfn_list_av1 nl_hfnlist[] = {
	{ "umode_changed", (hookfn) h_nl_umode_changed },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(no_locops, NULL, NULL, NULL, NULL, nl_hfnlist, NULL, NULL, no_locops_desc);

static void
h_nl_umode_changed(hook_data_umode_changed *hdata)
{
	struct Client *source_p = hdata->client;

	if (MyClient(source_p) && source_p->umodes & UMODE_LOCOPS)
	{
		source_p->umodes &= ~UMODE_LOCOPS;
	}
}
