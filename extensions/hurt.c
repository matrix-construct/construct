/*
 * charybdis: an advanced Internet Relay Chat Daemon(ircd).
 *
 * Copyright (C) 2006 charybdis development team
 * All rights reserved
 *
 * $Id: hurt.c 3161 2007-01-25 07:23:01Z nenolod $
 */
#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "numeric.h"
#include "hostmask.h"
#include "s_conf.h"
#include "s_newconf.h"
#include "hash.h"

/* {{{ Structures */
#define HURT_CUTOFF             (10)            /* protocol messages. */
#define HURT_DEFAULT_EXPIRE     (7 * 24 * 60)   /* minutes. */
#define HURT_EXIT_REASON        "Hurt: Failed to identify to services"

enum {
        HEAL_NICK = 0,
        HEAL_IP
};

typedef struct _hurt_state {
        time_t start_time;
        uint32_t n_hurts;
        rb_dlink_list hurt_clients;
        uint16_t cutoff;
        time_t default_expire;
        const char *exit_reason;
} hurt_state_t;

typedef struct _hurt {
        char *ip;
        struct sockaddr *saddr;
        int saddr_bits;
        char *reason;
        time_t expire;
} hurt_t;
/* }}} */

/* {{{ Prototypes */
static int mo_hurt(struct Client *, struct Client *, int, const char **);
static int me_hurt(struct Client *, struct Client *, int, const char **);
static int mo_heal(struct Client *, struct Client *, int, const char **);
static int me_heal(struct Client *, struct Client *, int, const char **);

static int modinit(void);
static void modfini(void);

static void client_exit_hook(hook_data_client_exit *);
static void new_local_user_hook(struct Client *);
static void doing_stats_hook(hook_data_int *hdata);

static void hurt_check_event(void *);
static void hurt_expire_event(void *);

static hurt_t *hurt_new(time_t, const char *, const char *);
static void hurt_add(hurt_t *);
static void hurt_propagate(struct Client *, struct Client *, hurt_t *);
static hurt_t *hurt_find(const char *ip);
static hurt_t *hurt_find_exact(const char *ip);
static void hurt_remove(const char *ip);
static void hurt_destroy(void *hurt);

static int heal_nick(struct Client *, struct Client *);

static int nick_is_valid(const char *);
/* }}} */

/* {{{ State containers */

rb_dlink_list hurt_confs = { NULL, NULL, 0 };

/* }}} */

/* {{{ Messages */
struct Message hurt_msgtab = {
	"HURT", 0, 0, 0, MFLG_SLOW, {
		mg_ignore, mg_ignore, mg_ignore,
		mg_ignore, {me_hurt, 0}, {mo_hurt, 3}
	}
};

struct Message heal_msgtab = {
	"HEAL", 0, 0, 0, MFLG_SLOW, {
		mg_ignore, mg_ignore, mg_ignore,
		mg_ignore, {me_heal, 0}, {mo_heal, 2}
	}
};
/* }}} */

/* {{{ Misc module stuff */
mapi_hfn_list_av1 hurt_hfnlist[] = {
	{"client_exit",		(hookfn) client_exit_hook},
	{"new_local_user",	(hookfn) new_local_user_hook},
	{"doing_stats",		(hookfn) doing_stats_hook},
	{NULL, 			NULL},
};

mapi_clist_av1 hurt_clist[] = { &hurt_msgtab, &heal_msgtab, NULL };

DECLARE_MODULE_AV1(
	hurt,
	modinit,
	modfini,
	hurt_clist,
	NULL,
	hurt_hfnlist,
	"$Revision: 3161 $"
);
/* }}} */

hurt_state_t hurt_state = {
	.cutoff = HURT_CUTOFF,
	.default_expire = HURT_DEFAULT_EXPIRE,
	.exit_reason = HURT_EXIT_REASON,
};

/*
 * Module constructor/destructor.
 */

/* {{{ static int modinit() */

struct ev_entry *hurt_expire_ev = NULL;
struct ev_entry *hurt_check_ev = NULL;

static int
modinit(void)
{
	/* set-up hurt_state. */
	hurt_state.start_time = rb_current_time();

	/* add our event handlers. */
	hurt_expire_ev = rb_event_add("hurt_expire", hurt_expire_event, NULL, 60);
	hurt_check_ev = rb_event_add("hurt_check", hurt_check_event, NULL, 5);

	return 0;
}
/* }}} */

/* {{{ static void modfini() */
static void
modfini(void)
{
	rb_dlink_node	*ptr, *next_ptr;

	/* and delete our events. */
	rb_event_delete(hurt_expire_ev);
	rb_event_delete(hurt_check_ev);

	RB_DLINK_FOREACH_SAFE (ptr, next_ptr, hurt_state.hurt_clients.head)
	{
		rb_dlinkDestroy(ptr, &hurt_state.hurt_clients);
	}
}
/* }}} */

/*
 * Message handlers.
 */

/* {{{ static int mo_hurt()
 *
 * HURT [<expire>] <ip> <reason>
 * 
 * parv[1] - expire or ip
 * parv[2] - ip or reason
 * parv[3] - reason or NULL
 */
static int
mo_hurt(struct Client *client_p, struct Client *source_p,
		int parc, const char **parv)
{
	const char			*ip, *expire, *reason;
	int				expire_time;
	hurt_t				*hurt;
	struct Client			*target_p;

	if (!IsOperK(source_p)) {
		sendto_one(source_p, form_str(ERR_NOPRIVS), me.name,
				source_p->name, "kline");
		return 0;
	}

	if (parc == 3)
		expire = NULL, ip = parv[1], reason = parv[2];
	else
		expire = parv[1], ip = parv[2], reason = parv[3];

	if (!expire)
		expire_time = HURT_DEFAULT_EXPIRE;
	if (expire && (expire_time = valid_temp_time(expire)) < 1) {
		sendto_one_notice(source_p, ":Permanent HURTs are not supported");
		return 0;
	}
	if (EmptyString(reason)) {
		sendto_one_notice(source_p, ":Empty HURT reasons are bad for business");
		return 0;
	}

	/* Is this a client? */
	if (strchr(ip, '.') == NULL && strchr(ip, ':') == NULL)
	{
		target_p = find_named_person(ip);
		if (target_p == NULL)
		{
			sendto_one_numeric(source_p, ERR_NOSUCHNICK,
					   form_str(ERR_NOSUCHNICK), ip);
			return 0;
		}
		ip = target_p->orighost;
	}
	else
	{
		if (!strncmp(ip, "*@", 2))
			ip += 2;
		if (strchr(ip, '!') || strchr(ip, '@'))
		{
			sendto_one_notice(source_p, ":Invalid HURT mask [%s]",
					ip);
			return 0;
		}
	}

	if (hurt_find(ip) != NULL) {
		sendto_one(source_p, ":[%s] already HURT", ip);
		return 0;
	}

	/*
	 * okay, we've got this far, now it's time to add the the HURT locally
	 * and propagate it to other servers on the network.
	 */
	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			"%s added HURT on [%s] for %ld minutes with reason [%s]",
			get_oper_name(source_p), ip, (long) expire_time / 60, reason);

	hurt = hurt_new(expire_time, ip, reason);
	hurt_add(hurt);
	hurt_propagate(NULL, source_p, hurt);

	return 0;
}
/* }}} */

/* {{{ static int me_hurt()
 *
 * [ENCAP mask] HURT <target> <expire> <ip> <reason>
 *
 * parv[1] - expire
 * parv[2] - ip
 * parv[3] - reason
 */
static int
me_hurt(struct Client *client_p, struct Client *source_p,
		int parc, const char **parv)
{
	time_t				expire_time;
	hurt_t				*hurt;

	/*
	 * right... if we don't get enough arguments, or if we get any invalid
	 * arguments, just ignore this request - shit happens, and it's not worth
	 * dropping a server over.
	 */
	if (parc < 4 || !IsPerson(source_p))
		return 0;
	if ((expire_time = atoi(parv[1])) < 1)
		return 0;
	if (hurt_find(parv[2]) != NULL)
		return 0;
	if (EmptyString(parv[3]))
		return 0;

	sendto_realops_snomask(SNO_GENERAL, L_ALL,
			"%s added HURT on [%s] for %ld minutes with reason [%s]",
			get_oper_name(source_p), parv[2], (long) expire_time / 60, parv[3]);
	hurt = hurt_new(expire_time, parv[2], parv[3]);
	hurt_add(hurt);

	return 0;
}
/* }}} */

/* {{{ static int mo_heal()
 *
 * HURT <nick>|<ip>
 *
 * parv[1] - nick or ip
 */
static int
mo_heal(struct Client *client_p, struct Client *source_p,
		int parc, const char **parv)
{
	struct Client *target_p;

	if (!IsOperUnkline(source_p))
	{
		sendto_one(source_p, form_str(ERR_NOPRIVS),
				me.name, source_p->name, "unkline");
		return 0;
	}

	if (nick_is_valid(parv[1]))
	{
		target_p = find_named_person(parv[1]);
		if (target_p == NULL)
		{
			sendto_one_numeric(source_p, ERR_NOSUCHNICK,
					form_str(ERR_NOSUCHNICK), parv[1]);
			return 0;
		}
		if (MyConnect(target_p))
			heal_nick(source_p, target_p);
		else
			sendto_one(target_p, ":%s ENCAP %s HEAL %s",
					get_id(source_p, target_p),
					target_p->servptr->name,
					get_id(target_p, target_p));
	}
	else if (strchr(parv[1], '.'))
	{
		if (hurt_find_exact(parv[1]) == NULL)
		{
			sendto_one_notice(source_p, ":Mask [%s] is not HURT", parv[1]);
			return 0;
		}
		hurt_remove(parv[1]);
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s removed HURT on %s",
				get_oper_name(source_p), parv[1]);
		sendto_server(NULL, NULL, NOCAPS, NOCAPS, ":%s ENCAP * HEAL %s",
			source_p->name, parv[1]);
	}
	else
	{
		sendto_one(source_p, ":[%s] is not a valid IP address/nick", parv[1]);
		return 0;
	}

	return 0;
}
/* }}} */

static int
me_heal(struct Client *client_p, struct Client *source_p,
		int parc, const char **parv)
{
	struct Client *target_p;

	/* as noted in me_hurt(), if we don't get sufficient arguments...
	 * *poof*, it's dropped...
	 */
	if (parc < 2)
		return 0;

	if (nick_is_valid(parv[1]))
	{
		target_p = find_person(parv[1]);
		if (target_p != NULL && MyConnect(target_p))
			heal_nick(source_p, target_p);
	}
	else if (strchr(parv[1], '.'))	/* host or mask to remove ban for */
	{
		if (hurt_find_exact(parv[1]) == NULL)
			return 0;

		hurt_remove(parv[1]);
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s removed HURT on %s",
				get_oper_name(source_p), parv[1]);
	}
	else
		return 0;

	return 0;
}

/*
 * Event handlers.
 */

/* {{{ static void hurt_check_event() */
static void
hurt_check_event(void *arg)
{
	rb_dlink_node	*ptr, *next_ptr;
	struct Client	*client_p;

	RB_DLINK_FOREACH_SAFE (ptr, next_ptr, hurt_state.hurt_clients.head) {
		client_p = ptr->data;
		if (!EmptyString(client_p->user->suser))
		{
			rb_dlinkDestroy(ptr, &hurt_state.hurt_clients);
			sendto_one_notice(client_p, ":HURT restriction removed for this session");
			client_p->localClient->target_last = rb_current_time();		/* don't ask --nenolod */
		}
		else if (client_p->localClient->receiveM > hurt_state.cutoff)
			exit_client(NULL, client_p, &me, hurt_state.exit_reason);
	}
}
/* }}} */

/* {{{ static void hurt_expire_event() */
static void
hurt_expire_event(void *unused)
{
	rb_dlink_node	*ptr, *next_ptr;
	hurt_t		*hurt;

	RB_DLINK_FOREACH_SAFE (ptr, next_ptr, hurt_confs.head)
	{
		hurt = (hurt_t *) ptr->data;

		if (hurt->expire <= rb_current_time())
		{
			rb_dlinkFindDestroy(hurt, &hurt_confs);
			hurt_destroy(hurt);
		}
	}
}
/* }}} */

/*
 * Hook functions.
 */

/* {{{ static void client_exit_hook() */
static void
client_exit_hook(hook_data_client_exit *data)
{
	s_assert(data != NULL);
	s_assert(data->target != NULL);

	rb_dlinkFindDestroy(data->target, &hurt_state.hurt_clients);
}
/* }}} */

/* {{{ static void new_local_user_hook() */
static void
new_local_user_hook(struct Client *source_p)
{
	if (IsAnyDead(source_p) || !EmptyString(source_p->user->suser) ||
			IsExemptKline(source_p))
		return;

	if (hurt_find(source_p->sockhost) || hurt_find(source_p->orighost))
	{
		source_p->localClient->target_last = rb_current_time() + 600;		/* don't ask --nenolod */
		SetTGChange(source_p);
		rb_dlinkAddAlloc(source_p, &hurt_state.hurt_clients);
		sendto_one_notice(source_p, ":You are hurt. Please identify to services immediately, or use /stats p for assistance.");
	}	
}
/* }}} */

/* {{{ static void doing_stats_hook() */
static void
doing_stats_hook(hook_data_int *hdata)
{
	rb_dlink_node	*ptr;
	hurt_t		*hurt;
	struct Client	*source_p;

	s_assert(hdata);
	s_assert(hdata->client);

	source_p = hdata->client;
	if(hdata->arg2 != (int) 's')
		return;
	if((ConfigFileEntry.stats_k_oper_only == 2) && !IsOper(source_p))
		return;
	if ((ConfigFileEntry.stats_k_oper_only == 1) && !IsOper(source_p))
	{
		hurt = hurt_find(source_p->sockhost);
		if (hurt != NULL)
		{
			sendto_one_numeric(source_p, RPL_STATSKLINE,
					form_str(RPL_STATSKLINE), 's',
					"*", hurt->ip, hurt->reason, "", "");
			return;
		}

		hurt = hurt_find(source_p->orighost);
		if (hurt != NULL)
		{
			sendto_one_numeric(source_p, RPL_STATSKLINE,
					form_str(RPL_STATSKLINE), 's',
					"*", hurt->ip, hurt->reason, "", "");
			return;
		}
		return;
	}

	RB_DLINK_FOREACH(ptr, hurt_confs.head)
	{
		hurt = (hurt_t *) ptr->data;
		sendto_one_numeric(source_p, RPL_STATSKLINE,
				form_str(RPL_STATSKLINE), 's',
				"*", hurt->ip, hurt->reason, "", "");
	}
}
/* }}} */

/* {{{ static void hurt_propagate()
 *
 * client_p - specific server to propagate HURT to, or NULL to propagate to all
 *      servers.
 * source_p - source (oper who added the HURT)
 * hurt   - HURT to be propagated
 */
static void
hurt_propagate(struct Client *client_p, struct Client *source_p, hurt_t *hurt)
{
	if (client_p)
		sendto_one(client_p,
				":%s ENCAP %s HURT %ld %s :%s",
				source_p->name, client_p->name,
				(long)(hurt->expire - rb_current_time()),
				hurt->ip, hurt->reason);
	else
		sendto_server(&me, NULL, NOCAPS, NOCAPS,
				":%s ENCAP * HURT %ld %s :%s",
				source_p->name,
				(long)(hurt->expire - rb_current_time()),
				hurt->ip, hurt->reason);
}
/* }}} */

/* {{{ static hurt_t *hurt_new() */
static hurt_t *
hurt_new(time_t expire, const char *ip, const char *reason)
{
	hurt_t *hurt;

	hurt = rb_malloc(sizeof(hurt_t));

	hurt->ip = rb_strdup(ip);
	hurt->reason = rb_strdup(reason);
	hurt->expire = rb_current_time() + expire;

	return hurt;
}
/* }}} */

/* {{{ static void hurt_destroy() */
static void
hurt_destroy(void *hurt)
{
	hurt_t *h;

	if (!hurt)
		return;

	h = (hurt_t *) hurt;
	rb_free(h->ip);
	rb_free(h->reason);
	rb_free(h);
}
/* }}} */

static void
hurt_add(hurt_t *hurt)
{
	rb_dlinkAddAlloc(hurt, &hurt_confs);
}

static hurt_t *
hurt_find_exact(const char *ip)
{
	rb_dlink_node *ptr;
	hurt_t *hurt;

	RB_DLINK_FOREACH(ptr, hurt_confs.head)
	{
		hurt = (hurt_t *) ptr->data;

		if (!strcasecmp(ip, hurt->ip))
			return hurt;
	}

	return NULL;
}

static hurt_t *
hurt_find(const char *ip)
{
	rb_dlink_node *ptr;
	hurt_t *hurt;

	RB_DLINK_FOREACH(ptr, hurt_confs.head)
	{
		hurt = (hurt_t *) ptr->data;

		if (match(hurt->ip, ip))
			return hurt;
	}

	return NULL;
}

static void
hurt_remove(const char *ip)
{
	hurt_t *hurt = hurt_find_exact(ip);

	rb_dlinkFindDestroy(hurt, &hurt_confs);
	hurt_destroy(hurt);
}

/* {{{ static int heal_nick() */
static int
heal_nick(struct Client *source_p, struct Client *target_p)
{
	if (rb_dlinkFindDestroy(target_p, &hurt_state.hurt_clients))
	{
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s used HEAL on %s",
				get_oper_name(source_p), get_client_name(target_p, HIDE_IP));
		sendto_one_notice(target_p, ":HURT restriction temporarily removed by operator");
		sendto_one_notice(source_p, ":HURT restriction on %s temporarily removed", target_p->name);
		target_p->localClient->target_last = rb_current_time();		/* don't ask --nenolod */
		return 1;
	}
	else
	{
		sendto_one_notice(source_p, ":%s was not hurt", target_p->name);
		return 0;
	}
}
/* }}} */

/*
 * Anything else...
 */

/* {{{ static int nick_is_valid() */
static int
nick_is_valid(const char *nick)
{
	const char *s = nick;

	for (; *s != '\0'; s++) {
		if (!IsNickChar(*s))
			return 0;
	}

	return 1;
}
/* }}} */

/*
 * vim: ts=8 sw=8 noet fdm=marker tw=80
 */
