/*
 * Override WHOIS logic to hide channel memberships that are not common.
 *   -- kaniini
 */

using namespace ircd;

static const char hide_desc[] = "Hides channel memberships not shared";

static void h_huc_doing_whois_channel_visibility(hook_data_client *);

mapi_hfn_list_av1 huc_hfnlist[] = {
	{ "doing_whois_channel_visibility", (hookfn) h_huc_doing_whois_channel_visibility },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(hide_uncommon_channels, NULL, NULL, NULL, NULL, huc_hfnlist, NULL, NULL, hide_desc);

static void
h_huc_doing_whois_channel_visibility(hook_data_client *hdata)
{
	hdata->approved = (is_public(hdata->chptr) && !is_invisible(*hdata->target)) || is_member(hdata->chptr, hdata->client);
}
