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

mapi_hfn_list_av1 sslonly_hfnlist[] = {
	{ "can_join", (hookfn) h_can_join },
	{ NULL, NULL }
};

static int
_modinit(void)
{
	chmode_table['S'].mode_type = find_cflag_slot();
	chmode_table['S'].set_func = chm_simple;

	construct_noparam_modes();

	return 0;
}


static void
_moddeinit(void)
{
	chmode_table['S'].mode_type = 0;

	construct_noparam_modes();
}

DECLARE_MODULE_AV1(chm_sslonly, _modinit, _moddeinit, NULL, NULL, sslonly_hfnlist, "$Revision$");

static void
h_can_join(hook_data_channel *data)
{
	struct Client *source_p = data->client;
	struct Channel *chptr = data->chptr;

	if((chptr->mode.mode & chmode_flags['S']) && !IsSSLClient(source_p)) {
		sendto_one_notice(source_p, ":Only users using SSL could join this channel!");
		data->approved = ERR_CUSTOM;
	}
}

