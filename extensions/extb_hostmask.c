/*
 * Hostmask extban type: bans all users matching a given hostmask, used for stacked extbans
 * -- kaniini
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "ircd.h"

static const char extb_desc[] = "Hostmask ($m) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_hostmask(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV2(extb_hostmask, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	extban_table['m'] = eb_hostmask;
	return 0;
}

static void
_moddeinit(void)
{
	extban_table['m'] = NULL;
}

static int
eb_hostmask(const char *banstr, struct Client *client_p, struct Channel *chptr, long mode_type)
{
	char src_host[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_iphost[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_althost[NICKLEN + USERLEN + HOSTLEN + 6];
	char src_ip4host[NICKLEN + USERLEN + HOSTLEN + 6];
	struct sockaddr_in ip4;
	char *s = src_host, *s2 = src_iphost, *s3 = NULL, *s4 = NULL;

	sprintf(src_host, "%s!%s@%s", client_p->name, client_p->username, client_p->host);
	sprintf(src_iphost, "%s!%s@%s", client_p->name, client_p->username, client_p->sockhost);

	/* handle hostmangling if necessary */
	if (client_p->localClient->mangledhost != NULL)
	{
		if (!strcmp(client_p->host, client_p->localClient->mangledhost))
			sprintf(src_althost, "%s!%s@%s", client_p->name, client_p->username, client_p->orighost);
		else if (!IsDynSpoof(client_p))
			sprintf(src_althost, "%s!%s@%s", client_p->name, client_p->username, client_p->localClient->mangledhost);

		s3 = src_althost;
	}

#ifdef RB_IPV6
	/* handle Teredo if necessary */
	if (GET_SS_FAMILY(&client_p->localClient->ip) == AF_INET6 && rb_ipv4_from_ipv6((const struct sockaddr_in6 *) &client_p->localClient->ip, &ip4))
	{
		sprintf(src_ip4host, "%s!%s@", client_p->name, client_p->username);
		s4 = src_ip4host + strlen(src_ip4host);
		rb_inet_ntop_sock((struct sockaddr *)&ip4,
				s4, src_ip4host + sizeof src_ip4host - s4);
		s4 = src_ip4host;
	}
#endif

	return match(banstr, s) || match(banstr, s2) || (s3 != NULL && match(banstr, s3)) || (s4 != NULL && match(banstr, s4)) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
}
