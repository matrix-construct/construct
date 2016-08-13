/*
 * Override WHOIS logic to hide channel memberships that are not common.
 *   -- kaniini
 */

#include <ircd/stdinc.h>
#include <ircd/modules.h>
#include <ircd/client.h>
#include <ircd/hook.h>
#include <ircd/ircd.h>
#include <ircd/send.h>
#include <ircd/s_conf.h>
#include <ircd/s_newconf.h>

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
	hdata->approved = ((PubChannel(hdata->chptr) && !IsInvisible(hdata->target)) || IsMember((hdata->client), (hdata->chptr)));
}
