/* SSL extban type: matches ssl users */

namespace chan = ircd::chan;
namespace mode = chan::mode;
namespace ext = mode::ext;
using namespace ircd;

static const char extb_desc[] = "SSL/TLS ($z) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_ssl(const char *data, struct Client *client_p, struct Channel *chptr, mode::type);

DECLARE_MODULE_AV2(extb_ssl, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	ext::table['z'] = eb_ssl;

	return 0;
}

static void
_moddeinit(void)
{
	ext::table['z'] = NULL;
}

static int eb_ssl(const char *data, struct Client *client_p,
		struct Channel *chptr, mode::type type)
{
	using namespace ext;

	(void)chptr;
	if (data != NULL)
		return INVALID;

	return IsSSLClient(client_p) ? MATCH : NOMATCH;
}
