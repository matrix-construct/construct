/*
 * Extended extban type: bans all users with matching nick!user@host#gecos.
 * Requested by Lockwood.
 *  - nenolod
 */

#include <ircd/stdinc.h>
#include <ircd/modules.h>
#include <ircd/client.h>
#include <ircd/ircd.h>

static const char extb_desc[] = "Extended mask ($x) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_extended(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV2(extb_extended, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	extban_table['x'] = eb_extended;

	return 0;
}

static void
_moddeinit(void)
{
	extban_table['x'] = NULL;
}

static int eb_extended(const char *data, struct Client *client_p,
		struct Channel *chptr, long mode_type)
{
	char buf[BUFSIZE];
	int ret;

	(void)chptr;

	if (data == NULL)
		return EXTBAN_INVALID;

	snprintf(buf, BUFSIZE, "%s!%s@%s#%s",
		client_p->name, client_p->username, client_p->host, client_p->info);

	ret = match(data, buf) ? EXTBAN_MATCH : EXTBAN_NOMATCH;

	if (ret == EXTBAN_NOMATCH && IsDynSpoof(client_p))
	{
		snprintf(buf, BUFSIZE, "%s!%s@%s#%s",
			client_p->name, client_p->username, client_p->orighost, client_p->info);

		ret = match(data, buf) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
	}

	return ret;
}
