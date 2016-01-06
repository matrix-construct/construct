/*
 *  ircd-ratbox: A slightly useful ircd.
 *  getopt.c: Uses getopt to fetch the command line options.
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
 *
 *  $Id: getopt.c 6 2005-09-10 01:02:21Z nenolod $
 */

#include "stdinc.h"

#include "ircd_getopt.h"

# define OPTCHAR '-'

void
parseargs(int *argc, char ***argv, struct lgetopt *opts)
{
	int i;
	char *progname = (*argv)[0];

	/* loop through each argument */
	for (;;)
	{
		int found = 0;

		(*argc)--;
		(*argv)++;

		if(*argc < 1)
		{
			return;
		}

		/* check if it *is* an arg.. */
		if((*argv)[0][0] != OPTCHAR)
		{
			return;
		}

		(*argv)[0]++;

		/* search through our argument list, and see if it matches */
		for (i = 0; opts[i].opt; i++)
		{
			if(!strcmp(opts[i].opt, (*argv)[0]))
			{
				/* found our argument */
				found = 1;

				switch (opts[i].argtype)
				{
				case YESNO:
					*((int *) opts[i].argloc) = 1;
					break;
				case INTEGER:
					if(*argc < 2)
					{
						fprintf(stderr,
							"Error: option '%c%s' requires an argument\n",
							OPTCHAR, opts[i].opt);
						usage((*argv)[0]);
					}

					*((int *) opts[i].argloc) = atoi((*argv)[1]);

					(*argc)--;
					(*argv)++;
					break;
				case STRING:
					if(*argc < 2)
					{
						fprintf(stderr,
							"error: option '%c%s' requires an argument\n",
							OPTCHAR, opts[i].opt);
						usage(progname);
					}

					*((char **) opts[i].argloc) =
						malloc(strlen((*argv)[1]) + 1);
					strcpy(*((char **) opts[i].argloc), (*argv)[1]);

					(*argc)--;
					(*argv)++;
					break;

				case USAGE:
					usage(progname);
				 /*NOTREACHED*/ default:
					fprintf(stderr,
						"Error: internal error in parseargs() at %s:%d\n",
						__FILE__, __LINE__);
					exit(EXIT_FAILURE);
				}
			}
		}
		if(!found)
		{
			fprintf(stderr, "error: unknown argument '%c%s'\n", OPTCHAR, (*argv)[0]);
			usage(progname);
		}
	}
}

void
usage(char *name)
{
	int i = 0;

	fprintf(stderr, "Usage: %s [options]\n", name);
	fprintf(stderr, "Where valid options are:\n");

	for (i = 0; myopts[i].opt; i++)
	{
		fprintf(stderr, "\t%c%-10s %-20s%s\n", OPTCHAR,
			myopts[i].opt, (myopts[i].argtype == YESNO
					|| myopts[i].argtype ==
					USAGE) ? "" : myopts[i].argtype ==
			INTEGER ? "<number>" : "<string>", myopts[i].desc);
	}

	exit(EXIT_FAILURE);
}
