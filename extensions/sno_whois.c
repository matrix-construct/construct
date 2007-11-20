/*
 * +W snomask: Displays if a local user has done a WHOIS request on you.
 * derived from spy_whois_notice.c.
 *
 * If #define OPERONLY is removed, then any user can use this snomask
 * (you need to put ~servnotice in oper_only_umodes for this to work).
 *
 * $Id: sno_whois.c 3498 2007-05-30 10:22:25Z jilles $
 */

#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"

/* undefine this to allow anyone to receive whois notifications */
#define OPERONLY

void show_whois(hook_data_client *);

mapi_hfn_list_av1 whois_hfnlist[] = {
	{"doing_whois", (hookfn) show_whois},
	{"doing_whois_global", (hookfn) show_whois},
	{NULL, NULL}
};

static int
init(void)
{
	snomask_modes['W'] = find_snomask_slot();

	return 0;
}

static void
fini(void)
{
	snomask_modes['W'] = find_snomask_slot();
}

DECLARE_MODULE_AV1(sno_whois, init, fini, NULL, NULL, whois_hfnlist, "$Revision: 3498 $");

void
show_whois(hook_data_client *data)
{
	struct Client *source_p = data->client;
	struct Client *target_p = data->target;

	if(MyClient(target_p) && 
#ifdef OPERONLY
	   IsOper(target_p) &&
#endif
	   (source_p != target_p) &&
	   (target_p->snomask & snomask_modes['W']))
	{
		sendto_one_notice(target_p,
				":*** Notice -- %s (%s@%s) is doing a whois on you [%s]",
				source_p->name,
				source_p->username, source_p->host,
				source_p->servptr->name);
	}
}
