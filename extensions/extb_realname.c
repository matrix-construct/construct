/*
 * Realname extban type: bans all users with matching gecos
 * -- jilles
 *
 * $Id: extb_realname.c 1299 2006-05-11 15:43:03Z jilles $
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "ircd.h"

static int _modinit(void);
static void _moddeinit(void);
static int eb_realname(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV1(extb_realname, _modinit, _moddeinit, NULL, NULL, NULL, "$Revision: 1299 $");

static int
_modinit(void)
{
	extban_table['r'] = eb_realname;

	return 0;
}

static void
_moddeinit(void)
{
	extban_table['r'] = NULL;
}

static int eb_realname(const char *data, struct Client *client_p,
		struct Channel *chptr, long mode_type)
{

	(void)chptr;
	/* This type is not safe for exceptions */
	if (mode_type == CHFL_EXCEPTION || mode_type == CHFL_INVEX)
		return EXTBAN_INVALID;
	if (data == NULL)
		return EXTBAN_INVALID;
	return match(data, client_p->info) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
}
