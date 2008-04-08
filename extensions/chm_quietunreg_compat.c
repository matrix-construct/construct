/*
 * Treat cmode +-R as +-q $~a.
 * -- jilles
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "hook.h"
#include "ircd.h"
#include "chmode.h"

static int _modinit(void);
static void _moddeinit(void);
static void chm_quietunreg(struct Client *source_p, struct Channel *chptr,
	int alevel, int parc, int *parn,
	const char **parv, int *errors, int dir, char c, long mode_type);

DECLARE_MODULE_AV1(chm_quietunreg_compat, _modinit, _moddeinit, NULL, NULL, NULL, "$Revision$");

static int
_modinit(void)
{
	chmode_table['R'].set_func = chm_quietunreg;
	chmode_table['R'].mode_type = 0;

	return 0;
}

static void
_moddeinit(void)
{
	chmode_table['R'].set_func = chm_nosuch;
	chmode_table['R'].mode_type = 0;
}

static void
chm_quietunreg(struct Client *source_p, struct Channel *chptr,
	int alevel, int parc, int *parn,
	const char **parv, int *errors, int dir, char c, long mode_type)
{
	int newparn = 0;
	const char *newparv[] = { "$~a" };

	if (MyClient(source_p))
		chm_ban(source_p, chptr, alevel, 1, &newparn, newparv,
				errors, dir, 'q', CHFL_QUIET);
	else
		chm_nosuch(source_p, chptr, alevel, parc, parn, parv,
				errors, dir, c, mode_type);
}
