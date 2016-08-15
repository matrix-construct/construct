/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 * cache.c - code for caching files
 *
 * Copyright (C) 2003 Lee Hardy <lee@leeh.co.uk>
 * Copyright (C) 2003-2005 ircd-ratbox development team
 * Copyright (C) 2016 Charybdis development team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
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

namespace cache = ircd::cache;
using namespace cache;

static size_t untabify(char *dest, const char *src, size_t destlen);

struct cache::file
{
	std::string filename;
	std::string shortname;
	std::ifstream stream;
	int flags;
	std::vector<std::string> contents;

	file(const char *const &filename, const char *const &shortname, const int &flags);
	file() = default;

	file &operator=(file &&) noexcept;
};

file motd::user;
file motd::oper;

dict help::oper;
dict help::user;

char motd::user_motd_changed[MAX_DATE_STRING];


/* init_cache()
 *
 * inputs	-
 * outputs	-
 * side effects - inits the file/line cache blockheaps, loads motds
 */
void
cache::init()
{
	using namespace fs::path;

	motd::user = file(get(IRCD_MOTD), "ircd.motd", 0);
	motd::oper = file(get(IRCD_OMOTD), "opers.motd", 0);
}

/* load_help()
 *
 * inputs	-
 * outputs	-
 * side effects - old help cache deleted
 *		- contents of help directories are loaded.
 */
void
cache::help::load()
{
	using namespace fs;

	oper.clear();
	user.clear();

	DIR *dir(opendir(path::get(path::OPERHELP)));
	if (!dir)
		return;

	struct dirent *ldirent(nullptr);
	while ((ldirent = readdir(dir)) != nullptr)
	{
		const auto &d_name(ldirent->d_name);
		if (d_name[0] == '.')
			continue;

		char filename[PATH_MAX];
		const auto &ophelp_path(path::get(path::OPERHELP));
		snprintf(filename, sizeof(filename), "%s%c%s", ophelp_path, RB_PATH_SEPARATOR, d_name);
		oper.emplace(d_name, std::make_shared<file>(filename, d_name, OPER));
	}

	closedir(dir);

	dir = opendir(path::get(path::USERHELP));
	if (dir == nullptr)
		return;

	while ((ldirent = readdir(dir)) != nullptr)
	{
		const auto &d_name(ldirent->d_name);
		if (d_name[0] == '.')
			continue;

		char filename[PATH_MAX];
		const auto &userhelp_path(path::get(path::USERHELP));
		snprintf(filename, sizeof(filename), "%s%c%s", userhelp_path, RB_PATH_SEPARATOR, d_name);

		#if defined(S_ISLNK) && defined(HAVE_LSTAT)
		struct stat sb;
		if (lstat(filename, &sb) < 0)
			continue;

		/* ok, if its a symlink, we work on the presumption if an
		 * oper help exists of that name, its a symlink to that --fl
		 */
		if (S_ISLNK(sb.st_mode))
		{
			const auto it(oper.find(d_name));
			if (it != end(oper))
			{
				auto &file(it->second);
				file->flags |= USER;
				continue;
			}
		}
		#endif

		user.emplace(d_name, std::make_shared<file>(filename, d_name, USER));
	}

	closedir(dir);
}

void
cache::motd::cache_user(void)
{
	struct stat sb;

	const auto &path(fs::path::get(fs::path::IRCD_MOTD));
	if (stat(path, &sb) == 0)
	{
		struct tm *const local_tm(localtime(&sb.st_mtime));
		if (local_tm != nullptr)
		{
			snprintf(user_motd_changed, sizeof(user_motd_changed),
				 "%d/%d/%d %d:%d",
				 local_tm->tm_mday, local_tm->tm_mon + 1,
				 1900 + local_tm->tm_year, local_tm->tm_hour,
				 local_tm->tm_min);
		}
	}

	user = file(path, "ircd.motd", 0);
}

void
cache::motd::cache_oper(void)
{
	oper = cache::file(fs::path::get(fs::path::IRCD_OMOTD), "opers.motd", 0);
}

/* send_user_motd()
 *
 * inputs	- client to send motd to
 * outputs	- client is sent motd if exists, else ERR_NOMOTD
 * side effects -
 */
void
cache::motd::send_user(Client *source_p)
{
	const char *const myname(get_id(&me, source_p));
	const char *const nick(get_id(source_p, source_p));

	if (user.contents.empty())
	{
		sendto_one(source_p, form_str(ERR_NOMOTD), myname, nick);
		return;
	}

	sendto_one(source_p, form_str(RPL_MOTDSTART), myname, nick, me.name);

	for (const auto &it : user.contents)
		sendto_one(source_p, form_str(RPL_MOTD), myname, nick, it.c_str());

	sendto_one(source_p, form_str(RPL_ENDOFMOTD), myname, nick);
}

/* send_oper_motd()
 *
 * inputs	- client to send motd to
 * outputs	- client is sent oper motd if exists
 * side effects -
 */
void
cache::motd::send_oper(Client *source_p)
{
	if (oper.contents.empty())
		return;

	sendto_one(source_p, form_str(RPL_OMOTDSTART), me.name, source_p->name);

	for (const auto &it : motd::oper.contents)
		sendto_one(source_p, form_str(RPL_OMOTD), me.name, source_p->name, it.c_str());

	sendto_one(source_p, form_str(RPL_ENDOFOMOTD), me.name, source_p->name);
}

/* ircd::cache::file::file()
 *
 * inputs	- file to cache, files "shortname", flags to set
 * outputs	- none
 * side effects - cachefile.contents is populated with the lines from the file
 */
file::file(const char *const &filename,
           const char *const &shortname,
           const int &flags)
:filename(filename)
,shortname(shortname)
,stream(filename)
,flags(flags)
,contents([this]
{
	std::vector<std::string> contents;
	while (stream.good())
	{
		char line[BUFSIZE], *p;
		stream.getline(line, sizeof(line));
		if ((p = strpbrk(line, "\r\n")) != nullptr)
			*p = '\0';

		char untabline[BUFSIZE];
		untabify(untabline, line, sizeof(untabline));
		contents.emplace_back(untabline);
	}

	stream.close();
	return contents;
}())
{
}

file &file::operator=(file &&other)
noexcept
{
	filename = std::move(other.filename);
	shortname = std::move(other.shortname);
	flags = std::move(other.flags);
	contents = std::move(other.contents);
	return *this;
}

uint
cache::flags(const file &file)
{
	return file.flags;
}

const std::string &
cache::name(const file &file)
{
	return file.shortname;
}

const std::vector<std::string> &
cache::contents(const file &file)
{
	return file.contents;
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
