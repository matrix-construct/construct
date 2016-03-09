/*
 * Realname extban type: bans all users with matching gecos
 * -- jilles
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "ircd.h"

static const char extb_desc[] = "Realname/GECOS ($r) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_realname(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV2(extb_realname, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

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
