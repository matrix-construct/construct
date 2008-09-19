/* $Id: ip_cloaking.c 3526 2007-07-06 07:56:14Z nenolod $ */

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

/* if you're modifying this module, you'll probably to change this */
#define KEY 0x13748cfa

static int
_modinit(void)
{
	/* add the usermode to the available slot */
	user_modes['h'] = find_umode_slot();
	construct_umodebuf();

	return 0;
}

static void
_moddeinit(void)
{
	/* disable the umode and remove it from the available list */
	user_modes['h'] = 0;
	construct_umodebuf();
}

static void check_umode_change(void *data);
static void check_new_user(void *data);
mapi_hfn_list_av1 ip_cloaking_hfnlist[] = {
	{ "umode_changed", (hookfn) check_umode_change },
	{ "new_local_user", (hookfn) check_new_user },
	{ NULL, NULL }
};

DECLARE_MODULE_AV1(ip_cloaking, _modinit, _moddeinit, NULL, NULL,
			ip_cloaking_hfnlist, "$Revision: 3526 $");

static void
distribute_hostchange(struct Client *client)
{
	if (irccmp(client->host, client->orighost))
		sendto_one_numeric(client, RPL_HOSTHIDDEN, "%s :is now your hidden host",
			client->host);
	else
		sendto_one_numeric(client, RPL_HOSTHIDDEN, "%s :hostname reset",
			client->host);

	sendto_server(NULL, NULL,
		CAP_EUID | CAP_TS6, NOCAPS, ":%s CHGHOST %s :%s",
		use_id(&me), use_id(client), client->host);
	sendto_server(NULL, NULL,
		CAP_TS6, CAP_EUID, ":%s ENCAP * CHGHOST %s :%s",
		use_id(&me), use_id(client), client->host);
	if (irccmp(client->host, client->orighost))
		SetDynSpoof(client);
	else
		ClearDynSpoof(client);
}

static void
do_host_cloak_ip(const char *inbuf, char *outbuf)
{
	/* None of the characters in this table can be valid in an IP */
	char chartable[] = "ghijklmnopqrstuvwxyz";
	char *tptr;
	uint32_t accum = fnv_hash((const unsigned char*) inbuf, 32);
	int sepcount = 0;
	int totalcount = 0;
	int ipv6 = 0;

	rb_strlcpy(outbuf, inbuf, HOSTLEN + 1);

	if (strchr(outbuf, ':'))
	{
		ipv6 = 1;

		/* Damn you IPv6... 
		 * We count the number of colons so we can calculate how much
		 * of the host to cloak. This is because some hostmasks may not
		 * have as many octets as we'd like.
		 *
		 * We have to do this ahead of time because doing this during
		 * the actual cloaking would get ugly
		 */
		for (tptr = outbuf; *tptr != '\0'; tptr++)
			if (*tptr == ':')
				totalcount++;
	}
	else if (!strchr(outbuf, '.'))
		return;

	for (tptr = outbuf; *tptr != '\0'; tptr++) 
	{
		if (*tptr == ':' || *tptr == '.')
		{
			sepcount++;
			continue;
		}

		if (ipv6 && sepcount < totalcount / 2)
			continue;

		if (!ipv6 && sepcount < 2)
			continue;

		*tptr = chartable[(*tptr + accum) % 20];
		accum = (accum << 1) | (accum >> 31);
	}
}

static void
do_host_cloak_host(const char *inbuf, char *outbuf)
{
	char b26_alphabet[] = "abcdefghijklmnopqrstuvwxyz";
	char *tptr;
	uint32_t accum = fnv_hash((const unsigned char*) inbuf, 32);

	rb_strlcpy(outbuf, inbuf, HOSTLEN + 1);

	/* pass 1: scramble first section of hostname using base26 
	 * alphabet toasted against the FNV hash of the string.
	 *
	 * numbers are not changed at this time, only letters.
	 */
	for (tptr = outbuf; *tptr != '\0'; tptr++)
	{
		if (*tptr == '.')
			break;

		if (isdigit(*tptr) || *tptr == '-')
			continue;

		*tptr = b26_alphabet[(*tptr + accum) % 26];

		/* Rotate one bit to avoid all digits being turned odd or even */
		accum = (accum << 1) | (accum >> 31);
	}

	/* pass 2: scramble each number in the address */
	for (tptr = outbuf; *tptr != '\0'; tptr++)
	{
		if (isdigit(*tptr))
			*tptr = '0' + (*tptr + accum) % 10;

		accum = (accum << 1) | (accum >> 31);
	}	
}

static void
check_umode_change(void *vdata)
{
	hook_data_umode_changed *data = (hook_data_umode_changed *)vdata;
	struct Client *source_p = data->client;

	if (!MyClient(source_p))
		return;

	/* didn't change +h umode, we don't need to do anything */
	if (!((data->oldumodes ^ source_p->umodes) & user_modes['h']))
		return;

	if (source_p->umodes & user_modes['h'])
	{
		if (IsIPSpoof(source_p) || source_p->localClient->mangledhost == NULL || (IsDynSpoof(source_p) && strcmp(source_p->host, source_p->localClient->mangledhost)))
		{
			source_p->umodes &= ~user_modes['h'];
			return;
		}
		if (strcmp(source_p->host, source_p->localClient->mangledhost))
		{
			rb_strlcpy(source_p->host, source_p->localClient->mangledhost, HOSTLEN + 1);
			distribute_hostchange(source_p);
		}
		else /* not really nice, but we need to send this numeric here */
			sendto_one_numeric(source_p, RPL_HOSTHIDDEN, "%s :is now your hidden host",
				source_p->host);
	}
	else if (!(source_p->umodes & user_modes['h']))
	{
		if (source_p->localClient->mangledhost != NULL &&
				!strcmp(source_p->host, source_p->localClient->mangledhost))
		{
			rb_strlcpy(source_p->host, source_p->orighost, HOSTLEN + 1);
			distribute_hostchange(source_p);
		}
	}
}

static void
check_new_user(void *vdata)
{
	struct Client *source_p = (void *)vdata;

	if (IsIPSpoof(source_p))
	{
		source_p->umodes &= ~user_modes['h'];
		return;
	}
	source_p->localClient->mangledhost = rb_malloc(HOSTLEN + 1);
	if (!irccmp(source_p->orighost, source_p->sockhost))
		do_host_cloak_ip(source_p->orighost, source_p->localClient->mangledhost);
	else
		do_host_cloak_host(source_p->orighost, source_p->localClient->mangledhost);
	if (IsDynSpoof(source_p))
		source_p->umodes &= ~user_modes['h'];
	if (source_p->umodes & user_modes['h'])
	{
		rb_strlcpy(source_p->host, source_p->localClient->mangledhost, sizeof(source_p->host));
		if (irccmp(source_p->host, source_p->orighost))
			SetDynSpoof(source_p);
	}
}
