/*
 * Treat cmode +-R as +-q $~a.
 * -- jilles
 */

using namespace ircd;

static const char chm_quietunreg_compat_desc[] =
	"Adds an emulated channel mode +R which is converted into mode +q $~a";

static int _modinit(void);
static void _moddeinit(void);
static void chm_quietunreg(struct Client *source_p, struct Channel *chptr,
	int alevel, int parc, int *parn,
	const char **parv, int *errors, int dir, char c, long mode_type);

DECLARE_MODULE_AV2(chm_quietunreg_compat, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, chm_quietunreg_compat_desc);

static int
_modinit(void)
{
	chmode_table['R'].set_func = chm_quietunreg;
	chmode_table['R'].mode_type = 0;
	chmode_table['R'].mode_class = CHM_D;

	return 0;
}

static void
_moddeinit(void)
{
	chmode_table['R'].set_func = chm_nosuch;
	chmode_table['R'].mode_type = 0;
	chmode_table['R'].mode_class = ChmClass(0);
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
