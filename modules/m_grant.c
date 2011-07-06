/*
 *  charybdis: an advanced ircd
 *  m_grant: handle services grant commands
 */

#include "stdinc.h"
#include "client.h"
#include "ircd.h"
#include "numeric.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"
#include "s_conf.h"
#include "hash.h"

struct flag_list
{
	char *name;
	int flag;
};

static struct flag_list flaglist[] = {
	{"kick",	CHANROLE_KICK},
	{"grant",	CHANROLE_GRANT},
	{"mode",	CHANROLE_MODE},
	{"topic",	CHANROLE_TOPIC},
	{"status",	CHANROLE_STATUS},
	{NULL,		0},
};

static int me_grant(struct Client *, struct Client *, int, const char **);

struct Message grant_msgtab = {
	"GRANT", 0, 0, 0, MFLG_SLOW,
	{mg_ignore, mg_ignore, mg_ignore, mg_ignore, {me_grant, 4}, mg_ignore}
};

mapi_clist_av1 grant_clist[] = { &grant_msgtab, NULL };
DECLARE_MODULE_AV1(grant, NULL, NULL, grant_clist, NULL, NULL, "Charybdis development team");

/*
 * me_grant
 *
 * parv[1] = channel
 * parv[2] = target UID
 * parv[3] = flag spec
*/
static int
me_grant(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr;
	struct Client *target_p;
	struct membership *msptr;
	char *s, *t, *p;

	if (!(chptr = find_channel(parv[1])))
		return 0;

	if (!(target_p = find_person(parv[2])))
		return 0;

	/* Makes no sense to do this for non-local users */
	if(!MyClient(target_p))
		return 0;

	if (!(msptr = find_channel_membership(chptr, target_p)))
		return 0;

	/* unset all chanroles as we're setting new ones */
	msptr->roles = 0;

	t = LOCAL_COPY(parv[3]);
	for (s = rb_strtok_r(t, " ", &p); s; s = rb_strtok_r(NULL, " ", &p))
	{
		const char *priv = s + 1;	/* The actual priv */
		struct flag_list *fl;
		int flag = 0;

		/* Go through the flags list... */
		for(fl = flaglist; fl->name != NULL; fl++)
		{
			if (strcasecmp(fl->name, priv) == 0)
			{
				flag = fl->flag;
				break;
			}
		}

		/* Ack no flag! */
		if (!flag)
			continue;

		if (s[0] == '-')
			RemoveChanRole(msptr, flag);
		else if (s[0] == '+')
			SetChanRole(msptr, flag);
	}

	return 0;
}
