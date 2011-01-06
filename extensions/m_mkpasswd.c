/*
 * m_mkpasswd.c: Encrypts a password online.
 *
 * Based on mkpasswd.c, originally by Nelson Minar (minar@reed.edu)
 * You can use this code in any way as long as these names remain.
 */

#include "stdinc.h"
#include "client.h"
#include "numeric.h"
#include "s_conf.h"
#include "modules.h"

#include <string.h>

static int m_mkpasswd(struct Client *client_p, struct Client *source_p,
		      int parc, const char *parv[]);
static int mo_mkpasswd(struct Client *client_p, struct Client *source_p,
		       int parc, const char *parv[]);

static char *make_md5_salt(int);
static char *make_sha256_salt(int);
static char *make_sha512_salt(int);
static char *generate_random_salt(char *, int);
static char *generate_poor_salt(char *, int);

static char saltChars[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	/* 0 .. 63, ascii - 64 */

struct Message mkpasswd_msgtab = {
	"MKPASSWD", 0, 0, 0, MFLG_SLOW,
	{mg_unreg, {m_mkpasswd, 2}, mg_ignore, mg_ignore, mg_ignore, {mo_mkpasswd, 2}}
};

mapi_clist_av1 mkpasswd_clist[] = { &mkpasswd_msgtab, NULL };

DECLARE_MODULE_AV1(mkpasswd, NULL, NULL, mkpasswd_clist, NULL, NULL, "$Revision$");


/* m_mkpasswd - mkpasswd message handler
 *	parv[1] = password
 *	parv[2] = type
 */
static int
m_mkpasswd(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	static time_t last_used = 0;
	char *salt;
	const char *hashtype;
	const char hashdefault[] = "SHA512";

	if(EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "MKPASSWD");
		return 0;
	}

	if(parc < 3)
		hashtype = hashdefault;
	else
		hashtype = parv[2];

	if((last_used + ConfigFileEntry.pace_wait) > rb_current_time())
	{
		/* safe enough to give this on a local connect only */
		sendto_one(source_p, form_str(RPL_LOAD2HI), me.name, source_p->name, "MKPASSWD");
		return 0;
	}
	else
		last_used = rb_current_time();

	if(!irccmp(hashtype, "SHA256"))
		salt = make_sha256_salt(16);
	else if(!irccmp(hashtype, "SHA512"))
		salt = make_sha512_salt(16);
	else if(!irccmp(hashtype, "MD5"))
		salt = make_md5_salt(8);
	else
	{
		sendto_one_notice(source_p,
				  ":MKPASSWD syntax error:  MKPASSWD pass [SHA256|SHA512|MD5]");
		return 0;
	}

	sendto_one_notice(source_p, ":Hash [%s] for %s: %s", hashtype, parv[1], rb_crypt(parv[1], salt));
	return 0;
}

/* mo_mkpasswd - mkpasswd message handler
 *	parv[1] = password
 *	parv[2] = type
 */
static int
mo_mkpasswd(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	char *salt;
	const char *hashtype;
	const char hashdefault[] = "SHA512";

	if(EmptyString(parv[1]))
	{
		sendto_one(source_p, form_str(ERR_NEEDMOREPARAMS), me.name, source_p->name, "MKPASSWD");
		return 0;
	}

	if(parc < 3)
		hashtype = hashdefault;
	else
		hashtype = parv[2];

	if(!irccmp(hashtype, "SHA256"))
		salt = make_sha256_salt(16);
	else if(!irccmp(hashtype, "SHA512"))
		salt = make_sha512_salt(16);
	else if(!irccmp(hashtype, "MD5"))
		salt = make_md5_salt(8);
	else
	{
		sendto_one_notice(source_p,
				  ":MKPASSWD syntax error:  MKPASSWD pass [SHA256|SHA512|MD5]");
		return 0;
	}

	sendto_one_notice(source_p, ":Hash [%s] for %s: %s", hashtype, parv[1], rb_crypt(parv[1], salt));
	return 0;
}

char *
make_md5_salt(int length)
{
	static char salt[21];
	if(length > 16)
	{
		printf("MD5 salt length too long\n");
		exit(0);
	}
	salt[0] = '$';
	salt[1] = '1';
	salt[2] = '$';
	generate_random_salt(&salt[3], length);
	salt[length + 3] = '$';
	salt[length + 4] = '\0';
	return salt;
}

char *
make_sha256_salt(int length)
{
	static char salt[21];
	if(length > 16)
	{
		printf("SHA256 salt length too long\n");
		exit(0);
	}
	salt[0] = '$';
	salt[1] = '5';
	salt[2] = '$';
	generate_random_salt(&salt[3], length);
	salt[length + 3] = '$';
	salt[length + 4] = '\0';
	return salt;
}

char *
make_sha512_salt(int length)
{
	static char salt[21];
	if(length > 16)
	{
		printf("SHA512 salt length too long\n");
		exit(0);
	}
	salt[0] = '$';
	salt[1] = '6';
	salt[2] = '$';
	generate_random_salt(&salt[3], length);
	salt[length + 3] = '$';
	salt[length + 4] = '\0';
	return salt;
}

char *
generate_poor_salt(char *salt, int length)
{
	int i;
	srand(time(NULL));
	for(i = 0; i < length; i++)
	{
		salt[i] = saltChars[rand() % 64];
	}
	return (salt);
}

char *
generate_random_salt(char *salt, int length)
{
	char *buf;
	int fd, i;
	if((fd = open("/dev/random", O_RDONLY)) < 0)
	{
		return (generate_poor_salt(salt, length));
	}
	buf = calloc(1, length);
	if(read(fd, buf, length) != length)
	{
		free(buf);
		return (generate_poor_salt(salt, length));
	}

	for(i = 0; i < length; i++)
	{
		salt[i] = saltChars[abs(buf[i]) % 64];
	}
	free(buf);
	return (salt);
}
