/*
 * Oper extban type: matches opers
 * -- jilles
 */

#include <ircd/stdinc.h>
#include <ircd/modules.h>
#include <ircd/client.h>
#include <ircd/privilege.h>
#include <ircd/s_newconf.h>
#include <ircd/ircd.h>

static const char extb_desc[] = "Oper ($o) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_oper(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV2(extb_oper, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

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

	if (data != NULL)
	{
		struct PrivilegeSet *set = privilegeset_get(data);
		if (set != NULL && client_p->localClient->privset == set)
			return EXTBAN_MATCH;

		/* $o:admin or whatever */
		return HasPrivilege(client_p, data) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
	}

	return IsOper(client_p) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
}
