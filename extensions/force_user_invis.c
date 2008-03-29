/*
 * Deny user to remove +i flag except they are irc operators
 *
 * Based off no_oper_invis.c by jilles
 *
 * Note that +i must be included in default_umodes
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "hook.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "s_newconf.h"

static void h_noi_umode_changed(hook_data_umode_changed *);

mapi_hfn_list_av1 noi_hfnlist[] = {
	{ "umode_changed", (hookfn) h_noi_umode_changed },
	{ NULL, NULL }
};

DECLARE_MODULE_AV1(force_user_invis, NULL, NULL, NULL, NULL, noi_hfnlist, "1.0.0");

static void
h_noi_umode_changed(hook_data_umode_changed *hdata)
{
	struct Client *source_p = hdata->client;

	if (MyClient(source_p) && !IsOper(source_p) && !IsInvisible(source_p)) {
		SetInvisible(source_p);
	}
}
