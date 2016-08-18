/*
 * Treat cmode +-R as +-q $~a.
 * -- jilles
 */

namespace mode = ircd::chan::mode;
using namespace mode;
using namespace ircd;

static const char chm_quietunreg_compat_desc[] =
	"Adds an emulated channel mode +R which is converted into mode +q $~a";

static int _modinit(void);
static void _moddeinit(void);
static void chm_quietunreg(struct Client *source_p, chan::chan *chptr,
	int alevel, int parc, int *parn,
	const char **parv, int *errors, int dir, char c, type type);

DECLARE_MODULE_AV2(chm_quietunreg_compat, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, chm_quietunreg_compat_desc);

static int
_modinit(void)
{
	table['R'].type = type(0);
	table['R'].category = category::D;
	table['R'].set_func = chm_quietunreg;

	return 0;
}

static void
_moddeinit(void)
{
	table['R'].type = type(0);
	table['R'].category = category(0);
	table['R'].set_func = functor::nosuch;
}

static void
chm_quietunreg(struct Client *source_p, chan::chan *chptr,
	int alevel, int parc, int *parn,
	const char **parv, int *errors, int dir, char c, type type)
{
	int newparn = 0;
	const char *newparv[] = { "$~a" };

	if (MyClient(source_p))
		functor::ban(source_p, chptr, alevel, 1, &newparn, newparv, errors, dir, 'q', QUIET);
	else
		functor::nosuch(source_p, chptr, alevel, parc, parn, parv, errors, dir, c, type);
}
