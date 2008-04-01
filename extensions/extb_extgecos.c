/*
 * Extended extban type: bans all users with matching nick!user@host#gecos.
 * Requested by Lockwood.
 *  - nenolod
 *
 * $Id: extb_realname.c 1339 2006-05-17 00:45:40Z nenolod $
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "ircd.h"

static int _modinit(void);
static void _moddeinit(void);
static int eb_extended(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV1(extb_extended, _modinit, _moddeinit, NULL, NULL, NULL, "$Revision: 1339 $");

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

	rb_snprintf(buf, BUFSIZE, "%s!%s@%s#%s",
		client_p->name, client_p->username, client_p->host, client_p->info);

	ret = match(data, buf) ? EXTBAN_MATCH : EXTBAN_NOMATCH;

	if (ret == EXTBAN_NOMATCH && IsDynSpoof(client_p))
	{
		rb_snprintf(buf, BUFSIZE, "%s!%s@%s#%s",
			client_p->name, client_p->username, client_p->orighost, client_p->info);

		ret = match(data, buf) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
	}

	return ret;
}
