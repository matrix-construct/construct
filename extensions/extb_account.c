/*
 * Account extban type: bans all users with any/matching account
 * -- jilles
 *
 * $Id: extb_account.c 1299 2006-05-11 15:43:03Z jilles $
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "ircd.h"

static int _modinit(void);
static void _moddeinit(void);
static int eb_account(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV1(extb_account, _modinit, _moddeinit, NULL, NULL, NULL, "$Revision: 1299 $");

static int
_modinit(void)
{
	extban_table['a'] = eb_account;

	return 0;
}

static void
_moddeinit(void)
{
	extban_table['a'] = NULL;
}

static int eb_account(const char *data, struct Client *client_p,
		struct Channel *chptr, long mode_type)
{

	(void)chptr;
	/* $a alone matches any logged in user */
	if (data == NULL)
		return EmptyString(client_p->user->suser) ? EXTBAN_NOMATCH : EXTBAN_MATCH;
	/* $a:MASK matches users logged in under matching account */
	return match(data, client_p->user->suser) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
}
