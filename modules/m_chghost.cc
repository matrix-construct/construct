/*
 * Copyright (c) 2005 William Pitcock <nenolod -at- nenolod.net>
 * and Jilles Tjoelker <jilles -at- stack.nl>
 * All rights reserved.
 *
 * Redistribution in both source and binary forms are permitted
 * provided that the above copyright notice remains unchanged.
 *
 * m_chghost.c: A module for handling spoofing dynamically.
 */

using namespace ircd;

static const char chghost_desc[] = "Provides commands used to change and retrieve client hostnames";

static void me_realhost(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void ms_chghost(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void me_chghost(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void mo_chghost(struct MsgBuf *, client::client &, client::client &, int, const char **);

struct Message realhost_msgtab = {
	"REALHOST", 0, 0, 0, 0,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {me_realhost, 2}, mg_ignore}
};

struct Message chghost_msgtab = {
	"CHGHOST", 0, 0, 0, 0,
	{mg_ignore, mg_not_oper, {ms_chghost, 3}, {ms_chghost, 3}, {me_chghost, 3}, {mo_chghost, 3}}
};

mapi_clist_av1 chghost_clist[] = { &chghost_msgtab, &realhost_msgtab, NULL };

DECLARE_MODULE_AV2(chghost, NULL, NULL, chghost_clist, NULL, NULL, NULL, NULL, chghost_desc);

/* clean_host()
 *
 * input	- host to check
 * output	- false if erroneous, else true
 * side effects -
 */
static bool
clean_host(const char *host)
{
	int len = 0;
	const char *last_slash = 0;

	if (*host == '\0' || *host == ':')
		return false;

	for(; *host; host++)
	{
		len++;

		if(!rfc1459::is_host(*host))
			return false;
		if(*host == '/')
			last_slash = host;
	}

	if(len > HOSTLEN)
		return false;

	if(last_slash && rfc1459::is_digit(last_slash[1]))
		return false;

	return true;
}

/*
 * me_realhost
 * parv[1] = real host
 *
 * Yes this contains a little race condition if someone does a whois
 * in between the UID and REALHOST and use_whois_actually is enabled.
 * I don't think that's a big problem as the whole thing is a
 * race condition.
 */
static void
me_realhost(struct MsgBuf *msgbuf_p, client::client &client, client::client &source,
	int parc, const char *parv[])
{
	if (!IsPerson(&source))
		return;

	del_from_hostname_hash(source.orighost, &source);
	rb_strlcpy(source.orighost, parv[1], sizeof source.orighost);
	if (irccmp(source.host, source.orighost))
		SetDynSpoof(&source);
	else
		ClearDynSpoof(&source);
	add_to_hostname_hash(source.orighost, &source);
}

static bool
do_chghost(client::client &source, client::client *target_p,
		const char *newhost, int is_encap)
{
	if (!clean_host(newhost))
	{
		sendto_realops_snomask(SNO_GENERAL, is_encap ? L_ALL : L_NETWIDE, "%s attempted to change hostname for %s to %s (invalid)",
				IsServer(&source) ? source.name : get_oper_name(&source),
				target_p->name, newhost);
		/* sending this remotely may disclose important
		 * routing information -- jilles */
		if (is_encap ? MyClient(target_p) : !ConfigServerHide.flatten_links)
			sendto_one_notice(target_p, ":*** Notice -- %s attempted to change your hostname to %s (invalid)",
					source.name, newhost);
		return false;
	}
	change_nick_user_host(target_p, target_p->name, target_p->username, newhost, 0, "Changing host");
	if (irccmp(target_p->host, target_p->orighost))
	{
		SetDynSpoof(target_p);
		if (MyClient(target_p))
			sendto_one_numeric(target_p, RPL_HOSTHIDDEN, "%s :is now your hidden host (set by %s)", target_p->host, source.name);
	}
	else
	{
		ClearDynSpoof(target_p);
		if (MyClient(target_p))
			sendto_one_numeric(target_p, RPL_HOSTHIDDEN, "%s :hostname reset by %s", target_p->host, source.name);
	}
	if (MyClient(&source))
		sendto_one_notice(&source, ":Changed hostname for %s to %s", target_p->name, target_p->host);
	if (!IsServer(&source) && !IsService(&source))
		sendto_realops_snomask(SNO_GENERAL, L_ALL, "%s changed hostname for %s to %s", get_oper_name(&source), target_p->name, target_p->host);
	return true;
}

/*
 * ms_chghost
 * parv[1] = target
 * parv[2] = host
 */
static void
ms_chghost(struct MsgBuf *msgbuf_p, client::client &client, client::client &source,
	int parc, const char *parv[])
{
	client::client *target_p;

	if (!(target_p = client::find_person(parv[1])))
		return;

	if (do_chghost(source, target_p, parv[2], 0))
	{
		sendto_server(&client, NULL,
			CAP_EUID | CAP_TS6, NOCAPS, ":%s CHGHOST %s %s",
			use_id(&source), use_id(target_p), parv[2]);
		sendto_server(&client, NULL,
			CAP_TS6, CAP_EUID, ":%s ENCAP * CHGHOST %s :%s",
			use_id(&source), use_id(target_p), parv[2]);
	}

	return;
}

/*
 * me_chghost
 * parv[1] = target
 * parv[2] = host
 */
static void
me_chghost(struct MsgBuf *msgbuf_p, client::client &client, client::client &source,
	int parc, const char *parv[])
{
	client::client *target_p;

	if (!(target_p = client::find_person(parv[1])))
		return;

	do_chghost(source, target_p, parv[2], 1);
}

/*
 * mo_chghost
 * parv[1] = target
 * parv[2] = host
 */
/* Disable this because of the abuse potential -- jilles
 * No, make it toggleable via ./configure. --nenolod
 */
static void
mo_chghost(struct MsgBuf *msgbuf_p, client::client &client, client::client &source,
	int parc, const char *parv[])
{
#ifdef ENABLE_OPER_CHGHOST
	client::client *target_p;

	if(!IsOperAdmin(&source))
	{
		sendto_one(&source, form_str(ERR_NOPRIVS),
			   me.name, source.name, "admin");
		return;
	}

	if (!(target_p = client::find_named_person(parv[1])))
	{
		sendto_one_numeric(&source, ERR_NOSUCHNICK,
				form_str(ERR_NOSUCHNICK), parv[1]);
		return;
	}

	if (!clean_host(parv[2]))
	{
		sendto_one_notice(&source, ":Hostname %s is invalid", parv[2]);
		return;
	}

	do_chghost(&source, target_p, parv[2], 0);

	sendto_server(NULL, NULL,
		CAP_EUID | CAP_TS6, NOCAPS, ":%s CHGHOST %s %s",
		use_id(&source), use_id(target_p), parv[2]);
	sendto_server(NULL, NULL,
		CAP_TS6, CAP_EUID, ":%s ENCAP * CHGHOST %s :%s",
		use_id(&source), use_id(target_p), parv[2]);
#else
	sendto_one_numeric(&source, ERR_DISABLED, form_str(ERR_DISABLED),
			"CHGHOST");
#endif
}

