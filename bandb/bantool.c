/**
 *  ircd-ratbox: A slightly useful ircd.
 *  bantool.c: The ircd-ratbox database managment tool.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2008 ircd-ratbox development team
 *  Copyright (C) 2008 Daniel J Reidy <dubkat@gmail.com>
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
 *
 *  $Id: bantool.c 26164 2008-10-26 19:52:43Z androsyn $
 *
 *
 * The following server admins have either contributed various configs to test against,
 * or helped with debugging and feature requests. Many thanks to them.
 * stevoo / efnet.port80.se
 * AndroSyn / irc2.choopa.net, irc.igs.ca
 * Salvation / irc.blessed.net
 * JamesOff / efnet.demon.co.uk
 *
 * Thanks to AndroSyn for challenging me to learn C on the fly :)
 * BUGS Direct Question, Bug Reports, and Feature Requests to #ratbox on EFnet.
 * BUGS Complaints >/dev/null
 *
 */

#define _XOPEN_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "stdinc.h"
#include "common.h"
#include "rsdb.h"

#define EmptyString(x) ((x == NULL) || (*(x) == '\0'))
#define CheckEmpty(x) EmptyString(x) ? "" : x

#define BT_VERSION "0.4.1"

typedef enum
{
	BANDB_KLINE,
	BANDB_KLINE_PERM,
	BANDB_DLINE,
	BANDB_DLINE_PERM,
	BANDB_XLINE,
	BANDB_XLINE_PERM,
	BANDB_RESV,
	BANDB_RESV_PERM,
	LAST_BANDB_TYPE
} bandb_type;


static char bandb_letter[LAST_BANDB_TYPE] = {
	'K', 'K', 'D', 'D', 'X', 'X', 'R', 'R'
};

static const char *bandb_table[LAST_BANDB_TYPE] = {
	"kline", "kline", "dline", "dline", "xline", "xline", "resv", "resv"
};

static const char *bandb_suffix[LAST_BANDB_TYPE] = {
	"", ".perm",
	"", ".perm",
	"", ".perm",
	"", ".perm"
};

static char me[PATH_MAX];

/* *INDENT-OFF* */
/* report counters */
struct counter
{
	unsigned int klines;
	unsigned int dlines;
	unsigned int xlines;
	unsigned int resvs;
	unsigned int error;
} count = {0, 0, 0, 0, 0};

/* flags set by command line options */
struct flags
{
	int none;
	int export;
	int import;
	int verify;
	int vacuum;
	int pretend;
	int verbose;
	int wipe;
	int dupes_ok;
} flag = {YES, NO, NO, NO, NO, NO, NO, NO, NO};
/* *INDENT-ON* */

static int table_has_rows(const char *table);
static int table_exists(const char *table);

static const char *clean_gecos_field(const char *gecos);
static char *bt_smalldate(const char *string);
static char *getfield(char *newline);
static char *strip_quotes(const char *string);
static char *mangle_reason(const char *string);
static char *escape_quotes(const char *string);

static void db_error_cb(const char *errstr);
static void db_reclaim_slack(void);
static void export_config(const char *conf, int id);
static void import_config(const char *conf, int id);
static void check_schema(void);
static void print_help(int i_exit);
static void wipe_schema(void);
static void drop_dupes(const char *user, const char *host, const char *t);

/**
 *  swing your pants 
 */
int
main(int argc, char *argv[])
{
	char etc[PATH_MAX];
	char conf[PATH_MAX];
	int opt;
	int i;

	rb_strlcpy(me, argv[0], sizeof(me));

	while((opt = getopt(argc, argv, "hieuspvwd")) != -1)
	{
		switch (opt)
		{
		case 'h':
			print_help(EXIT_SUCCESS);
			break;
		case 'i':
			flag.none = NO;
			flag.import = YES;
			break;
		case 'e':
			flag.none = NO;
			flag.export = YES;
			break;
		case 'u':
			flag.none = NO;
			flag.verify = YES;
			break;
		case 's':
			flag.none = NO;
			flag.vacuum = YES;
			break;
		case 'p':
			flag.pretend = YES;
			break;
		case 'v':
			flag.verbose = YES;
			break;
		case 'w':
			flag.wipe = YES;
			break;
		case 'd':
			flag.dupes_ok = YES;
			break;
		default:	/* '?' */
			print_help(EXIT_FAILURE);
		}
	}

	/* they should really read the help. */
	if(flag.none)
		print_help(EXIT_FAILURE);

	if((flag.import && flag.export) || (flag.export && flag.wipe)
	   || (flag.verify && flag.pretend) || (flag.export && flag.pretend))
	{
		fprintf(stderr, "* Error: Conflicting flags.\n");
		if(flag.export && flag.pretend)
			fprintf(stderr, "* There is nothing to 'pretend' when exporting.\n");

		fprintf(stderr, "* For an explination of commands, run: %s -h\n", me);
		exit(EXIT_FAILURE);
	}

	if(argv[optind] != NULL)
		rb_strlcpy(etc, argv[optind], sizeof(etc));
	else
		rb_strlcpy(etc, ETCPATH, sizeof(ETCPATH));

	fprintf(stdout,
		"* ircd-ratbox bantool v.%s ($Id: bantool.c 26164 2008-10-26 19:52:43Z androsyn $)\n",
		BT_VERSION);

	if(flag.pretend == NO)
	{
		if(rsdb_init(db_error_cb) == -1)
		{
			fprintf(stderr, "* Error: Unable to open database\n");
			exit(EXIT_FAILURE);
		}
		check_schema();

		if(flag.vacuum)
			db_reclaim_slack();

		if(flag.import && flag.wipe)
		{
			flag.dupes_ok = YES;	/* dont check for dupes if we are wiping the db clean */
			for(i = 0; i < 3; i++)
				fprintf(stdout,
					"* WARNING: YOU ARE ABOUT TO WIPE YOUR DATABASE!\n");

			fprintf(stdout, "* Press ^C to abort! ");
			fflush(stdout);
			rb_sleep(10, 0);
			fprintf(stdout, "Carrying on...\n");
			wipe_schema();
		}
	}
	if(flag.verbose && flag.dupes_ok == YES)
		fprintf(stdout, "* Allowing duplicate bans...\n");

	/* checking for our files to import or export */
	for(i = 0; i < LAST_BANDB_TYPE; i++)
	{
		rb_snprintf(conf, sizeof(conf), "%s/%s.conf%s",
			    etc, bandb_table[i], bandb_suffix[i]);

		if(flag.import && flag.pretend == NO)
			rsdb_transaction(RSDB_TRANS_START);

		if(flag.import)
			import_config(conf, i);

		if(flag.export)
			export_config(conf, i);

		if(flag.import && flag.pretend == NO)
			rsdb_transaction(RSDB_TRANS_END);
	}

	if(flag.import)
	{
		if(count.error && flag.verbose)
			fprintf(stderr, "* I was unable to locate %i config files to import.\n",
				count.error);

		fprintf(stdout, "* Import Stats: Klines: %i, Dlines: %i, Xlines: %i, Resvs: %i \n",
			count.klines, count.dlines, count.xlines, count.resvs);

		fprintf(stdout,
			"*\n* If your IRC server is currently running, newly imported bans \n* will not take effect until you issue the command: /quote rehash bans\n");

		if(flag.pretend)
			fprintf(stdout,
				"* Pretend mode engaged. Nothing was actually entered into the database.\n");
	}

	return 0;
}


/**
 * export the database to old-style flat files
 */
static void
export_config(const char *conf, int id)
{
	struct rsdb_table table;
	static char sql[BUFSIZE * 2];
	static char buf[512];
	FILE *fd = NULL;
	int j;

	/* for sanity sake */
	const int mask1 = 0;
	const int mask2 = 1;
	const int reason = 2;
	const int oper = 3;
	const int ts = 4;
	/* const int perm = 5; */

	if(!table_has_rows(bandb_table[id]))
		return;

	if(strstr(conf, ".perm") != 0)
		rb_snprintf(sql, sizeof(sql),
			    "SELECT DISTINCT mask1,mask2,reason,oper,time FROM %s WHERE perm = 1 ORDER BY time",
			    bandb_table[id]);
	else
		rb_snprintf(sql, sizeof(sql),
			    "SELECT DISTINCT mask1,mask2,reason,oper,time FROM %s WHERE perm = 0 ORDER BY time",
			    bandb_table[id]);

	rsdb_exec_fetch(&table, sql);
	if(table.row_count <= 0)
	{
		rsdb_exec_fetch_end(&table);
		return;
	}

	if(flag.verbose)
		fprintf(stdout, "* checking for %s: ", conf);	/* debug  */

	/* open config for reading, or skip to the next */
	if(!(fd = fopen(conf, "w")))
	{
		if(flag.verbose)
			fprintf(stdout, "\tmissing.\n");
		count.error++;
		return;
	}

	for(j = 0; j < table.row_count; j++)
	{
		switch (id)
		{
		case BANDB_DLINE:
		case BANDB_DLINE_PERM:
			rb_snprintf(buf, sizeof(buf),
				    "\"%s\",\"%s\",\"\",\"%s\",\"%s\",%s\n",
				    table.row[j][mask1],
				    mangle_reason(table.row[j][reason]),
				    bt_smalldate(table.row[j][ts]),
				    table.row[j][oper], table.row[j][ts]);
			break;

		case BANDB_XLINE:
		case BANDB_XLINE_PERM:
			rb_snprintf(buf, sizeof(buf),
				    "\"%s\",\"0\",\"%s\",\"%s\",%s\n",
				    escape_quotes(table.row[j][mask1]),
				    mangle_reason(table.row[j][reason]),
				    table.row[j][oper], table.row[j][ts]);
			break;

		case BANDB_RESV:
		case BANDB_RESV_PERM:
			rb_snprintf(buf, sizeof(buf),
				    "\"%s\",\"%s\",\"%s\",%s\n",
				    table.row[j][mask1],
				    mangle_reason(table.row[j][reason]),
				    table.row[j][oper], table.row[j][ts]);
			break;


		default:	/* Klines */
			rb_snprintf(buf, sizeof(buf),
				    "\"%s\",\"%s\",\"%s\",\"\",\"%s\",\"%s\",%s\n",
				    table.row[j][mask1], table.row[j][mask2],
				    mangle_reason(table.row[j][reason]),
				    bt_smalldate(table.row[j][ts]), table.row[j][oper],
				    table.row[j][ts]);
			break;
		}

		fprintf(fd, "%s", buf);
	}

	rsdb_exec_fetch_end(&table);
	if(flag.verbose)
		fprintf(stdout, "\twritten.\n");
	fclose(fd);
}

/**
 * attempt to condense the individual conf functions into one
 */
static void
import_config(const char *conf, int id)
{
	FILE *fd;

	char line[BUFSIZE];
	char *p;
	int i = 0;

	char f_perm = 0;
	const char *f_mask1 = NULL;
	const char *f_mask2 = NULL;
	const char *f_oper = NULL;
	const char *f_time = NULL;
	const char *f_reason = NULL;
	const char *f_oreason = NULL;
	char newreason[REASONLEN];

	if(flag.verbose)
		fprintf(stdout, "* checking for %s: ", conf);	/* debug  */

	/* open config for reading, or skip to the next */
	if(!(fd = fopen(conf, "r")))
	{
		if(flag.verbose)
			fprintf(stdout, "%*s", strlen(bandb_suffix[id]) > 0 ? 10 : 15,
				"missing.\n");
		count.error++;
		return;
	}

	if(strstr(conf, ".perm") != 0)
		f_perm = 1;


	/* xline
	 * "SYSTEM","0","banned","stevoo!stevoo@efnet.port80.se{stevoo}",1111080437
	 * resv
	 * "OseK","banned nickname","stevoo!stevoo@efnet.port80.se{stevoo}",1111031619
	 * dline
	 * "194.158.192.0/19","laptop scammers","","2005/3/17 05.33","stevoo!stevoo@efnet.port80.se{stevoo}",1111033988
	 */
	while(fgets(line, sizeof(line), fd))
	{
		if((p = strpbrk(line, "\r\n")) != NULL)
			*p = '\0';

		if((*line == '\0') || (*line == '#'))
			continue;

		/* mask1 */
		f_mask1 = getfield(line);

		if(EmptyString(f_mask1))
			continue;

		/* mask2 */
		switch (id)
		{
		case BANDB_XLINE:
		case BANDB_XLINE_PERM:
			f_mask1 = escape_quotes(clean_gecos_field(f_mask1));
			getfield(NULL);	/* empty field */
			break;

		case BANDB_RESV:
		case BANDB_RESV_PERM:
		case BANDB_DLINE:
		case BANDB_DLINE_PERM:
			break;

		default:
			f_mask2 = getfield(NULL);
			if(EmptyString(f_mask2))
				continue;
			break;
		}

		/* reason */
		f_reason = getfield(NULL);
		if(EmptyString(f_reason))
			continue;

		/* oper comment */
		switch (id)
		{
		case BANDB_KLINE:
		case BANDB_KLINE_PERM:
		case BANDB_DLINE:
		case BANDB_DLINE_PERM:
			f_oreason = getfield(NULL);
			getfield(NULL);
			break;

		default:
			break;
		}

		f_oper = getfield(NULL);
		f_time = strip_quotes(f_oper + strlen(f_oper) + 2);
		if(EmptyString(f_oper))
			f_oper = "unknown";

		/* meh */
		if(id == BANDB_KLINE || id == BANDB_KLINE_PERM)
		{
			if(strstr(f_mask1, "!") != NULL)
			{
				fprintf(stderr,
					"* SKIPPING INVALID KLINE %s@%s set by %s\n",
					f_mask1, f_mask2, f_oper);
				fprintf(stderr, "  You may wish to re-apply it correctly.\n");
				continue;
			}
		}

		/* append operreason_field to reason_field */
		if(!EmptyString(f_oreason))
			rb_snprintf(newreason, sizeof(newreason), "%s | %s", f_reason, f_oreason);
		else
			rb_snprintf(newreason, sizeof(newreason), "%s", f_reason);

		if(flag.pretend == NO)
		{
			if(flag.dupes_ok == NO)
				drop_dupes(f_mask1, f_mask2, bandb_table[id]);

			rsdb_exec(NULL,
				  "INSERT INTO %s (mask1, mask2, oper, time, perm, reason) VALUES('%Q','%Q','%Q','%Q','%d','%Q')",
				  bandb_table[id], f_mask1, f_mask2, f_oper, f_time, f_perm,
				  newreason);
		}

		if(flag.pretend && flag.verbose)
			fprintf(stdout,
				"%s: perm(%d) mask1(%s) mask2(%s) oper(%s) reason(%s) time(%s)\n",
				bandb_table[id], f_perm, f_mask1, f_mask2, f_oper, newreason,
				f_time);

		i++;
	}

	switch (bandb_letter[id])
	{
	case 'K':
		count.klines += i;
		break;
	case 'D':
		count.dlines += i;
		break;
	case 'X':
		count.xlines += i;
		break;
	case 'R':
		count.resvs += i;
		break;
	default:
		break;
	}

	if(flag.verbose)
		fprintf(stdout, "%*s\n", strlen(bandb_suffix[id]) > 0 ? 10 : 15, "imported.");

	return;
}

/**
 * getfield
 *
 * inputs	- input buffer
 * output	- next field
 * side effects	- field breakup for ircd.conf file.
 */
char *
getfield(char *newline)
{
	static char *line = NULL;
	char *end, *field;

	if(newline != NULL)
		line = newline;

	if(line == NULL)
		return (NULL);

	field = line;

	/* XXX make this skip to first " if present */
	if(*field == '"')
		field++;
	else
		return (NULL);	/* mal-formed field */

	end = strchr(line, ',');

	while(1)
	{
		/* no trailing , - last field */
		if(end == NULL)
		{
			end = line + strlen(line);
			line = NULL;

			if(*end == '"')
			{
				*end = '\0';
				return field;
			}
			else
				return NULL;
		}
		else
		{
			/* look for a ", to mark the end of a field.. */
			if(*(end - 1) == '"')
			{
				line = end + 1;
				end--;
				*end = '\0';
				return field;
			}

			/* search for the next ',' */
			end++;
			end = strchr(end, ',');
		}
	}

	return NULL;
}

/**
 * strip away "quotes" from around strings
 */
static char *
strip_quotes(const char *string)
{
	static char buf[14];	/* int(11) + 2 + \0 */
	char *str = buf;

	if(string == NULL)
		return NULL;

	while(*string)
	{
		if(*string != '"')
		{
			*str++ = *string;
		}
		string++;
	}
	*str = '\0';
	return buf;
}

/**
 * escape quotes in a string
 */
static char *
escape_quotes(const char *string)
{
	static char buf[BUFSIZE * 2];
	char *str = buf;

	if(string == NULL)
		return NULL;

	while(*string)
	{
		if(*string == '"')
		{
			*str++ = '\\';
			*str++ = '"';
		}
		else
		{
			*str++ = *string;
		}
		string++;
	}
	*str = '\0';
	return buf;
}


static char *
mangle_reason(const char *string)
{
	static char buf[BUFSIZE * 2];
	char *str = buf;

	if(string == NULL)
		return NULL;

	while(*string)
	{
		switch (*string)
		{
		case '"':
			*str = '\'';
			break;
		case ':':
			*str = ' ';
			break;
		default:
			*str = *string;
		}
		string++;
		str++;

	}
	*str = '\0';
	return buf;
}


/**
 * change spaces to \s in gecos field
 */
static const char *
clean_gecos_field(const char *gecos)
{
	static char buf[BUFSIZE * 2];
	char *str = buf;

	if(gecos == NULL)
		return NULL;

	while(*gecos)
	{
		if(*gecos == ' ')
		{
			*str++ = '\\';
			*str++ = 's';
		}
		else
			*str++ = *gecos;
		gecos++;
	}
	*str = '\0';
	return buf;
}

/**
 * verify the database integrity, and if necessary create apropriate tables
 */
static void
check_schema(void)
{
	int i, j;
	char type[8];		/* longest string is 'INTEGER\0' */

	if(flag.verify || flag.verbose)
		fprintf(stdout, "* Verifying database.\n");

	const char *columns[] = {
		"perm",
		"mask1",
		"mask2",
		"oper",
		"time",
		"reason",
		NULL
	};

	for(i = 0; i < LAST_BANDB_TYPE; i++)
	{
		if(!table_exists(bandb_table[i]))
		{
			rsdb_exec(NULL,
				  "CREATE TABLE %s (mask1 TEXT, mask2 TEXT, oper TEXT, time INTEGER, perm INTEGER, reason TEXT)",
				  bandb_table[i]);
		}

		/*
		 * i can't think of any better way to do this, other then attempt to
		 * force the creation of column that may, or may not already exist. --dubkat
		 */
		else
		{
			for(j = 0; columns[j] != NULL; j++)
			{
				if(!strcmp(columns[j], "time") && !strcmp(columns[j], "perm"))
					rb_strlcpy(type, "INTEGER", sizeof(type));
				else
					rb_strlcpy(type, "TEXT", sizeof(type));

				/* attempt to add a column with extreme prejudice, errors are ignored */
				rsdb_exec(NULL, "ALTER TABLE %s ADD COLUMN %s %s", bandb_table[i],
					  columns[j], type);
			}
		}

		i++;		/* skip over .perm */
	}
}

static void
db_reclaim_slack(void)
{
	fprintf(stdout, "* Reclaiming free space.\n");
	rsdb_exec(NULL, "VACUUM");
}


/**
 * check that appropriate tables exist.
 */
static int
table_exists(const char *dbtab)
{
	struct rsdb_table table;
	rsdb_exec_fetch(&table, "SELECT name FROM sqlite_master WHERE type='table' AND name='%s'",
			dbtab);
	rsdb_exec_fetch_end(&table);
	return table.row_count;
}

/**
 * check that there are actual entries in a table
 */
static int
table_has_rows(const char *dbtab)
{
	struct rsdb_table table;
	rsdb_exec_fetch(&table, "SELECT * FROM %s", dbtab);
	rsdb_exec_fetch_end(&table);
	return table.row_count;
}

/**
 * completly wipes out an existing ban.db of all entries.
 */
static void
wipe_schema(void)
{
	int i;
	rsdb_transaction(RSDB_TRANS_START);
	for(i = 0; i < LAST_BANDB_TYPE; i++)
	{
		rsdb_exec(NULL, "DROP TABLE %s", bandb_table[i]);
		i++;		/* double increment to skip over .perm */
	}
	rsdb_transaction(RSDB_TRANS_END);

	check_schema();
}

/**
 * remove pre-existing duplicate bans from the database.
 * we favor the new, imported ban over the one in the database
 */
void
drop_dupes(const char *user, const char *host, const char *t)
{
	rsdb_exec(NULL, "DELETE FROM %s WHERE mask1='%Q' AND mask2='%Q'", t, user, host);
}

static void
db_error_cb(const char *errstr)
{
	return;
}


/**
 * convert unix timestamp to human readable (small) date
 */
static char *
bt_smalldate(const char *string)
{
	static char buf[MAX_DATE_STRING];
	struct tm *lt;
	time_t t;
	t = strtol(string, NULL, 10);
	lt = gmtime(&t);
	if(lt == NULL)
		return NULL;
	rb_snprintf(buf, sizeof(buf), "%d/%d/%d %02d.%02d",
		    lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min);
	return buf;
}

/**
 * you are here ->.
 */
void
print_help(int i_exit)
{
	fprintf(stderr, "bantool v.%s - the ircd-ratbox database tool.\n", BT_VERSION);
	fprintf(stderr, "Copyright (C) 2008 Daniel J Reidy <dubkat@gmail.com>\n");
	fprintf(stderr, "$Id: bantool.c 26164 2008-10-26 19:52:43Z androsyn $\n\n");
	fprintf(stderr, "This program is distributed in the hope that it will be useful,\n"
		"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		"GNU General Public License for more details.\n\n");

	fprintf(stderr, "Usage: %s <-i|-e> [-p] [-v] [-h] [-d] [-w] [path]\n", me);
	fprintf(stderr, "       -h : Display some slightly useful help.\n");
	fprintf(stderr, "       -i : Actually import configs into your database.\n");
	fprintf(stderr, "       -e : Export your database to old-style flat files.\n");
	fprintf(stderr,
		"            This is suitable for redistrubuting your banlists, or creating backups.\n");
	fprintf(stderr, "       -s : Reclaim empty slack space the database may be taking up.\n");
	fprintf(stderr, "       -u : Update the database tables to support any new features.\n");
	fprintf(stderr,
		"            This is automaticlly done if you are importing or exporting\n");
	fprintf(stderr, "            but should be run whenever you upgrade the ircd.\n");
	fprintf(stderr,
		"       -p : pretend, checks for the configs, and parses them, then tells you some data...\n");
	fprintf(stderr, "            but does not touch your database.\n");
	fprintf(stderr,
		"       -v : Be verbose... and it *is* very verbose! (intended for debugging)\n");
	fprintf(stderr, "       -d : Enable checking for redunant entries.\n");
	fprintf(stderr, "       -w : Completly wipe your database clean. May be used with -i \n");
	fprintf(stderr,
		"     path : An optional directory containing old ratbox configs for import, or export.\n");
	fprintf(stderr, "            If not specified, it looks in PREFIX/etc.\n");
	exit(i_exit);
}
