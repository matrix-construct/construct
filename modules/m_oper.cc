/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_oper.c: Makes a user an IRC Operator.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

using namespace ircd;

static const char oper_desc[] = "Provides the OPER command to become an IRC operator";

static void m_oper(struct MsgBuf *, client::client &, client::client &, int, const char **);

static bool match_oper_password(const char *password, struct oper_conf *oper_p);

struct Message oper_msgtab = {
	"OPER", 0, 0, 0, 0,
	{mg_unreg, {m_oper, 3}, mg_ignore, mg_ignore, mg_ignore, {m_oper, 3}}
};

mapi_clist_av1 oper_clist[] = { &oper_msgtab, NULL };

DECLARE_MODULE_AV2(oper, NULL, NULL, oper_clist, NULL, NULL, NULL, NULL, oper_desc);

/*
 * m_oper
 *      parv[1] = oper name
 *      parv[2] = oper password
 */
static void
m_oper(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	struct oper_conf *oper_p;
	const char *name;
	const char *password;

	name = parv[1];
	password = parv[2];

	if(is(source, umode::OPER))
	{
		sendto_one(&source, form_str(RPL_YOUREOPER), me.name, source.name);
		cache::motd::send_oper(source);
		return;
	}

	/* end the grace period */
	if(!is_flood_done(source))
		flood_endgrace(&source);

	oper_p = find_oper_conf(source.username, source.orighost,
				source.sockhost, name);

	if(oper_p == NULL)
	{
		sendto_one_numeric(&source, ERR_NOOPERHOST, form_str(ERR_NOOPERHOST));
		ilog(L_FOPER, "FAILED OPER (%s) by (%s!%s@%s) (%s)",
		     name, source.name,
		     source.username, source.host, source.sockhost);

		if(ConfigFileEntry.failed_oper_notice)
		{
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
					     "Failed OPER attempt - host mismatch by %s (%s@%s)",
					     source.name, source.username, source.host);
		}

		return;
	}

	if(IsOperConfNeedSSL(oper_p) && !is(source, umode::SSLCLIENT))
	{
		sendto_one_numeric(&source, ERR_NOOPERHOST, form_str(ERR_NOOPERHOST));
		ilog(L_FOPER, "FAILED OPER (%s) by (%s!%s@%s) (%s) -- requires SSL/TLS",
		     name, source.name,
		     source.username, source.host, source.sockhost);

		if(ConfigFileEntry.failed_oper_notice)
		{
			sendto_realops_snomask(SNO_GENERAL, L_ALL,
					     "Failed OPER attempt - missing SSL/TLS by %s (%s@%s)",
					     source.name, source.username, source.host);
		}
		return;
	}

	if (oper_p->certfp != NULL)
	{
		if (source.certfp == NULL || rb_strcasecmp(source.certfp, oper_p->certfp))
		{
			sendto_one_numeric(&source, ERR_NOOPERHOST, form_str(ERR_NOOPERHOST));
			ilog(L_FOPER, "FAILED OPER (%s) by (%s!%s@%s) (%s) -- client certificate fingerprint mismatch",
			     name, source.name,
			     source.username, source.host, source.sockhost);

			if(ConfigFileEntry.failed_oper_notice)
			{
				sendto_realops_snomask(SNO_GENERAL, L_ALL,
						     "Failed OPER attempt - client certificate fingerprint mismatch by %s (%s@%s)",
						     source.name, source.username, source.host);
			}
			return;
		}
	}

	if(match_oper_password(password, oper_p))
	{
		oper_up(&source, oper_p);

		ilog(L_OPERED, "OPER %s by %s!%s@%s (%s)",
		     name, source.name, source.username, source.host,
		     source.sockhost);
		return;
	}
	else
	{
		sendto_one(&source, form_str(ERR_PASSWDMISMATCH),
			   me.name, source.name);

		ilog(L_FOPER, "FAILED OPER (%s) by (%s!%s@%s) (%s)",
		     name, source.name, source.username, source.host,
		     source.sockhost);

		if(ConfigFileEntry.failed_oper_notice)
		{
			sendto_realops_snomask(SNO_GENERAL, L_NETWIDE,
					     "Failed OPER attempt by %s (%s@%s)",
					     source.name, source.username, source.host);
		}
	}
}

/*
 * match_oper_password
 *
 * inputs       - pointer to given password
 *              - pointer to Conf
 * output       - true if match, false otherwise
 * side effects - none
 */
static bool
match_oper_password(const char *password, struct oper_conf *oper_p)
{
	const char *encr;

	/* passwd may be NULL pointer. Head it off at the pass... */
	if(EmptyString(oper_p->passwd))
		return false;

	if(IsOperConfEncrypted(oper_p))
	{
		/* use first two chars of the password they send in as salt */
		/* If the password in the conf is MD5, and ircd is linked
		 * to scrypt on FreeBSD, or the standard crypt library on
		 * glibc Linux, then this code will work fine on generating
		 * the proper encrypted hash for comparison.
		 */
		if(!EmptyString(password))
			encr = rb_crypt(password, oper_p->passwd);
		else
			encr = "";
	}
	else
		encr = password;

	if(encr != NULL && strcmp(encr, oper_p->passwd) == 0)
		return true;
	else
		return false;
}
