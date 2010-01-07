/* src/rsdb_sqlite.h
 *   Contains the code for the sqlite database backend.
 *
 * Copyright (C) 2003-2006 Lee Hardy <leeh@leeh.co.uk>
 * Copyright (C) 2003-2006 ircd-ratbox development team
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
 * $Id: rsdb_sqlite3.c 26182 2008-11-11 02:52:41Z androsyn $
 */
#include "stdinc.h"
#include "rsdb.h"

#include <sqlite3.h>

struct sqlite3 *rb_bandb;

rsdb_error_cb *error_cb;

static void
mlog(const char *errstr, ...)
{
	if(error_cb != NULL)
	{
		char buf[256];
		va_list ap;
		va_start(ap, errstr);
		rb_vsnprintf(buf, sizeof(buf), errstr, ap);
		va_end(ap);
		error_cb(buf);
	}
	else
		exit(1);
}

int
rsdb_init(rsdb_error_cb * ecb)
{
	const char *bandb_dpath;
	char dbpath[PATH_MAX];
	char errbuf[128];
	error_cb = ecb;

	/* try a path from the environment first, useful for basedir overrides */
	bandb_dpath = getenv("BANDB_DPATH");

	if(bandb_dpath != NULL)
		rb_snprintf(dbpath, sizeof(dbpath), "%s/etc/ban.db", bandb_dpath);
	else
		rb_strlcpy(dbpath, DBPATH, sizeof(dbpath));
	
	if(sqlite3_open(dbpath, &rb_bandb) != SQLITE_OK)
	{
		rb_snprintf(errbuf, sizeof(errbuf), "Unable to open sqlite database: %s",
			    sqlite3_errmsg(rb_bandb));
		mlog(errbuf);
		return -1;
	}
	if(access(dbpath, W_OK))
	{
		rb_snprintf(errbuf, sizeof(errbuf),  "Unable to open sqlite database for write: %s", strerror(errno));
		mlog(errbuf);
		return -1;			
	}
	return 0;
}

void
rsdb_shutdown(void)
{
	if(rb_bandb)
		sqlite3_close(rb_bandb);
}

const char *
rsdb_quote(const char *src)
{
	static char buf[BUFSIZE * 4];
	char *p = buf;

	/* cheap and dirty length check.. */
	if(strlen(src) >= (sizeof(buf) / 2))
		return NULL;

	while(*src)
	{
		if(*src == '\'')
			*p++ = '\'';

		*p++ = *src++;
	}

	*p = '\0';
	return buf;
}

static int
rsdb_callback_func(void *cbfunc, int argc, char **argv, char **colnames)
{
	rsdb_callback cb = (rsdb_callback)((uintptr_t)cbfunc);
	(cb) (argc, (const char **)argv);
	return 0;
}

void
rsdb_exec(rsdb_callback cb, const char *format, ...)
{
	static char buf[BUFSIZE * 4];
	va_list args;
	char *errmsg;
	unsigned int i;
	int j;

	va_start(args, format);
	i = rs_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	if(i >= sizeof(buf))
	{
		mlog("fatal error: length problem with compiling sql");
	}

	if((i = sqlite3_exec(rb_bandb, buf, (cb ? rsdb_callback_func : NULL), (void *)((uintptr_t)cb), &errmsg)))
	{
		switch (i)
		{
		case SQLITE_BUSY:
			for(j = 0; j < 5; j++)
			{
				rb_sleep(0, 500000);
				if(!sqlite3_exec
				   (rb_bandb, buf, (cb ? rsdb_callback_func : NULL), (void *)((uintptr_t)cb), &errmsg))
					return;
			}

			/* failed, fall through to default */
			mlog("fatal error: problem with db file: %s", errmsg);
			break;

		default:
			mlog("fatal error: problem with db file: %s", errmsg);
			break;
		}
	}
}

void
rsdb_exec_fetch(struct rsdb_table *table, const char *format, ...)
{
	static char buf[BUFSIZE * 4];
	va_list args;
	char *errmsg;
	char **data;
	int pos;
	unsigned int retval;
	int i, j;

	va_start(args, format);
	retval = rs_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	if(retval >= sizeof(buf))
	{
		mlog("fatal error: length problem with compiling sql");
	}

	if((retval =
	    sqlite3_get_table(rb_bandb, buf, &data, &table->row_count, &table->col_count, &errmsg)))
	{
		int success = 0;

		switch (retval)
		{
		case SQLITE_BUSY:
			for(i = 0; i < 5; i++)
			{
				rb_sleep(0, 500000);
				if(!sqlite3_get_table
				   (rb_bandb, buf, &data, &table->row_count, &table->col_count,
				    &errmsg))
				{
					success++;
					break;
				}
			}

			if(success)
				break;

			mlog("fatal error: problem with db file: %s", errmsg);
			break;

		default:
			mlog("fatal error: problem with db file: %s", errmsg);
			break;
		}
	}

	/* we need to be able to free data afterward */
	table->arg = data;

	if(table->row_count == 0)
	{
		table->row = NULL;
		return;
	}

	/* sqlite puts the column names as the first row */
	pos = table->col_count;
	table->row = rb_malloc(sizeof(char **) * table->row_count);
	for(i = 0; i < table->row_count; i++)
	{
		table->row[i] = rb_malloc(sizeof(char *) * table->col_count);

		for(j = 0; j < table->col_count; j++)
		{
			table->row[i][j] = data[pos++];
		}
	}
}

void
rsdb_exec_fetch_end(struct rsdb_table *table)
{
	int i;

	for(i = 0; i < table->row_count; i++)
	{
		rb_free(table->row[i]);
	}
	rb_free(table->row);

	sqlite3_free_table((char **)table->arg);
}

void
rsdb_transaction(rsdb_transtype type)
{
	if(type == RSDB_TRANS_START)
		rsdb_exec(NULL, "BEGIN TRANSACTION");
	else if(type == RSDB_TRANS_END)
		rsdb_exec(NULL, "COMMIT TRANSACTION");
}
