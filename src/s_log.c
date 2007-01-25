/*
 * ircd-ratbox: an advanced Internet Relay Chat Daemon(ircd).
 *
 * Copyright (C) 2003 Lee H <lee@leeh.co.uk>
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
 *
 * $Id: s_log.c 1563 2006-06-02 00:43:35Z nenolod $
 */

#include "stdinc.h"
#include "ircd_defs.h"
#include "s_log.h"
#include "s_conf.h"
#include "sprintf_irc.h"
#include "send.h"
#include "client.h"
#include "s_serv.h"

static FILE *log_main;
static FILE *log_user;
static FILE *log_fuser;
static FILE *log_oper;
static FILE *log_foper;
static FILE *log_server;
static FILE *log_kill;
static FILE *log_gline;
static FILE *log_kline;
static FILE *log_operspy;
static FILE *log_ioerror;

struct log_struct
{
	char **name;
	FILE **logfile;
};

static struct log_struct log_table[LAST_LOGFILE] =
{
	{ NULL, 				&log_main	},
	{ &ConfigFileEntry.fname_userlog,	&log_user	},
	{ &ConfigFileEntry.fname_fuserlog,	&log_fuser	},
	{ &ConfigFileEntry.fname_operlog,	&log_oper	},
	{ &ConfigFileEntry.fname_foperlog,	&log_foper	},
	{ &ConfigFileEntry.fname_serverlog,	&log_server	},
	{ &ConfigFileEntry.fname_killlog,	&log_kill	},
	{ &ConfigFileEntry.fname_klinelog,	&log_kline	},
	{ &ConfigFileEntry.fname_glinelog,	&log_gline	},
	{ &ConfigFileEntry.fname_operspylog,	&log_operspy	},
	{ &ConfigFileEntry.fname_ioerrorlog,	&log_ioerror	}
};

void
init_main_logfile(void)
{
	if(log_main == NULL)
		log_main = fopen(LPATH, "a");
}

void
open_logfiles(void)
{
	int i;

	if(log_main != NULL)
		fclose(log_main);

	log_main = fopen(LPATH, "a");

	/* log_main is handled above, so just do the rest */
	for(i = 1; i < LAST_LOGFILE; i++)
	{
		/* close open logfiles */
		if(*log_table[i].logfile != NULL)
		{
			fclose(*log_table[i].logfile);
			*log_table[i].logfile = NULL;
		}

		/* reopen those with paths */
		if(!EmptyString(*log_table[i].name))
			*log_table[i].logfile = fopen(*log_table[i].name, "a");
	}
}			

void
ilog(ilogfile dest, const char *format, ...)
{
	FILE *logfile = *log_table[dest].logfile;
	char buf[BUFSIZE];
	char buf2[BUFSIZE];
	va_list args;

	if(logfile == NULL)
		return;

	va_start(args, format);
	ircvsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	ircsnprintf(buf2, sizeof(buf2), "%s %s\n", smalldate(), buf);

	if(fputs(buf2, logfile) < 0)
	{
		fclose(logfile);
		*log_table[dest].logfile = NULL;
	}

	fflush(logfile);
}

static void
_iprint(const char *domain, char *buf)
{
	if (domain == NULL || buf == NULL)
		return;

	fprintf(stderr, "%8s: %s\n", domain, buf);
}

void
inotice(const char *format, ...)
{
	char buf[BUFSIZE];
	va_list args;

	va_start(args, format);
	ircvsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	_iprint("notice", buf);

	ilog(L_MAIN, "%s", buf);
}

void
iwarn(const char *format, ...)
{
	char buf[BUFSIZE];
	va_list args;

	va_start(args, format);
	ircvsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	_iprint("warning", buf);

	ilog(L_MAIN, "%s", buf);
}

void
ierror(const char *format, ...)
{
	char buf[BUFSIZE];
	va_list args;

	va_start(args, format);
	ircvsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	_iprint("error", buf);

	ilog(L_MAIN, "%s", buf);
}

void
report_operspy(struct Client *source_p, const char *token, const char *arg)
{
	/* if its not my client its already propagated */
	if(MyClient(source_p))
		sendto_match_servs(source_p, "*", CAP_ENCAP, NOCAPS,
				   "ENCAP * OPERSPY %s %s",
				   token, arg ? arg : "");

	sendto_realops_snomask(SNO_OPERSPY,
			     ConfigFileEntry.operspy_admin_only ? L_ADMIN : L_ALL,
			     "OPERSPY %s %s %s",
			     get_oper_name(source_p), token,
			     arg ? arg : "");

	ilog(L_OPERSPY, "OPERSPY %s %s %s",
	     get_oper_name(source_p), token, arg ? arg : "");
}

const char *
smalldate(void)
{
	static char buf[MAX_DATE_STRING];
	struct tm *lt;
	time_t ltime = CurrentTime;

	lt = localtime(&ltime);

	ircsnprintf(buf, sizeof(buf), "%d/%d/%d %02d.%02d",
		    lt->tm_year + 1900, lt->tm_mon + 1,
		    lt->tm_mday, lt->tm_hour, lt->tm_min);

	return buf;
}

/*
 * report_error - report an error from an errno. 
 * Record error to log and also send a copy to all *LOCAL* opers online.
 *
 *        text        is a *format* string for outputing error. It must
 *                contain only two '%s', the first will be replaced
 *                by the sockhost from the client_p, and the latter will
 *                be taken from sys_errlist[errno].
 *
 *        client_p        if not NULL, is the *LOCAL* client associated with
 *                the error.
 *
 * Cannot use perror() within daemon. stderr is closed in
 * ircd and cannot be used. And, worse yet, it might have
 * been reassigned to a normal connection...
 * 
 * Actually stderr is still there IFF ircd was run with -s --Rodder
 */

void
report_error(const char *text, const char *who, const char *wholog, int error)
{
	who = (who) ? who : "";
	wholog = (wholog) ? wholog : "";

	sendto_realops_snomask(SNO_DEBUG, L_ALL, text, who, strerror(error));

	ilog(L_IOERROR, text, wholog, strerror(error));
}
