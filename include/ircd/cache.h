/**
 * Copyright (C) 2016 Charybdis development team
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
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

#pragma once
#define HAVE_IRCD_CACHE_H

#define HELP_MAX	100
#define CACHEFILELEN	30
/* two servernames, a gecos, three spaces, ":1", '\0' */
#define LINKSLINELEN	(HOSTLEN + HOSTLEN + REALLEN + 6)
#define HELP_USER	0x001
#define HELP_OPER	0x002

#ifdef __cplusplus
namespace ircd {

struct cachefile
{
	cachefile(const char *filename, const char *shortname, int flags) {
		cache(filename, shortname, flags);
	}

	cachefile() {};

	std::string name;
	std::vector<std::string> contents;
	int flags;

	void cache(const char *filename, const char *shortname, int flags);
};

void init_cache(void);
void cache_links(void *unused);

void load_help(void);

void send_user_motd(struct Client *);
void send_oper_motd(struct Client *);
void cache_user_motd(void);

extern struct cachefile user_motd;
extern struct cachefile oper_motd;

extern char user_motd_changed[MAX_DATE_STRING];

extern std::map<std::string, std::shared_ptr<cachefile>, case_insensitive_less> help_dict_oper;
extern std::map<std::string, std::shared_ptr<cachefile>, case_insensitive_less> help_dict_user;

}       // namespace ircd
#endif  // __cplusplus
