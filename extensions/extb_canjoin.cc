/*
 * Canjoin extban type: matches users who are or are not banned from a
 * specified channel.
 *    -- nenolod/jilles
 */

namespace chan = ircd::chan;
namespace mode = chan::mode;
namespace ext = mode::ext;
using namespace ircd;

static const char extb_desc[] = "Can join ($j) extban type - matches users who are or are not banned from a specified channel";

static int _modinit(void);
static void _moddeinit(void);
static int eb_canjoin(const char *data, struct Client *client_p, struct Channel *chptr, mode::type type);

DECLARE_MODULE_AV2(extb_canjoin, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	ext::table['j'] = eb_canjoin;

	return 0;
}

static void
_moddeinit(void)
{
	ext::table['j'] = NULL;
}

static int eb_canjoin(const char *data, struct Client *client_p,
		struct Channel *chptr, mode::type type)
{
	using namespace ext;

	struct Channel *chptr2;
	int ret;
	static int recurse = 0;

	/* don't process a $j in a $j'ed list */
	if (recurse)
		return INVALID;
	if (data == NULL)
		return INVALID;
	chptr2 = find_channel(data);
	/* must exist, and no point doing this with the same channel */
	if (chptr2 == NULL || chptr2 == chptr)
		return INVALID;
	/* require consistent target */
	if (chptr->chname[0] == '#' && data[0] == '&')
		return INVALID;
	/* this allows getting some information about ban exceptions
	 * but +s/+p doesn't seem the right criterion */
#if 0
	/* privacy! don't allow +s/+p channels to influence another channel */
	if (!PubChannel(chptr2))
		return INVALID;
#endif
	recurse = 1;
	ret = is_banned(chptr2, client_p, NULL, NULL, NULL, NULL) == CHFL_BAN ? MATCH : NOMATCH;
	recurse = 0;
	return ret;
}
