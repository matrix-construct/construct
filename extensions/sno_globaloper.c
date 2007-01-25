/*
 * Remote oper up notices.
 *
 * $Id: sno_globaloper.c 639 2006-01-29 21:42:06Z jilles $
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "hook.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "snomask.h"

static void h_sgo_umode_changed(void *);

mapi_hfn_list_av1 sgo_hfnlist[] = {
	{ "umode_changed", (hookfn) h_sgo_umode_changed },
	{ NULL, NULL }
};

DECLARE_MODULE_AV1(sno_globaloper, NULL, NULL, NULL, NULL, sgo_hfnlist, "$Revision: 639 $");

static void
h_sgo_umode_changed(void *vdata)
{
	hook_data_umode_changed *data = (hook_data_umode_changed *)vdata;
	struct Client *source_p = data->client;

	if (MyConnect(source_p) || !HasSentEob(source_p->servptr))
		return;

	if (!(data->oldumodes & UMODE_OPER) && IsOper(source_p))
		sendto_realops_snomask_from(SNO_GENERAL, L_ALL, source_p->servptr,
				"%s (%s@%s) is now an operator",
				source_p->name, source_p->username, source_p->host);
}
