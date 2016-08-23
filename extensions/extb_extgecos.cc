/*
 * Extended extban type: bans all users with matching nick!user@host#gecos.
 * Requested by Lockwood.
 *  - nenolod
 */

namespace mode = ircd::chan::mode;
namespace ext = mode::ext;
using namespace ircd;

static const char extb_desc[] = "Extended mask ($x) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_extended(const char *data, client::client *client_p, chan::chan *chptr, mode::type);

DECLARE_MODULE_AV2(extb_extended, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	ext::table['x'] = eb_extended;

	return 0;
}

static void
_moddeinit(void)
{
	ext::table['x'] = NULL;
}

static int eb_extended(const char *data, client::client *client_p,
		chan::chan *chptr, mode::type type)
{
	using namespace ext;

	char buf[BUFSIZE];
	int ret;

	(void)chptr;

	if (data == NULL)
		return INVALID;

	snprintf(buf, BUFSIZE, "%s!%s@%s#%s",
		client_p->name, client_p->username, client_p->host, client_p->info);

	ret = match(data, buf) ? MATCH : NOMATCH;

	if (ret == NOMATCH && is_dyn_spoof(*client_p))
	{
		snprintf(buf, BUFSIZE, "%s!%s@%s#%s",
			client_p->name, client_p->username, client_p->orighost, client_p->info);

		ret = match(data, buf) ? MATCH : NOMATCH;
	}

	return ret;
}
