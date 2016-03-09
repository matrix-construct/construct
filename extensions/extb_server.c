/*
 * Server name extban type: bans all users using a certain server
 * -- jilles
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "ircd.h"

static const char extb_desc[] = "Server ($s) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_server(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV2(extb_server, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	extban_table['s'] = eb_server;

	return 0;
}

static void
_moddeinit(void)
{
	extban_table['s'] = NULL;
}

static int eb_server(const char *data, struct Client *client_p,
		struct Channel *chptr, long mode_type)
{

	(void)chptr;
	/* This type is not safe for exceptions */
	if (mode_type == CHFL_EXCEPTION || mode_type == CHFL_INVEX)
		return EXTBAN_INVALID;
	if (data == NULL)
		return EXTBAN_INVALID;
	return match(data, me.name) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
}
