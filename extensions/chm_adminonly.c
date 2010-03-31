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

mapi_hfn_list_av1 adminonly_hfnlist[] = {
	{ "can_join", (hookfn) h_can_join },
	{ NULL, NULL }
};

static unsigned int mymode;

static int
_modinit(void)
{
	mymode = cflag_add('A', chm_staff);
	if (mymode == 0)
		return -1;

	return 0;
}

static void
_moddeinit(void)
{
	cflag_orphan('A');
}

DECLARE_MODULE_AV1(chm_adminonly, _modinit, _moddeinit, NULL, NULL, adminonly_hfnlist, "$Revision$");

static void
h_can_join(hook_data_channel *data)
{
	struct Client *source_p = data->client;
	struct Channel *chptr = data->chptr;

	if((chptr->mode.mode & mymode) && !IsAdmin(source_p)) {
		sendto_one_numeric(source_p, 519, "%s :Cannot join channel (+A) - you are not an IRC server administrator", chptr->chname);
		data->approved = ERR_CUSTOM;
	}
}

