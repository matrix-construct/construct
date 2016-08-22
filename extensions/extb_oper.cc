/*
 * Oper extban type: matches opers
 * -- jilles
 */

namespace mode = ircd::chan::mode;
namespace ext = mode::ext;
using namespace ircd;

static const char extb_desc[] = "Oper ($o) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_oper(const char *data, client::client *client_p, chan::chan *chptr, mode::type);

DECLARE_MODULE_AV2(extb_oper, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	ext::table['o'] = eb_oper;

	return 0;
}

static void
_moddeinit(void)
{
	ext::table['o'] = NULL;
}

static int
eb_oper(const char *data, client::client *client_p, chan::chan *chptr, mode::type type)
{
	using namespace ext;

	if (data != NULL)
	{
		struct PrivilegeSet *set = privilegeset_get(data);
		if (set != NULL && client_p->localClient->privset == set)
			return MATCH;

		/* $o:admin or whatever */
		return HasPrivilege(client_p, data) ? MATCH : NOMATCH;
	}

	return IsOper(client_p) ? MATCH : NOMATCH;
}
