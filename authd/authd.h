/* authd/dns.h - header for authd DNS functions
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

#ifndef _AUTHD_H
#define _AUTHD_H

#include "stdinc.h"
#include "rb_lib.h"
#include "rb_dictionary.h"

#include "setup.h"
#include "ircd_defs.h"

typedef enum exit_reasons
{
	EX_ERROR = 1,
	EX_DNS_ERROR = 2,
	EX_PROVIDER_ERROR = 3,
} exit_reasons;

typedef void (*provider_opts_handler_t)(const char *, int, const char **);

struct auth_opts_handler
{
	const char *option;
	int min_parc;
	provider_opts_handler_t handler;
};

extern rb_helper *authd_helper;

typedef void (*authd_cmd_handler)(int parc, char *parv[]);
typedef void (*authd_stat_handler)(uint32_t rid, const char letter);
typedef void (*authd_reload_handler)(const char letter);

extern authd_cmd_handler authd_cmd_handlers[256];
extern authd_stat_handler authd_stat_handlers[256];
extern authd_reload_handler authd_reload_handlers[256];

extern rb_dictionary *authd_option_handlers;

#endif
