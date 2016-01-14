/*
 * Helpops system.
 *   -- kaniini
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "hook.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_newconf.h"
#include "numeric.h"

static rb_dlink_list helper_list = { NULL, NULL, 0 };
static void h_hdl_stats_request(hook_data_int *hdata);
static void h_hdl_new_remote_user(struct Client *client_p);
static void h_hdl_client_exit(hook_data_client_exit *hdata);
static void h_hdl_umode_changed(hook_data_umode_changed *hdata);
static void h_hdl_whois(hook_data_client *hdata);

mapi_hfn_list_av1 helpops_hfnlist[] = {
	{ "doing_stats", (hookfn) h_hdl_stats_request },
	{ "new_remote_user", (hookfn) h_hdl_new_remote_user },
	{ "client_exit", (hookfn) h_hdl_client_exit },
	{ "umode_changed", (hookfn) h_hdl_umode_changed },
	{ "doing_whois", (hookfn) h_hdl_whois },
	{ "doing_whois_global", (hookfn) h_hdl_whois },
	{ NULL, NULL }
};

static int UMODE_HELPOPS = 0;

static int
_modinit(void)
{
	/* add the usermode to the available slot */
	user_modes['H'] = UMODE_HELPOPS = find_umode_slot();
	construct_umodebuf();

	return 0;
}

static void
_moddeinit(void)
{
	/* disable the umode and remove it from the available list */
	user_modes['H'] = UMODE_HELPOPS = 0;
	construct_umodebuf();
}

static void
h_hdl_stats_request(hook_data_int *hdata)
{
	struct Client *target_p;
	rb_dlink_node *helper_ptr;
	unsigned int count = 0;

	if (hdata->arg2 != 'p')
		return;

	RB_DLINK_FOREACH (helper_ptr, helper_list.head)
	{
		target_p = helper_ptr->data;

		if(IsOperInvis(target_p) && !IsOper(hdata->client))
			continue;

		if(target_p->user->away)
			continue;

		count++;

		sendto_one_numeric(hdata->client, RPL_STATSDEBUG,
				   "p :%s (%s@%s)",
				   target_p->name, target_p->username,
				   target_p->host);
	}

	sendto_one_numeric(hdata->client, RPL_STATSDEBUG,
		"p :%u staff members", count);

	hdata->result = 1;
}

static void
h_hdl_new_remote_user(struct Client *client_p)
{
	if (client_p->umodes & UMODE_HELPOPS)
		rb_dlinkAddAlloc(client_p, &helper_list);
}

static void
h_hdl_client_exit(hook_data_client_exit *hdata)
{
	if (hdata->target->umodes & UMODE_HELPOPS)
		rb_dlinkFindDestroy(hdata->target, &helper_list);
}

static void
h_hdl_umode_changed(hook_data_umode_changed *hdata)
{
	struct Client *source_p = hdata->client;

	/* didn't change +H umode, we don't need to do anything */
	if (!((hdata->oldumodes ^ source_p->umodes) & UMODE_HELPOPS))
		return;

	if (source_p->umodes & UMODE_HELPOPS)
	{
		if (MyClient(source_p) && !HasPrivilege(source_p, "usermode:helpops"))
		{
			source_p->umodes &= ~UMODE_HELPOPS;
			sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "usermode:helpops");
			return;
		}

		rb_dlinkAddAlloc(source_p, &helper_list);
	}
	else if (!(source_p->umodes & UMODE_HELPOPS))
		rb_dlinkFindDestroy(source_p, &helper_list);
}

static void
h_hdl_whois(hook_data_client *hdata)
{
	struct Client *source_p = hdata->client;
	struct Client *target_p = hdata->target;

	if ((target_p->umodes & UMODE_HELPOPS) && EmptyString(target_p->user->away))
	{
		sendto_one_numeric(source_p, RPL_WHOISHELPOP, form_str(RPL_WHOISHELPOP), target_p->name);
	}
}

DECLARE_MODULE_AV1(helpops, _modinit, _moddeinit, NULL, NULL, helpops_hfnlist, "");
