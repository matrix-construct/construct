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

static int m_grant(struct Client *, struct Client *, int, const char **);
static int me_grant(struct Client *, struct Client *, int, const char **);

struct Message grant_msgtab = {
	"GRANT", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_grant, 4}, mg_ignore, mg_ignore, {me_grant, 4}, mg_ignore}
};

mapi_clist_av1 grant_clist[] = { &grant_msgtab, NULL };
DECLARE_MODULE_AV1(grant, NULL, NULL, grant_clist, NULL, NULL, "Charybdis development team");

static void
apply_flags(struct membership *msptr, const char *flagspec, unsigned int flagmask)
{
	char *s, *t, *p;

	/* unset all chanroles as we're setting new ones */
	msptr->roles = 0;

	t = LOCAL_COPY(flagspec);
	for (s = rb_strtok_r(t, " ", &p); s; s = rb_strtok_r(NULL, " ", &p))
	{
		const char *priv = s + 1;       /* The actual priv */
		struct flag_list *fl;
		unsigned int flag = 0;

		/* Go through the flags list... */
		for(fl = flaglist; fl->name != NULL; fl++)
		{
			if (strcasecmp(fl->name, priv) == 0)
			{
				/* flagmask exists to ensure users can't give privileges they
				 * don't possess themselves */
				if (!flagmask || (flagmask & fl->flag))
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
}

/*
 * m_grant
 *
 * parv[1] = channel
 * parv[2] = target nickname
 * parv[3] = flag spec
 */
static int
m_grant(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	struct Channel *chptr;
	struct Client *target_p;
	struct membership *cmsptr, *tmsptr;
	char flagspec[BUFSIZE];
	const char *flagptr;

	if(parc > 4)
	{
		int i;
		char *buf = flagspec;

		flagptr = flagspec;

		for (i = 3; i < parc; i++)
			/* Rest assured it can't overflow, parv contents will always be < BUFSIZE
			 * --Elizabeth */
			buf += rb_sprintf(buf, "%s ", parv[i]);

		*buf = '\0';
	}
	else
		flagptr = parv[3];

	if (!(chptr = find_channel(parv[1])))
	{
		sendto_one_numeric(source_p, ERR_NOSUCHCHANNEL, form_str(ERR_NOSUCHCHANNEL), parv[1]);
		return 0;
	}

	if(!(cmsptr = find_channel_membership(chptr, client_p)))
		/* Can't happen */
		return 0;

	/* Check for grant privilege */
	if(get_channel_access(source_p, cmsptr, CHANROLE_GRANT) < CHFL_CHANOP)
	{
		sendto_one_numeric(source_p, ERR_CHANOPRIVSNEEDED, form_str(ERR_CHANOPRIVSNEEDED),
				me.name, source_p->name, parv[2]);
		return 0;
	}

	if (!(target_p = find_named_person(parv[2])))
	{
		sendto_one_numeric(source_p, ERR_NOSUCHNICK, form_str(ERR_NOSUCHNICK), parv[2]);
		return 0;
	}

	/* Send it to the server the user is on if not local */
	if(!MyClient(target_p))
	{
		struct Client *cptr = target_p->servptr;
		sendto_one(cptr, ":%s ENCAP %s GRANT %s %s :%s",
			get_id(source_p, cptr), cptr->name, chptr->chname,
			get_id(target_p, cptr), flagptr);
		return 0;
	}

	if (!(tmsptr = find_channel_membership(chptr, target_p)))
		return 0;
		
	apply_flags(tmsptr, flagptr, cmsptr->roles);

	return 0;
}

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

	if (!(chptr = find_channel(parv[1])))
		return 0;

	if (!(target_p = find_person(parv[2])))
		return 0;

	/* Makes no sense to do this for non-local users */
	if(!MyClient(target_p))
		return 0;

	if (!(msptr = find_channel_membership(chptr, target_p)))
		return 0;

	apply_flags(msptr, parv[3], 0);

	return 0;
}
