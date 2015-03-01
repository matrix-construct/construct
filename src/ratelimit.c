/*
 * charybdis: an advanced ircd
 * ratelimit.c: Per-client ratelimiting for high-bandwidth commands.
 *
 * Copyright (c) 2012 Keith Buck <mr_flea -at- esper.net>
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

#include "stdinc.h"
#include "s_conf.h"
#include "s_stats.h"
#include "ratelimit.h"
#include "s_assert.h"

/*
 * ratelimit_client(struct Client *client_p, int penalty)
 *
 * Applies a penalty to a client for executing a rate-limited command.
 *
 * Inputs:
 *    - the client to be rate-limited
 *    - the penalty to apply
 *
 * Outputs:
 *    - 1 if the user has been penalized and the command should be
 *      allowed to execute
 *    - 0 if the command should not execute and the user has not
 *      been penalized (they are executing commands too fast and have
 *      been rate-limited)
 *      The caller should return RPL_LOAD2HI
 *
 * Side effects:
 *    - The ratelimit for the user will be initialized if it hasn't
 *      been initialized yet.
 */
int ratelimit_client(struct Client *client_p, unsigned int penalty)
{
	s_assert(client_p);
	s_assert(MyClient(client_p));

	if (!client_p->localClient->ratelimit)
	{
		/* Not initialized yet - do it now. */
		client_p->localClient->ratelimit = rb_current_time() - ConfigFileEntry.max_ratelimit_tokens;
	}

	/* Don't make it impossible to execute anything. */
	if (penalty > (unsigned int)ConfigFileEntry.max_ratelimit_tokens)
		penalty = ConfigFileEntry.max_ratelimit_tokens;

	if (client_p->localClient->ratelimit <= rb_current_time() - ConfigFileEntry.max_ratelimit_tokens)
	{
		client_p->localClient->ratelimit = rb_current_time() - ConfigFileEntry.max_ratelimit_tokens + penalty;
		return 1;
	}

	if (client_p->localClient->ratelimit + penalty > rb_current_time())
	{
		ServerStats.is_rl++;
		return 0;
	}

	client_p->localClient->ratelimit += penalty;

	return 1;
}

/*
 * ratelimit_client_who(struct Client *client_p, int penalty)
 *
 * Rate-limits a client for a WHO query if they have no remaining "free"
 * WHO queries to execute.
 *
 * Inputs:
 *   - same as ratelimit_client
 *
 * Outputs:
 *   - same as ratelimit_client
 *
 * Side effects:
 *   - A "free who" token will be removed from the user if one exists.
 *     If one doesn't exist, the user will be ratelimited as normal.
 */
int ratelimit_client_who(struct Client *client_p, unsigned int penalty)
{
	s_assert(client_p);
	s_assert(MyClient(client_p));

	if (client_p->localClient->join_who_credits)
	{
		--client_p->localClient->join_who_credits;
		return 1;
	}

	return ratelimit_client(client_p, penalty);
}

/*
 * credit_client_join(struct Client *client_p)
 *
 * Gives a user a credit to execute a WHO for joining a channel.
 *
 * Inputs:
 *   - the client to be credited
 *
 * Outputs:
 *   - (none)
 *
 * Side effects:
 *   - (none)
 */
void credit_client_join(struct Client *client_p)
{
	s_assert(client_p);
	s_assert(MyClient(client_p));

	++client_p->localClient->join_who_credits;
}
