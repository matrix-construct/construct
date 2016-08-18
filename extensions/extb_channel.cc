/*
 * Channel extban type: matches users who are in a certain public channel
 * -- jilles
 */

namespace mode = ircd::chan::mode;
namespace ext = mode::ext;
using namespace ircd;

static const char extb_desc[] = "Channel ($c) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_channel(const char *data, struct Client *client_p, chan::chan *chptr, mode::type type);

DECLARE_MODULE_AV2(extb_channel, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	ext::table['c'] = eb_channel;

	return 0;
}

static void
_moddeinit(void)
{
	ext::table['c'] = NULL;
}

static int eb_channel(const char *data, struct Client *client_p,
		chan::chan *chptr, mode::type type)
{
	using namespace ext;

	chan::chan *chptr2;

	(void)chptr;

	if (data == NULL)
		return INVALID;

	chptr2 = find_channel(data);
	if (chptr2 == NULL)
		return INVALID;
	/* require consistent target */
	if (chptr->name[0] == '#' && data[0] == '&')
		return INVALID;
	/* privacy! don't allow +s/+p channels to influence another channel */
	if (!pub(chptr2) && chptr2 != chptr)
		return INVALID;

	return is_member(chptr2, client_p)? MATCH : NOMATCH;
}
