/*
 * Oper extban type: matches opers
 * -- jilles
 *
 * $Id: extb_oper.c 1299 2006-05-11 15:43:03Z jilles $
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "ircd.h"

static int _modinit(void);
static void _moddeinit(void);
static int eb_oper(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV1(extb_oper, _modinit, _moddeinit, NULL, NULL, NULL, "$Revision: 1299 $");

static int
_modinit(void)
{
	extban_table['o'] = eb_oper;

	return 0;
}

static void
_moddeinit(void)
{
	extban_table['o'] = NULL;
}

static int eb_oper(const char *data, struct Client *client_p,
		struct Channel *chptr, long mode_type)
{

	(void)chptr;
	(void)mode_type;
	/* perhaps use data somehow? (opernick/flags?) */
	/* so deny any bans with data for now */
	if (data != NULL)
		return EXTBAN_INVALID;
	return IsOper(client_p) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
}
