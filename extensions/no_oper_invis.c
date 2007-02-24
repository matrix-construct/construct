/*
 * Deny opers setting themselves +i unless they are bots (i.e. have
 * hidden_oper privilege).
 * -- jilles
 *
 * $Id: no_oper_invis.c 3219 2007-02-24 19:34:28Z jilles $
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

DECLARE_MODULE_AV1(no_oper_invis, NULL, NULL, NULL, NULL, noi_hfnlist, "$Revision: 3219 $");

static void
h_noi_umode_changed(hook_data_umode_changed *hdata)
{
	struct Client *source_p = hdata->client;

	if (MyClient(source_p) && IsOper(source_p) && !IsOperInvis(source_p) &&
			IsInvisible(source_p))
	{
		ClearInvisible(source_p);
		/* If they tried /umode +i, complain; do not complain
		 * if they opered up while invisible -- jilles */
		if (hdata->oldumodes & UMODE_OPER)
			sendto_one_notice(source_p, ":*** Opers may not set themselves invisible");
	}
}
