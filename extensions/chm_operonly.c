#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"
#include "s_conf.h"
#include "s_user.h"
#include "s_serv.h"
#include "numeric.h"
#include "chmode.h"

static void h_can_join(hook_data_channel *);

mapi_hfn_list_av1 operonly_hfnlist[] = {
	{ "can_join", (hookfn) h_can_join },
	{ NULL, NULL }
};

static unsigned int mymode;

/* This is a simple example of how to use dynamic channel modes.
 * Not tested enough yet, use at own risk.
 * -- dwr
 */
static int
_modinit(void)
{
	mymode = cflag_add('O', chm_staff);
	if (mymode == 0)
		return -1;

	return 0;
}


static void
_moddeinit(void)
{
	cflag_orphan('O');
}

DECLARE_MODULE_AV1(chm_operonly, _modinit, _moddeinit, NULL, NULL, operonly_hfnlist, "$Revision$");

static void
h_can_join(hook_data_channel *data)
{
	struct Client *source_p = data->client;
	struct Channel *chptr = data->chptr;

	if((chptr->mode.mode & mymode) && !IsOper(source_p)) {
		sendto_one_numeric(source_p, 520, "%s :Cannot join channel (+O) - you are not an IRC operator", chptr->chname);
		data->approved = ERR_CUSTOM;
	}
}

