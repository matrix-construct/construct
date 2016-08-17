/*
 * Server name extban type: bans all users using a certain server
 * -- jilles
 */

namespace chan = ircd::chan;
namespace mode = chan::mode;
namespace ext = mode::ext;
using namespace ircd;

static const char extb_desc[] = "Server ($s) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_server(const char *data, struct Client *client_p, struct Channel *chptr, mode::type);

DECLARE_MODULE_AV2(extb_server, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	ext::table['s'] = eb_server;

	return 0;
}

static void
_moddeinit(void)
{
	ext::table['s'] = NULL;
}

static int eb_server(const char *data, struct Client *client_p,
		struct Channel *chptr, mode::type type)
{
	using namespace ext;

	(void)chptr;
	/* This type is not safe for exceptions */
	if (type == CHFL_EXCEPTION || type == CHFL_INVEX)
		return INVALID;
	if (data == NULL)
		return INVALID;
	return match(data, me.name) ? MATCH : NOMATCH;
}
