/*
 * Treat cmode +-O as +-iI $o.
 */

using namespace ircd::chan::mode;
using namespace ircd;

static const char chm_operonly_compat[] =
	"Adds an emulated channel mode +O which is converted into mode +i and +I $o";

static int _modinit(void);
static void _moddeinit(void);
static void chm_operonly(client::client *source_p, chan::chan *chptr,
	int alevel, int parc, int *parn,
	const char **parv, int *errors, int dir, char c, type type);

DECLARE_MODULE_AV2(chm_operonly_compat, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, chm_operonly_compat);

static int
_modinit(void)
{
	table['O'].type = type(0);
	table['O'].category = category::D;
	table['O'].set_func = chm_operonly;

	return 0;
}

static void
_moddeinit(void)
{
	table['O'].type = type(0);
	table['O'].category = category::D;
	table['O'].set_func = functor::nosuch;
}

static void
chm_operonly(client::client *source_p, chan::chan *chptr,
	int alevel, int parc, int *parn,
	const char **parv, int *errors, int dir, char c, type type)
{
	int newparn = 0;
	const char *newparv[] = { "$o" };

	if (MyClient(source_p))
	{
		functor::simple(source_p, chptr, alevel, parc, parn, parv, errors, dir, 'i', INVITEONLY);
		functor::ban(source_p, chptr, alevel, 1, &newparn, newparv, errors, dir, 'I', INVEX);
	} else
		functor::nosuch(source_p, chptr, alevel, parc, parn, parv, errors, dir, c, type);
}
