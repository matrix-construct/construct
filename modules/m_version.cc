/*
 *  ircd-ratbox: A slightly useful ircd.
 *  m_version.c: Shows ircd version information.
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

static const char version_desc[] =
	"Provides the VERSION command to display server version information";

static char *confopts(void);

static void m_version(struct MsgBuf *, client::client &, client::client &, int, const char **);
static void mo_version(struct MsgBuf *, client::client &, client::client &, int, const char **);

struct Message version_msgtab = {
	"VERSION", 0, 0, 0, 0,
	{mg_unreg, {m_version, 0}, {mo_version, 0}, {mo_version, 0}, mg_ignore, {mo_version, 0}}
};

mapi_clist_av1 version_clist[] = { &version_msgtab, NULL };

DECLARE_MODULE_AV2(version, NULL, NULL, version_clist, NULL, NULL, NULL, NULL, version_desc);

/*
 * m_version - VERSION command handler
 *      parv[1] = remote server
 */
static void
m_version(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	static time_t last_used = 0L;

	if(parc > 1)
	{
		if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
		{
			/* safe enough to give this on a local connect only */
			sendto_one(&source, form_str(RPL_LOAD2HI),
				   me.name, source.name, "VERSION");
			return;
		}
		else
			last_used = rb_current_time();

		if(hunt_server(&client, &source, ":%s VERSION :%s", 1, parc, parv) != HUNTED_ISME)
			return;
	}

	sendto_one_numeric(&source, RPL_VERSION, form_str(RPL_VERSION),
			   info::version.c_str(), info::serno.c_str(),
#ifdef CUSTOM_BRANDING
			   PACKAGE_NAME "-" PACKAGE_VERSION,
#endif
			   me.name, confopts(), TS_CURRENT,
			   ServerInfo.sid);

	show_isupport(&source);
}

/*
 * mo_version - VERSION command handler
 *      parv[1] = remote server
 */
static void
mo_version(struct MsgBuf *msgbuf_p, client::client &client, client::client &source, int parc, const char *parv[])
{
	if(hunt_server(&client, &source, ":%s VERSION :%s", 1, parc, parv) == HUNTED_ISME)
	{
		sendto_one_numeric(&source, RPL_VERSION, form_str(RPL_VERSION),
				   info::version.c_str(), info::serno.c_str(),
#ifdef CUSTOM_BRANDING
				   PACKAGE_NAME "-" PACKAGE_VERSION,
#endif
				   me.name, confopts(), TS_CURRENT,
				   ServerInfo.sid);
		show_isupport(&source);
	}
}

/* confopts()
 * input  - none
 * output - ircd.conf option string
 * side effects - none
 */
static char *
confopts(void)
{
	static char result[15];
	char *p;

	result[0] = '\0';
	p = result;

	if(ConfigChannel.use_except)
		*p++ = 'e';

	if(ConfigChannel.use_invex)
		*p++ = 'I';

	if(ConfigChannel.use_knock)
		*p++ = 'K';

	*p++ = 'M';
	*p++ = 'p';

	if(opers_see_all_users || ConfigFileEntry.operspy_dont_care_user_info)
		*p++ = 'S';

#ifdef HAVE_LIBZ
	*p++ = 'Z';
#endif

#ifdef RB_IPV6
	*p++ = '6';
#endif

	*p = '\0';

	return result;
}
