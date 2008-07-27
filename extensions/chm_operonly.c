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



/* This is a simple example of how to use dynamic channel modes.
 * Not tested enough yet, use at own risk.
 * -- dwr
 */
static int
_modinit(void)
{
	/* add the channel mode to the available slot */
	chmode_table['O'].mode_type = find_cflag_slot();
	chmode_table['O'].set_func = chm_simple;

	construct_noparam_modes();

	return 0;
}


/* Well, the first ugly thing is that we changle chmode_table in _modinit
 * and chmode_flags in _moddeinit (different arrays) - must be fixed.
 * -- dwr
 */
static void
_moddeinit(void)
{
	/* disable the channel mode and remove it from the available list */
	chmode_table['O'].mode_type = 0;

	construct_noparam_modes();
}

DECLARE_MODULE_AV1(chm_operonly, _modinit, _moddeinit, NULL, NULL, operonly_hfnlist, "$Revision$");

static void
h_can_join(hook_data_channel *data)
{
	struct Client *source_p = data->client;
	struct Channel *chptr = data->chptr;

	if((chptr->mode.mode & chmode_flags['O']) && !IsOper(source_p)) {
		sendto_one_notice(source_p, ":Only IRC Operators could join this channel!");
		data->approved = ERR_CUSTOM;
	}
}

