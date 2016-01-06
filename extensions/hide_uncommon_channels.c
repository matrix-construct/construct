/*
 * Override WHOIS logic to hide channel memberships that are not common.
 *   -- kaniini
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "hook.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "s_newconf.h"

static void h_huc_doing_whois_channel_visibility(hook_data_client *);

mapi_hfn_list_av1 huc_hfnlist[] = {
	{ "doing_whois_channel_visibility", (hookfn) h_huc_doing_whois_channel_visibility },
	{ NULL, NULL }
};

DECLARE_MODULE_AV1(hide_uncommon_channels, NULL, NULL, NULL, NULL, huc_hfnlist, "");

static void
h_huc_doing_whois_channel_visibility(hook_data_client *hdata)
{
	hdata->approved = ((PubChannel(hdata->chptr) && !IsInvisible(hdata->target)) || IsMember((hdata->client), (hdata->chptr)));
}
