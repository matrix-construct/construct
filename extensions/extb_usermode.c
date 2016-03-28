/*
 * Usermode extban type: bans all users with a specific usermode
 * -- nenolod
 */

#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "hash.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_serv.h"
#include "numeric.h"

static const char extb_desc[] = "Usermode ($m) extban type";

static int _modinit(void);
static void _moddeinit(void);
static int eb_usermode(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV2(extb_usermode, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, extb_desc);

static int
_modinit(void)
{
	extban_table['u'] = eb_usermode;

	return 0;
}

static void
_moddeinit(void)
{
	extban_table['u'] = NULL;
}

static int eb_usermode(const char *data, struct Client *client_p,
		struct Channel *chptr, long mode_type)
{
	int dir = MODE_ADD;
	unsigned int modes_ack = 0, modes_nak = 0;
	const char *p;

	(void)chptr;

	/* $m must have a specified mode */
	if (data == NULL)
		return EXTBAN_INVALID;

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
				modes_nak |= user_modes[(unsigned char) *p];
				break;
			case MODE_ADD:
			default:
				modes_ack |= user_modes[(unsigned char) *p];
				break;
			}
			break;
		}
	}

	return ((client_p->umodes & modes_ack) == modes_ack &&
			!(client_p->umodes & modes_nak)) ?
		EXTBAN_MATCH : EXTBAN_NOMATCH;
}
