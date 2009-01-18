/*
 *  m_mkpasswd.c: Encrypts a password online, DES or MD5.
 *
 *  Copyright 2002 W. Campbell and the ircd-ratbox development team
 *  Based on mkpasswd.c, originally by Nelson Minar (minar@reed.edu)
 *
 *  You can use this code in any way as long as these names remain.
 *
 *  $Id: m_mkpasswd.c 3161 2007-01-25 07:23:01Z nenolod $
 */

/* List of ircd includes from ../include/ */
#include "stdinc.h"
#include "client.h"
#include "common.h"		/* FALSE bleah */
#include "ircd.h"
#include "match.h"
#include "numeric.h"
#include "s_newconf.h"
#include "s_conf.h"
#include "logger.h"
#include "s_serv.h"
#include "send.h"
#include "msg.h"
#include "parse.h"
#include "modules.h"

#include <string.h>

static int m_mkpasswd(struct Client *client_p, struct Client *source_p,
		      int parc, const char *parv[]);
static int mo_mkpasswd(struct Client *client_p, struct Client *source_p,
		       int parc, const char *parv[]);
static char *make_salt(void);
static char *make_md5_salt(void);

static char saltChars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";


struct Message mkpasswd_msgtab = {
	"MKPASSWD", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_mkpasswd, 2}, mg_ignore, mg_ignore, mg_ignore, {mo_mkpasswd, 2}}
};

mapi_clist_av1 mkpasswd_clist[] = { &mkpasswd_msgtab, NULL };

DECLARE_MODULE_AV1(mkpasswd, NULL, NULL, mkpasswd_clist, NULL, NULL, "$Revision: 3161 $");


static int
m_mkpasswd(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0;
	int is_md5 = 0;

	if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
	{
		/* safe enough to give this on a local connect only */
		sendto_one(source_p, form_str(RPL_LOAD2HI), me.name, source_p->name, "MKPASSWD");
		return 0;
	}
	else
	{
		last_used = rb_current_time();
	}

	if(parc == 3)
	{
		if(!irccmp(parv[2], "MD5"))
		{
			is_md5 = 1;
		}
		else if(!irccmp(parv[2], "DES"))
		{
			/* Not really needed, but we may want to have a default encryption
			 * setting somewhere down the road
			 */
			is_md5 = 0;
		}
		else
		{
			sendto_one_notice(source_p, ":MKPASSWD syntax error:  MKPASSWD pass [DES|MD5]");
			return 0;
		}
	}

	if(parc == 1)
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "MKPASSWD");
	else
		sendto_one_notice(source_p, ":Encryption for [%s]:  %s",
			   parv[1], rb_crypt(parv[1],
							    is_md5 ? make_md5_salt() :
							    make_salt()));

	return 0;
}

/*
** mo_test
**      parv[1] = parameter
*/
static int
mo_mkpasswd(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	int is_md5 = 0;

	if(parc == 3)
	{
		if(!irccmp(parv[2], "MD5"))
		{
			is_md5 = 1;
		}
		else if(!irccmp(parv[2], "DES"))
		{
			/* Not really needed, but we may want to have a default encryption
			 * setting somewhere down the road
			 */
			is_md5 = 0;
		}
		else
		{
			sendto_one_notice(source_p,
				   ":MKPASSWD syntax error:  MKPASSWD pass [DES|MD5]");
			return 0;
		}
	}

	if(parc == 1)
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "MKPASSWD");
	else
		sendto_one_notice(source_p, ":Encryption for [%s]:  %s",
			   parv[1], rb_crypt(parv[1], is_md5 ? make_md5_salt() : make_salt()));

	return 0;
}

static char *
make_salt(void)
{
	static char salt[3];
	salt[0] = saltChars[random() % 64];
	salt[1] = saltChars[random() % 64];
	salt[2] = '\0';
	return salt;
}

static char *
make_md5_salt(void)
{
	static char salt[13];
	int i;
	salt[0] = '$';
	salt[1] = '1';
	salt[2] = '$';
	for(i = 3; i < 11; i++)
		salt[i] = saltChars[random() % 64];
	salt[11] = '$';
	salt[12] = '\0';
	return salt;
}
