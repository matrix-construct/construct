#pragma once
#define HAVE_IRCD_CACHE_H

#define HELP_MAX	100

#define CACHEFILELEN	30
/* two servernames, a gecos, three spaces, ":1", '\0' */
#define LINKSLINELEN	(HOSTLEN + HOSTLEN + REALLEN + 6)

#define HELP_USER	0x001
#define HELP_OPER	0x002

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
