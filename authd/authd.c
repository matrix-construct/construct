/* authd/authd.c - main code for authd
 * Copyright (c) 2016 William Pitcock <nenolod@dereferenced.org>
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

#include "authd.h"
#include "dns.h"
#include "provider.h"
#include "notice.h"

#define MAXPARA 10

static void error_cb(rb_helper *helper) __attribute__((noreturn));
static void handle_reload(int parc, char *parv[]);
static void handle_stat(int parc, char *parv[]);
static void handle_options(int parc, char *parv[]);

rb_helper *authd_helper = NULL;
authd_cmd_handler authd_cmd_handlers[256] = {
	['C'] = handle_new_connection,
	['D'] = handle_resolve_dns,
	['E'] = handle_cancel_connection,
	['O'] = handle_options,
	['R'] = handle_reload,
	['S'] = handle_stat,
};

authd_stat_handler authd_stat_handlers[256] = {
	['D'] = enumerate_nameservers,
};

authd_reload_handler authd_reload_handlers[256] = {
	['D'] = reload_nameservers,
};

rb_dictionary *authd_option_handlers;

static void
handle_stat(int parc, char *parv[])
{
	authd_stat_handler handler;
	unsigned long long rid;

	if(parc < 3)
	{
		warn_opers(L_CRIT, "BUG: handle_stat received too few parameters (at least 3 expected, got %d)", parc);
		return;
	}

	if((rid = strtoull(parv[1], NULL, 16)) > UINT32_MAX)
	{
		warn_opers(L_CRIT, "BUG: handle_stat got a rid that was too large: %s", parv[1]);
		return;
	}

	if (!(handler = authd_stat_handlers[(unsigned char)parv[2][0]]))
		return;

	handler((uint32_t)rid, parv[2][0]);
}

static void
handle_options(int parc, char *parv[])
{
	struct auth_opts_handler *handler;

	if(parc < 2)
	{
		warn_opers(L_CRIT, "BUG: handle_options received too few parameters (at least 2 expected, got %d)", parc);
		return;
	}

	if((handler = rb_dictionary_retrieve(authd_option_handlers, parv[1])) == NULL)
	{
		warn_opers(L_CRIT, "BUG: handle_options got a bad option type %s", parv[1]);
		return;
	}

	if((parc - 2) < handler->min_parc)
	{
		warn_opers(L_CRIT, "BUG: handle_options received too few parameters (at least %d expected, got %d)", handler->min_parc, parc);
		return;
	}

	handler->handler(parv[1], parc - 2, (const char **)&parv[2]);
}

static void
handle_reload(int parc, char *parv[])
{
	authd_reload_handler handler;

	if(parc <= 2)
	{
		/* Reload all handlers */
		for(size_t i = 0; i < 256; i++)
		{
			if ((handler = authd_reload_handlers[(unsigned char) i]) != NULL)
				handler('\0');
		}

		return;
	}

	if (!(handler = authd_reload_handlers[(unsigned char)parv[1][0]]))
		return;

	handler(parv[1][0]);
}

static void
parse_request(rb_helper *helper)
{
	static char *parv[MAXPARA + 1];
	static char readbuf[READBUF_SIZE];
	int parc;
	int len;
	authd_cmd_handler handler;

	while((len = rb_helper_read(helper, readbuf, sizeof(readbuf))) > 0)
	{
		parc = rb_string_to_array(readbuf, parv, MAXPARA);

		if(parc < 1)
			continue;

		handler = authd_cmd_handlers[(unsigned char)parv[0][0]];
		if (handler != NULL)
			handler(parc, parv);
	}
}

static void
error_cb(rb_helper *helper)
{
	exit(EX_ERROR);
}

#ifndef _WIN32
static void
dummy_handler(int sig)
{
	return;
}
#endif

static void
setup_signals(void)
{
#ifndef _WIN32
	struct sigaction act;

	act.sa_flags = 0;
	act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGPIPE);
	sigaddset(&act.sa_mask, SIGALRM);
#ifdef SIGTRAP
	sigaddset(&act.sa_mask, SIGTRAP);
#endif

#ifdef SIGWINCH
	sigaddset(&act.sa_mask, SIGWINCH);
	sigaction(SIGWINCH, &act, 0);
#endif
	sigaction(SIGPIPE, &act, 0);
#ifdef SIGTRAP
	sigaction(SIGTRAP, &act, 0);
#endif

	act.sa_handler = dummy_handler;
	sigaction(SIGALRM, &act, 0);
#endif
}

int
main(int argc, char *argv[])
{
	setup_signals();

	authd_helper = rb_helper_child(parse_request, error_cb, NULL, NULL, NULL, 256, 256, 256);	/* XXX fix me */
	if(authd_helper == NULL)
	{
		fprintf(stderr, "authd is not meant to be invoked by end users\n");
		exit(EX_ERROR);
	}

	rb_set_time();
	setup_signals();

	authd_option_handlers = rb_dictionary_create("authd options handlers", rb_strcasecmp);

	init_resolver();
	init_providers();
	rb_init_prng(NULL, RB_PRNG_DEFAULT);

	rb_helper_loop(authd_helper, 0);

	/*
	 * XXX this function will never be called from here -- is it necessary?
	 */
	destroy_providers();

	return 0;
}
