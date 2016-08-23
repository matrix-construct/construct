/*
 * Helpops system.
 *   -- kaniini
 */

using namespace ircd;

static const char helpops_desc[] = "The helpops system as used by freenode";

static rb_dlink_list helper_list = { NULL, NULL, 0 };
static void h_hdl_stats_request(hook_data_int *hdata);
static void h_hdl_new_remote_user(client::client *client_p);
static void h_hdl_client_exit(hook_data_client_exit *hdata);
static void h_hdl_umode_changed(hook_data_umode_changed *hdata);
static void h_hdl_whois(hook_data_client *hdata);
static void mo_dehelper(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void me_dehelper(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void do_dehelper(client::client &source, client::client &target_p);

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

static void
mo_dehelper(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char **parv)
{
	client::client *target_p;

	if (!IsOperAdmin(&source))
	{
		sendto_one(&source, form_str(ERR_NOPRIVS), me.name, source.name, "admin");
		return;
	}

	if(!(target_p = client::find_named_person(parv[1])))
	{
		sendto_one_numeric(&source, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), parv[1]);
		return;
	}

	if(my(*target_p))
		do_dehelper(source, *target_p);
	else
		sendto_one(target_p, ":%s ENCAP %s DEHELPER %s",
				use_id(&source), target_p->servptr->name, use_id(target_p));
}

static void
me_dehelper(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char **parv)
{
	client::client *target_p = client::find_person(parv[1]);
	if(!target_p)
	{
		sendto_one_numeric(&source, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), parv[1]);
		return;
	}
	if(!my(*target_p))
		return;

	do_dehelper(source, *target_p);
}

static void
do_dehelper(client::client &source, client::client &target)
{
	const char *fakeparv[4];

	if(!(target.umodes & UMODE_HELPOPS))
		return;

	sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s is using DEHELPER on %s",
			source.name, target.name);
	sendto_one_notice(&target, ":*** %s is using DEHELPER on you", source.name);

	fakeparv[0] = fakeparv[1] = target.name;
	fakeparv[2] = "-H";
	fakeparv[3] = NULL;
	user_mode(&target, &target, 3, fakeparv);
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
	client::client *target_p;
	rb_dlink_node *helper_ptr;
	unsigned int count = 0;

	if (hdata->arg2 != 'p')
		return;

	RB_DLINK_FOREACH (helper_ptr, helper_list.head)
	{
		target_p = (client::client *)helper_ptr->data;

		if (away(user(*target_p)).size())
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
h_hdl_new_remote_user(client::client *client_p)
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
	client::client &source = *hdata->client;

	/* didn't change +H umode, we don't need to do anything */
	if (!((hdata->oldumodes ^ source.umodes) & UMODE_HELPOPS))
		return;

	if (source.umodes & UMODE_HELPOPS)
	{
		if (my(source) && !HasPrivilege(&source, "usermode:helpops"))
		{
			source.umodes &= ~UMODE_HELPOPS;
			sendto_one(&source, form_str(ERR_NOPRIVS), me.name, source.name, "usermode:helpops");
			return;
		}

		rb_dlinkAddAlloc(&source, &helper_list);
	}
	else if (!(source.umodes & UMODE_HELPOPS))
		rb_dlinkFindDestroy(&source, &helper_list);
}

static void
h_hdl_whois(hook_data_client *hdata)
{
	client::client &source = *hdata->client;
	client::client *target_p = hdata->target;

	if ((target_p->umodes & UMODE_HELPOPS) && away(user(*target_p)).empty())
	{
		sendto_one_numeric(&source, RPL_WHOISHELPOP, form_str(RPL_WHOISHELPOP), target_p->name);
	}
}

DECLARE_MODULE_AV2(helpops, _modinit, _moddeinit, helpops_clist, NULL, helpops_hfnlist, NULL, NULL, helpops_desc);
