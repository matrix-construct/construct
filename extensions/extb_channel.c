/*
 * Channel extban type: matches users who are in a certain public channel
 * -- jilles
 */

#include "stdinc.h"
#include "modules.h"
#include "client.h"
#include "channel.h"
#include "hash.h"
#include "ircd.h"

static int _modinit(void);
static void _moddeinit(void);
static int eb_channel(const char *data, struct Client *client_p, struct Channel *chptr, long mode_type);

DECLARE_MODULE_AV2(extb_channel, _modinit, _moddeinit, NULL, NULL, NULL, NULL, NULL, NULL);

static int
_modinit(void)
{
	extban_table['c'] = eb_channel;

	return 0;
}

static void
_moddeinit(void)
{
	extban_table['c'] = NULL;
}

static int eb_channel(const char *data, struct Client *client_p,
		struct Channel *chptr, long mode_type)
{
	struct Channel *chptr2;

	(void)chptr;
	(void)mode_type;
	if (data == NULL)
		return EXTBAN_INVALID;
	chptr2 = find_channel(data);
	if (chptr2 == NULL)
		return EXTBAN_INVALID;
	/* require consistent target */
	if (chptr->chname[0] == '#' && data[0] == '&')
		return EXTBAN_INVALID;
	/* privacy! don't allow +s/+p channels to influence another channel */
	if (!PubChannel(chptr2) && chptr2 != chptr)
		return EXTBAN_INVALID;
	return IsMember(client_p, chptr2) ? EXTBAN_MATCH : EXTBAN_NOMATCH;
}
