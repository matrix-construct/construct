/*
 * Deny user to remove +i flag except they are irc operators
 *
 * Based off no_oper_invis.c by jilles
 *
 * Note that +i must be included in default_umodes
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

static const char noi_desc[] =
	"Do not allow users to remove user mode +i unless they are operators";

static void h_noi_umode_changed(hook_data_umode_changed *);

mapi_hfn_list_av1 noi_hfnlist[] = {
	{ "umode_changed", (hookfn) h_noi_umode_changed },
	{ NULL, NULL }
};

DECLARE_MODULE_AV2(force_user_invis, NULL, NULL, NULL, NULL, noi_hfnlist, NULL, NULL, noi_desc);

static void
h_noi_umode_changed(hook_data_umode_changed *hdata)
{
	struct Client *source_p = hdata->client;

	if (MyClient(source_p) && !IsOper(source_p) && !IsInvisible(source_p)) {
		SetInvisible(source_p);
	}
}
