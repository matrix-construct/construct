/*
 * Usermode extban type: bans all users with a specific usermode
 * -- nenolod
 */

namespace mode = ircd::chan::mode;
namespace ext = mode::ext;
using namespace ircd;

static const char extb_desc[] = "Usermode ($m) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_usermode(const char *data, client::client *client_p, chan::chan *chptr, mode::type);

DECLARE_MODULE_AV2(extb_usermode, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	ext::table['u'] = eb_usermode;

	return 0;
}

static void
_moddeinit(void)
{
	ext::table['u'] = NULL;
}

static int eb_usermode(const char *data, client::client *client_p,
		chan::chan *chptr, mode::type type)
{
	using namespace ext;

	int dir = MODE_ADD;
	unsigned int modes_ack = 0, modes_nak = 0;
	const char *p;

	(void)chptr;

	/* $m must have a specified mode */
	if (data == NULL)
		return INVALID;

	for (p = data; *p != '\0'; p++)
	{
		switch (*p)
		{
		case '+':
			dir = MODE_ADD;
			break;
		case '-':
			dir = MODE_DEL;
			break;
		default:
			switch (dir)
			{
			case MODE_DEL:
				modes_nak |= umode::table[(unsigned char) *p];
				break;
			case MODE_ADD:
			default:
				modes_ack |= umode::table[(unsigned char) *p];
				break;
			}
			break;
		}
	}

	return ((client_p->mode & modes_ack) == modes_ack &&
			!(client_p->mode & modes_nak)) ?
		MATCH : NOMATCH;
}
