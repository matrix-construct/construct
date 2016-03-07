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
static int mo_dehelper(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static int me_dehelper(struct MsgBuf *, struct Client *, struct Client *, int, const char **);
static int do_dehelper(struct Client *source_p, struct Client *target_p);

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

struct Message dehelper_msgtab = {
	"DEHELPER", 0, 0, 0, 0,
	{mg_unreg, mg_not_oper, mg_not_oper, mg_ignore, {me_dehelper, 2}, {mo_dehelper, 2}}
};

mapi_clist_av1 helpops_clist[] = { &dehelper_msgtab, NULL };

static int mo_dehelper(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	struct Client *target_p;

	if (!IsOperAdmin(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "admin");
		return 0;
	}

	if(!(target_p = find_named_person(parv[1])))
	{
		sendto_one_numeric(source_p, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), parv[1]);
		return 0;
	}

	if(MyClient(target_p))
		do_dehelper(source_p, target_p);
	else
		sendto_one(target_p, ":%s ENCAP %s DEHELPER %s",
				use_id(source_p), target_p->servptr->name, use_id(target_p));

	return 0;
}

static int me_dehelper(struct MsgBuf *msgbuf_p, struct Client *client_p, struct Client *source_p, int parc, const char **parv)
{
	struct Client *target_p = find_person(parv[1]);
	if(!target_p)
	{
		sendto_one_numeric(source_p, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), parv[1]);
		return 0;
	}
	if(!MyClient(target_p))
		return 0;

	do_dehelper(source_p, target_p);
	return 0;
}

static int do_dehelper(struct Client *source_p, struct Client *target_p)
{
	const char *fakeparv[4];

	if(!(target_p->umodes & UMODE_HELPOPS))
		return 0;

	sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s is using DEHELPER on %s",
			source_p->name, target_p->name);
	sendto_one_notice(target_p, ":*** %s is using DEHELPER on you", source_p->name);

	fakeparv[0] = fakeparv[1] = target_p->name;
	fakeparv[2] = "-H";
	fakeparv[3] = NULL;
	user_mode(target_p, target_p, 3, fakeparv);
	return 0;
}

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

DECLARE_MODULE_AV2(helpops, _modinit, _moddeinit, helpops_clist, NULL, helpops_hfnlist, NULL, NULL, NULL);
