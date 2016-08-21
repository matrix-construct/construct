/*
 * Account extban type: bans all users with any/matching account
 * -- jilles
 */

namespace mode = ircd::chan::mode;
namespace ext = mode::ext;
using namespace ircd;

static const char extb_desc[] = "Account ($a) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_account(const char *data, struct Client *client_p, chan::chan *chptr, mode::type);

DECLARE_MODULE_AV2(extb_account, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	ext::table['a'] = eb_account;

	return 0;
}

static void
_moddeinit(void)
{
	ext::table['a'] = NULL;
}

static int eb_account(const char *data, struct Client *client_p,
		chan::chan *chptr, mode::type type)
{
	using namespace ext;

	(void)chptr;
	/* $a alone matches any logged in user */
	if (data == NULL)
		return client_p->user->suser.empty()? NOMATCH : MATCH;
	/* $a:MASK matches users logged in under matching account */
	return match(data, client_p->user->suser) ? MATCH : NOMATCH;
}
