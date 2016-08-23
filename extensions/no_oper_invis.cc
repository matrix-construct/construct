/*
 * Deny opers setting themselves +i unless they are bots (i.e. have
 * hidden_oper privilege).
 * -- jilles
 */

using namespace ircd;

static const char noi_desc[] =
	"Disallow operators from setting user mode +i on themselves";

static void h_noi_umode_changed(hook_data_umode_changed *);

mapi_hfn_list_av1 noi_hfnlist[] = {
	{ "umode_changed", (hookfn) h_noi_umode_changed },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(no_oper_invis, NULL, NULL, NULL, NULL, noi_hfnlist, NULL, NULL, noi_desc);

static void
h_noi_umode_changed(hook_data_umode_changed *hdata)
{
	client::client *source_p = hdata->client;

	if (my(*source_p) && IsOper(source_p) && !IsOperInvis(source_p) &&
			is_invisible(*source_p))
	{
		ClearInvisible(source_p);
		/* If they tried /umode +i, complain; do not complain
		 * if they opered up while invisible -- jilles */
		if (hdata->oldumodes & UMODE_OPER)
			sendto_one_notice(source_p, ":*** Opers may not set themselves invisible");
	}
}
