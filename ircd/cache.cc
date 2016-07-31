/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * cache.c - code for caching files
 *
 * Copyright (C) 2003 Lee Hardy <lee@leeh.co.uk>
 * Copyright (C) 2003-2005 ircd-ratbox development team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1.Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * 2.Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 3.The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <ircd/stdinc.h>
#include <ircd/s_conf.h>
#include <ircd/client.h>
#include <ircd/hash.h>
#include <ircd/cache.h>
#include <ircd/numeric.h>
#include <ircd/send.h>

#include <map>
#include <vector>

struct cachefile user_motd {};
struct cachefile oper_motd {};
char user_motd_changed[MAX_DATE_STRING];

std::map<std::string, std::shared_ptr<cachefile>, case_insensitive_less> help_dict_oper;
std::map<std::string, std::shared_ptr<cachefile>, case_insensitive_less> help_dict_user;

/* init_cache()
 *
 * inputs	-
 * outputs	-
 * side effects - inits the file/line cache blockheaps, loads motds
 */
void
init_cache(void)
{
	user_motd_changed[0] = '\0';

	user_motd.cache(fs::paths[IRCD_PATH_IRCD_MOTD], "ircd.motd", 0);
	oper_motd.cache(fs::paths[IRCD_PATH_IRCD_OMOTD], "opers.motd", 0);
}

/*
 * removes tabs from src, replaces with 8 spaces, and returns the length
 * of the new string.  if the new string would be greater than destlen,
 * it is truncated to destlen - 1
 */
static size_t
untabify(char *dest, const char *src, size_t destlen)
{
	size_t x = 0, i;
	const char *s = src;
	char *d = dest;

	while(*s != '\0' && x < destlen - 1)
	{
		if(*s == '\t')
		{
			for(i = 0; i < 8 && x < destlen - 1; i++, x++, d++)
				*d = ' ';
			s++;
		} else
		{
			*d++ = *s++;
			x++;
		}
	}
	*d = '\0';
	return x;
}

/* cachefile::cache()
 *
 * inputs	- file to cache, files "shortname", flags to set
 * outputs	- none
 * side effects - cachefile.contents is populated with the lines from the file
 */
void
cachefile::cache(const char *filename, const char *shortname, int flags_)
{
	FILE *in;
	char line[BUFSIZE];
	char *p;

	if((in = fopen(filename, "r")) == NULL)
		return;

	contents.clear();

	name = shortname;
	flags = flags_;

	/* cache the file... */
	while(fgets(line, sizeof(line), in) != NULL)
	{
		char untabline[BUFSIZE];

		if((p = strpbrk(line, "\r\n")) != NULL)
			*p = '\0';

		untabify(untabline, line, sizeof(untabline));
		contents.push_back(untabline);
	}

	fclose(in);
}

/* load_help()
 *
 * inputs	-
 * outputs	-
 * side effects - old help cache deleted
 *		- contents of help directories are loaded.
 */
void
load_help(void)
{
	DIR *helpfile_dir = NULL;
	struct dirent *ldirent= NULL;
	char filename[PATH_MAX];

#if defined(S_ISLNK) && defined(HAVE_LSTAT)
	struct stat sb;
#endif

	void *elem;

	help_dict_oper.clear();
	help_dict_user.clear();

	helpfile_dir = opendir(fs::paths[IRCD_PATH_OPERHELP]);

	if(helpfile_dir == NULL)
		return;

	while((ldirent = readdir(helpfile_dir)) != NULL)
	{
		if(ldirent->d_name[0] == '.')
			continue;
		snprintf(filename, sizeof(filename), "%s%c%s", fs::paths[IRCD_PATH_OPERHELP], RB_PATH_SEPARATOR, ldirent->d_name);
		help_dict_oper[ldirent->d_name] = std::make_shared<cachefile>(filename, ldirent->d_name, HELP_OPER);
	}

	closedir(helpfile_dir);
	helpfile_dir = opendir(fs::paths[IRCD_PATH_USERHELP]);

	if(helpfile_dir == NULL)
		return;

	while((ldirent = readdir(helpfile_dir)) != NULL)
	{
		if(ldirent->d_name[0] == '.')
			continue;
		snprintf(filename, sizeof(filename), "%s%c%s", fs::paths[IRCD_PATH_USERHELP], RB_PATH_SEPARATOR, ldirent->d_name);

#if defined(S_ISLNK) && defined(HAVE_LSTAT)
		if(lstat(filename, &sb) < 0)
			continue;

		/* ok, if its a symlink, we work on the presumption if an
		 * oper help exists of that name, its a symlink to that --fl
		 */
		if(S_ISLNK(sb.st_mode))
		{
			std::shared_ptr<cachefile> cacheptr = help_dict_oper[ldirent->d_name];

			if(cacheptr != NULL)
			{
				cacheptr->flags |= HELP_USER;
				continue;
			}
		}
#endif

		help_dict_user[ldirent->d_name] = std::make_shared<cachefile>(filename, ldirent->d_name, HELP_USER);
	}

	closedir(helpfile_dir);
}

/* send_user_motd()
 *
 * inputs	- client to send motd to
 * outputs	- client is sent motd if exists, else ERR_NOMOTD
 * side effects -
 */
void
send_user_motd(struct Client *source_p)
{
	rb_dlink_node *ptr;
	const char *myname = get_id(&me, source_p);
	const char *nick = get_id(source_p, source_p);

	if (user_motd.contents.size() == 0)
	{
		sendto_one(source_p, form_str(ERR_NOMOTD), myname, nick);
		return;
	}

	sendto_one(source_p, form_str(RPL_MOTDSTART), myname, nick, me.name);

	for (auto it = user_motd.contents.begin(); it != user_motd.contents.end(); it++)
	{
		sendto_one(source_p, form_str(RPL_MOTD), myname, nick, it->c_str());
	}

	sendto_one(source_p, form_str(RPL_ENDOFMOTD), myname, nick);
}

void
cache_user_motd(void)
{
	struct stat sb;
	struct tm *local_tm;

	if(stat(fs::paths[IRCD_PATH_IRCD_MOTD], &sb) == 0)
	{
		local_tm = localtime(&sb.st_mtime);

		if(local_tm != NULL)
		{
			snprintf(user_motd_changed, sizeof(user_motd_changed),
				 "%d/%d/%d %d:%d",
				 local_tm->tm_mday, local_tm->tm_mon + 1,
				 1900 + local_tm->tm_year, local_tm->tm_hour,
				 local_tm->tm_min);
		}
	}

	user_motd.cache(fs::paths[IRCD_PATH_IRCD_MOTD], "ircd.motd", 0);
}


/* send_oper_motd()
 *
 * inputs	- client to send motd to
 * outputs	- client is sent oper motd if exists
 * side effects -
 */
void
send_oper_motd(struct Client *source_p)
{
	struct cacheline *lineptr;
	rb_dlink_node *ptr;

	if (oper_motd.contents.size() == 0)
		return;

	sendto_one(source_p, form_str(RPL_OMOTDSTART),
		   me.name, source_p->name);

	for (auto it = oper_motd.contents.begin(); it != oper_motd.contents.end(); it++)
	{
		sendto_one(source_p, form_str(RPL_OMOTD),
			   me.name, source_p->name, it->c_str());
	}

	sendto_one(source_p, form_str(RPL_ENDOFOMOTD),
		   me.name, source_p->name);
}
