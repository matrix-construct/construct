/************************************************************************
 *   IRC - Internet Relay Chat, doc/example_module.c
 *   Copyright (C) 2001 Hybrid Development Team
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   $Id: example_module.c 3161 2007-01-25 07:23:01Z nenolod $
 */

/* List of ircd includes from ../include/ */
#include "stdinc.h"
#include "modules.h"
#include "hook.h"
#include "client.h"
#include "ircd.h"
#include "send.h"

/* Declare the void's initially up here, as modules dont have an
 * include file, we will normally have client_p, source_p, parc
 * and parv[] where:
 *
 * client_p == client issuing command
 * source_p == where the command came from
 * parc     == the number of parameters
 * parv     == an array of the parameters
 */

static int munreg_test(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static int mclient_test(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static int mserver_test(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static int mrclient_test(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);
static int moper_test(struct Client *client_p, struct Client *source_p, int parc, const char *parv[]);

/* Show the commands this module can handle in a msgtab
 * and give the msgtab a name, here its test_msgtab
 */

struct Message test_msgtab = {
  "TEST",               /* the /COMMAND you want */
  0,                    /* SET TO ZERO -- number of times command used by clients */
  0,                    /* SET TO ZERO -- number of times command used by clients */
  0,                    /* SET TO ZERO -- number of times command used by clients */
  MFLG_SLOW,            /* ALWAYS SET TO MFLG_SLOW */

  /* the functions to call for each handler.  If not using the generic
   * handlers, the first param is the function to call, the second is the
   * required number of parameters.  NOTE: If you specify a min para of 2,
   * then parv[1] must *also* be non-empty.
   */
  {
    {munreg_test, 0},   /* function call for unregistered clients, 0 parms required */
    {mclient_test, 0},  /* function call for local clients, 0 parms required */
    {mrclient_test, 0}, /* function call for remote clients, 0 parms required */
    {mserver_test, 0},  /* function call for servers, 0 parms required */
    mg_ignore,          /* function call for ENCAP, unused in this test */
    {moper_test, 0}     /* function call for operators, 0 parms required */
  }
};
/*
 * There are also some macros for the above function calls and parameter counts.
 * Here's a list:
 *
 * mg_ignore:       ignore the command when it comes from certain types
 * mg_not_oper:     tell the client it requires being an operator
 * mg_reg:          prevent the client using this if registered
 * mg_unreg:        prevent the client using this if unregistered
 *
 * These macros assume a parameter count of zero; you do not set it.
 * For further details, see include/msg.h
 */


/* The mapi_clist_av1 indicates which commands (struct Message)
 * should be loaded from the module. The list should be terminated
 * by a NULL.
 */
mapi_clist_av1 test_clist[] = { &test_msgtab, NULL };

/* The mapi_hlist_av1 indicates which hook functions we need to be able to
 * call.  We need to declare an integer, then add the name of the hook
 * function to call and a pointer to this integer.  The list should be
 * terminated with NULLs.
 */
int doing_example_hook;
mapi_hlist_av1 test_hlist[] = { 
	{ "doing_example_hook", &doing_example_hook, },
	{ NULL, NULL }
};

/* The mapi_hfn_list_av1 declares the hook functions which other modules can
 * call.  The first parameter is the name of the hook, the second is a void
 * returning function, with arbitrary parameters casted to (hookfn).  This
 * list must be terminated with NULLs.
 */
static void show_example_hook(void *unused);

mapi_hfn_list_av1 test_hfnlist[] = {
	{ "doing_example_hook", (hookfn) show_example_hook },
	{ NULL, NULL }
};

/* Here we tell it what to do when the module is loaded */
static int
modinit(void)
{
	/* Nothing to do for the example module. */
	/* The init function should return -1 on failure,
	   which will cause the module to be unloaded,
	   otherwise 0 to indicate success. */
	return 0;
}

/* here we tell it what to do when the module is unloaded */
static void
moddeinit(void)
{
	/* Again, nothing to do. */
}

/* DECLARE_MODULE_AV1() actually declare the MAPI header. */
DECLARE_MODULE_AV1(
			  /* The first argument is the name */
			  example,
			  /* The second argument is the function to call on load */
			  modinit,
			  /* And the function to call on unload */
			  moddeinit,
			  /* Then the MAPI command list */
			  test_clist,
			  /* Next the hook list, if we have one. */
			  test_hlist,
			  /* Then the hook function list, if we have one */
			  test_hfnlist,
			  /* And finally the version number of this module. */
			  "$Revision: 3161 $");

/* Any of the above arguments can be NULL to indicate they aren't used. */


/*
 * mr_test
 *      parv[1] = parameter
 */

/* Here we have the functions themselves that we declared above,
 * and the fairly normal C coding
 */
static int
munreg_test(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(parc < 2)
	{
		sendto_one_notice(source_p, ":You are unregistered and sent no parameters");
	}
	else
	{
		sendto_one_notice(source_p, ":You are unregistered and sent parameter: %s", parv[1]);
	}

	/* illustration of how to call a hook function */
	call_hook(doing_example_hook, NULL);

	return 0;
}

/*
 * mclient_test
 *      parv[1] = parameter
 */
static int
mclient_test(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(parc < 2)
	{
		sendto_one_notice(source_p, ":You are a normal user, and sent no parameters");
	}
	else
	{
		sendto_one_notice(source_p, ":You are a normal user, and send parameters: %s", parv[1]);
	}

	/* illustration of how to call a hook function */
	call_hook(doing_example_hook, NULL);

	return 0;
}

/*
 * mrclient_test
 *      parv[1] = parameter
 */
static int
mrclient_test(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(parc < 2)
	{
		sendto_one_notice(source_p, ":You are a remote client, and sent no parameters");
	}
	else
	{
		sendto_one_notice(source_p, ":You are a remote client, and sent parameters: %s", parv[1]);
	}
	return 0;
}

/*
 * mserver_test
 *      parv[1] = parameter
 */
static int
mserver_test(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(parc < 2)
	{
		sendto_one_notice(source_p, ":You are a server, and sent no parameters");
	}
	else
	{
		sendto_one_notice(source_p, ":You are a server, and sent parameters: %s", parv[1]);
	}
	return 0;
}

/*
 * moper_test
 *      parv[1] = parameter
 */
static int
moper_test(struct Client *client_p, struct Client *source_p, int parc, const char *parv[])
{
	if(parc < 2)
	{
		sendto_one_notice(source_p, ":You are an operator, and sent no parameters");
	}
	else
	{
		sendto_one_notice(source_p, ":You are an operator, and sent parameters: %s", parv[1]);
	}
	return 0;
}

static void
show_example_hook(void *unused)
{
	sendto_realops_snomask(SNO_GENERAL, L_ALL, "Called example hook!");
}

/* END OF EXAMPLE MODULE */
