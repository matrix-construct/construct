/*
 * m_override.c: Enhanced oper-override for Charybdis.
 *
 * Adds the OVERRIDE command, which can be used to bypass some channel
 * permissions.
 *
 * Use of this module requires the oper:override permission.
 *
 * The main differences between m_override.c and the older override.c are that
 * m_override targets a specific channel (to prevent accidentally overriding
 * on other channels) and that m_override also prevents overriding opers from
 * being kicked (except by other opers).
 */

#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "hash.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_serv.h"
#include "numeric.h"
#include "privilege.h"
#include "s_newconf.h"
#include "s_assert.h"

static int _modinit(void);
static void _moddeinit(void);
static void hack_channel_access(void *data);
static void hack_can_join(void *data);
static void hack_can_send(void *data);
static void hack_can_kick(void *data);
static void handle_new_server(void *data);
static void handle_client_exit(void *data);
static int mo_override(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static int me_override(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static int mo_unoverride(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static int me_unoverride(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static int me_sendoverride(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static void expire_override_deadlines(void *unused);

struct Message override_msgtab = {
	"OVERRIDE", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, mg_not_oper, mg_ignore, mg_ignore, {me_override, 2}, {mo_override, 2}}
};

struct Message unoverride_msgtab = {
	"UNOVERRIDE", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, mg_not_oper, mg_ignore, mg_ignore, {me_unoverride, 2}, {mo_unoverride, 2}}
};

struct Message sendoverride_msgtab = {
	"SENDOVERRIDE", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {me_sendoverride, 1}, mg_ignore}
};

mapi_clist_av1 override_clist[] = { &override_msgtab, &unoverride_msgtab, &sendoverride_msgtab, NULL };

mapi_hfn_list_av1 override_hfnlist[] = {
	{ "get_channel_access", (hookfn) hack_channel_access },
	{ "can_join", (hookfn) hack_can_join },
	{ "can_send", (hookfn) hack_can_send },
	{ "can_kick", (hookfn) hack_can_kick },
	{ "server_eob", (hookfn) handle_new_server },
	{ "client_exit", (hookfn) handle_client_exit },
	{ NULL, NULL }
};

struct ev_entry *expire_override_deadlines_ev = NULL;
static const int expire_override_interval = 60;

struct OverrideSession {
	rb_dlink_node node;

	struct Client *client;
	char channel[CHANNELLEN + 1];

	time_t deadline;
};

DECLARE_MODULE_AV1(override, _modinit, _moddeinit, override_clist, NULL, override_hfnlist, "1.0.0");

rb_dlink_list overriding_opers = { NULL, NULL, 0 };

#define IsOperOverride(x)	(HasPrivilege((x), "oper:override"))

static struct OverrideSession *
add_override(struct Client *source_p, const char *channel)
{
	struct OverrideSession *session_p = rb_malloc(sizeof(struct OverrideSession));

	session_p->client = source_p;
	rb_strlcpy(session_p->channel, channel, sizeof session_p->channel);

	rb_dlinkAdd(session_p, &session_p->node, &overriding_opers);

	if (MyClient(source_p)) {
		sendto_server(NULL, NULL, CAP_TS6|CAP_ENCAP, NOCAPS, ":%s ENCAP * OVERRIDE %s",
			session_p->client->id, session_p->channel);
	}

	return session_p;
}

static void
del_override(struct OverrideSession *session_p, int skip_propagation)
{
	s_assert(session_p != NULL);
	if (session_p == NULL)
		return;

	rb_dlinkDelete(&session_p->node, &overriding_opers);

	if (!skip_propagation && MyClient(session_p->client)) {
		sendto_server(NULL, NULL, CAP_TS6|CAP_ENCAP, NOCAPS, ":%s ENCAP * UNOVERRIDE %s",
			session_p->client->id, session_p->channel);
	}

	rb_free(session_p);
}

static struct OverrideSession *
find_override(struct Client *target_p, const char *channel)
{
	/*
	 * Only IsOper is checked here (and not IsOperOverride) because target_p may
	 * be a remote client (and also because it's very unlikely that they were
	 * IsOperOverride before but now suddenly IsOper but not IsOperOverride).
	 * --mr_flea
	 */
	if (!IsOper(target_p))
		return NULL;

	rb_dlink_node *n;
	RB_DLINK_FOREACH(n, overriding_opers.head) {
		struct OverrideSession *s = n->data;

		if (s->client == target_p && strcmp(s->channel, channel) == 0) {
			return s;
		}
	}

	return NULL;
}

static void
update_session_deadline(struct OverrideSession *session_p)
{
	s_assert(session_p != NULL);
	if (session_p == NULL)
		return;

	s_assert(MyClient(session_p->client));
	if (!MyClient(session_p->client))
		return;

	session_p->deadline = rb_current_time() + 1800;
}

static void
expire_override_deadlines(void *unused)
{
	rb_dlink_node *n, *tn;

	time_t warning_cutoff_low = rb_current_time() + 300 - expire_override_interval;
	time_t warning_cutoff_high = rb_current_time() + 300;

	RB_DLINK_FOREACH_SAFE(n, tn, overriding_opers.head) {
		struct OverrideSession *session_p = n->data;

		if (session_p->deadline == 0 || !MyClient(session_p->client))
			continue;
		else if (session_p->deadline < rb_current_time()) {
			sendto_one_notice(session_p->client, ":*** Oper-override on %s has expired",
				session_p->channel);
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "Oper-override by %s on %s expired",
				get_oper_name(session_p->client), session_p->channel);
			del_override(session_p, 0);
		}
		else if (session_p->deadline > warning_cutoff_low && session_p->deadline <= warning_cutoff_high) {
			sendto_one_notice(session_p->client, ":*** Oper-override on %s will expire in %u seconds",
				session_p->channel, (unsigned int) (session_p->deadline - rb_current_time()));
		}
	}
}

static void
hack_channel_access(void *vdata)
{
	hook_data_channel_approval *data = (hook_data_channel_approval *) vdata;

	if (data->approved == CHFL_CHANOP)
		return;

	struct OverrideSession *session_p = find_override(data->client, data->chptr->chname);
	if (session_p != NULL) {
		update_session_deadline(session_p);
		data->approved = CHFL_CHANOP;

		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s is using oper-override on %s (modehacking)",
				       get_oper_name(data->client), data->chptr->chname);
	}
}

static void
hack_can_join(void *vdata)
{
	hook_data_channel *data = (hook_data_channel *) vdata;

	if (data->approved == 0)
		return;

	struct OverrideSession *session_p = find_override(data->client, data->chptr->chname);
	if (session_p != NULL) {
		update_session_deadline(session_p);
		data->approved = 0;

		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s is using oper-override on %s (banwalking)",
				       get_oper_name(data->client), data->chptr->chname);
	}
}

static void
hack_can_send(void *vdata)
{
	hook_data_channel_approval *data = (hook_data_channel_approval *) vdata;

	if (data->approved == CAN_SEND_NONOP || data->approved == CAN_SEND_OPV)
		return;

	struct OverrideSession *session_p = find_override(data->client, data->chptr->chname);
	if (session_p != NULL) {
		data->approved = CAN_SEND_NONOP;

		if (MyClient(data->client)) {
			update_session_deadline(session_p);
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s is using oper-override on %s (forcing message)",
					       get_oper_name(data->client), data->chptr->chname);
		}
	}
}

static void
hack_can_kick(void *vdata)
{
	hook_data_channel_approval *data = (hook_data_channel_approval *) vdata;

	if (IsOper(data->client) || !data->approved)
		return;

	struct OverrideSession *session_p = find_override(data->target, data->chptr->chname);
	if (session_p != NULL) {
		data->approved = 0;

		if (MyClient(data->client)) {
			sendto_one_numeric(data->client, ERR_ISCHANSERVICE, "%s %s :User is immune to KICK",
				data->target->name, data->chptr->chname);
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s is using oper-override on %s (preventing KICK from %s)",
				get_oper_name(data->target), data->chptr->chname, data->client->name);
		}
	}
}

static void
handle_new_server(void *vdata)
{
	struct Client *source_p = (struct Client *) vdata;

	rb_dlink_node *n;
	RB_DLINK_FOREACH(n, overriding_opers.head) {
		struct OverrideSession *session_p = n->data;

		if (!MyClient(session_p->client))
			continue;

		sendto_one(source_p, ":%s ENCAP %s OVERRIDE %s",
			session_p->client->id, source_p->name, session_p->channel);
	}
}

static void
handle_client_exit(void *vdata)
{
	hook_data_client_exit *data = (hook_data_client_exit *) vdata;
	rb_dlink_node *n, *tn;
	struct Client *source_p = data->target;

	/*
	 * We iterate over this even if source_p isn't an oper because mode -o may 
	 * have been set while override was still active. --mr_flea
	 */
	RB_DLINK_FOREACH_SAFE(n, tn, overriding_opers.head) {
		struct OverrideSession *session_p = n->data;

		if (session_p->client != source_p)
			continue;

		if (MyClient(session_p->client)) {
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "Oper-override by %s on %s removed due to client quit",
				get_oper_name(session_p->client), session_p->channel);
		}

		del_override(session_p, 1);
	}	
}

static int
mo_override(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if (!IsOperOverride(source_p)) {
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name, source_p->name, "override");
		return 0;
	}

	if (!IsChannelName(parv[1]) || !check_channel_name(parv[1])) {
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL, form_str(ERR_NOSUCHCHANNEL), parv[1]);
		return 0;
	}

	struct OverrideSession *session_p = find_override(source_p, parv[1]);
	if (session_p != NULL) {
		update_session_deadline(session_p);
		sendto_one_notice(source_p, ":*** Oper-override deadline for %s extended", parv[1]);
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s has extended oper-override timeout on %s",
			get_oper_name(session_p->client), session_p->channel);
	} else {
		session_p = add_override(source_p, parv[1]);
		sendto_one_notice(source_p, ":*** Oper-override enabled on %s", parv[1]);
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s has enabled oper-override on %s",
			get_oper_name(session_p->client), session_p->channel);
	}

	return 0;
}

static int
me_override(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	add_override(source_p, parv[1]);

	return 0;
}

static int
mo_unoverride(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct OverrideSession *session_p = find_override(source_p, parv[1]);
	if (session_p) {
		sendto_one_notice(source_p, ":*** Oper-override disabled on %s", parv[1]);
		sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "%s has disabled oper-override on %s",
			get_oper_name(session_p->client), session_p->channel);
		del_override(session_p, 0);
	} else {
		sendto_one_notice(source_p, ":*** You are not overriding on %s", parv[1]);
	}

	return 0;
}

static int
me_unoverride(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct OverrideSession *session_p = find_override(source_p, parv[1]);
	s_assert(session_p != NULL);
	if (session_p)
		del_override(session_p, 0);

	return 0;
}

static int
me_sendoverride(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	s_assert(IsServer(source_p));
	if (IsServer(source_p))
		handle_new_server((void *) source_p);

	return 0;
}

static int
_modinit(void)
{
        expire_override_deadlines_ev = rb_event_add("expire_override_deadlines",
		expire_override_deadlines, NULL, expire_override_interval);

	/* Ask remote servers to send any existing overrides. */
	sendto_server(NULL, NULL, CAP_TS6|CAP_ENCAP, NOCAPS, ":%s ENCAP * SENDOVERRIDE", me.id);
	return 0;
}

static void
_moddeinit(void)
{
	rb_event_delete(expire_override_deadlines_ev);

	rb_dlink_node *n, *tn;
	RB_DLINK_FOREACH_SAFE(n, tn, overriding_opers.head) {
		struct OverrideSession *session_p = n->data;

		if (MyClient(session_p->client)) {
			sendto_one_notice(session_p->client, ":*** Oper-override on %s removed due to override module unloading",
				session_p->channel);
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE, "Oper-override by %s on %s removed due to override module unloading",
				get_oper_name(session_p->client), session_p->channel);
		}
		del_override(session_p, 0);
	}
}
