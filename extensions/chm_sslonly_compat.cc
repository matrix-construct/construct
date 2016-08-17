/*
 * Treat cmode +-S as +-b $~z.
 */

using namespace ircd::chan::mode;
using namespace ircd;

static const char chm_sslonly_compat_desc[] =
	"Adds an emulated channel mode +S which is converted into mode +b $~z";

static int _modinit(void);
static void _moddeinit(void);
static void chm_sslonly(struct Client *source_p, struct Channel *chptr,
	int alevel, int parc, int *parn,
	const char **parv, int *errors, int dir, char c, type type);

DECLARE_MODULE_AV2(chm_sslonly_compat, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, chm_sslonly_compat_desc);

static int
_modinit(void)
{
	table['S'].type = type(0);
	table['S'].set_func = chm_sslonly;
	table['S'].category = category::D;

	return 0;
}

static void
_moddeinit(void)
{
	table['S'].type = type(0);
	table['S'].category = category(0);
	table['S'].set_func = functor::nosuch;
}

static void
chm_sslonly(struct Client *source_p, struct Channel *chptr,
	int alevel, int parc, int *parn,
	const char **parv, int *errors, int dir, char c, type type)
{
	int newparn = 0;
	const char *newparv[] = { "$~z" };

	if (MyClient(source_p))
		functor::ban(source_p, chptr, alevel, 1, &newparn, newparv, errors, dir, 'b', BAN);
	else
		functor::nosuch(source_p, chptr, alevel, parc, parn, parv, errors, dir, c, type);
}
