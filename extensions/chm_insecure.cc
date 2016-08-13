#include <ircd/stdinc.h>
#include <ircd/modules.h>
#include <ircd/hook.h>
#include <ircd/client.h>
#include <ircd/ircd.h>
#include <ircd/send.h>
#include <ircd/s_conf.h>
#include <ircd/s_user.h>
#include <ircd/s_serv.h>
#include <ircd/numeric.h>
#include <ircd/chmode.h>

using namespace ircd;

static const char chm_insecure_desc[] =
	"Adds channel mode +U that allows non-SSL users to join a channel, "
	"disallowing them by default";

static void h_can_join(hook_data_channel *);

mapi_hfn_list_av1 sslonly_hfnlist[] = {
	{ "can_join", (hookfn) h_can_join },
	{ NULL, NULL }
};

static unsigned int mymode;

static int
_modinit(void)
{
	mymode = cflag_add('U', CHM_D, chm_simple);
	if (mymode == 0)
		return -1;

	return 0;
}


static void
_moddeinit(void)
{
	cflag_orphan('U');
}

DECLARE_MODULE_AV2(chm_insecure, _modinit, _moddeinit, NULL, NULL, sslonly_hfnlist, NULL, NULL, chm_insecure_desc);

static void
h_can_join(hook_data_channel *data)
{
	struct Client *source_p = data->client;
	struct Channel *chptr = data->chptr;

	if(!(chptr->mode.mode & mymode) && !IsSSLClient(source_p)) {
		/* XXX This is equal to ERR_THROTTLE */
		sendto_one_numeric(source_p, 480, "%s :Cannot join channel (-U) - SSL/TLS required", chptr->chname);
		data->approved = ERR_CUSTOM;
	}
}

