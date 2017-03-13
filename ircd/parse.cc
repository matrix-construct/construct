/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
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

//#include <boost/spirit/include/qi.hpp>
//#include <boost/spirit/include/karma.hpp>

namespace ircd {

// Registry of grammars. Grammars are usualy static data or modules.
IRCD_INIT_PRIORITY(STD_CONTAINER)
decltype(parse::grammar::grammars)
parse::grammar::grammars
{
};

} // namespace ircd

ircd::parse::grammar::grammar(const char *const &name)
:name{name}
,grammars_it{[this, &name]
{
	const auto iit(grammars.emplace(name, this));
	if(!iit.second)
		throw grammar_error("Parsing grammar named \"%s\" already exists.", name);

	return iit.first;
}()}
{
}

ircd::parse::grammar::~grammar()
noexcept
{
	grammars.erase(grammars_it);
}


//
// Museum of historical comments
//
// parse.c (1990 - 2016)
//

    /* ok, fake prefix happens naturally during a burst on a nick
     * collision with TS5, we cant kill them because one client has to
     * survive, so we just send an error.
     */

    /* meepfoo  is a nickname (ignore)
     * #XXXXXXXX    is a UID (KILL)
     * #XX      is a SID (SQUIT)
     * meep.foo is a server (SQUIT)
     */
/*
 * *WARNING*
 *      Numerics are mostly error reports. If there is something
 *      wrong with the message, just *DROP* it! Don't even think of
 *      sending back a neat error message -- big danger of creating
 *      a ping pong error message...
 */

    /*
     * Prepare the parameter portion of the message into 'buffer'.
     * (Because the buffer is twice as large as the message buffer
     * for the socket, no overflow can occur here... ...on current
     * assumptions--bets are off, if these are changed --msa)
     * Note: if buffer is non-empty, it will begin with SPACE.
     */

            /*
             * We shouldn't get numerics sent to us,
             * any numerics we do get indicate a bug somewhere..
             */

            /* ugh.  this is here because of nick collisions.  when two servers
             * relink, they burst each other their nicks, then perform collides.
             * if there is a nick collision, BOTH servers will kill their own
             * nicks, and BOTH will kill the other servers nick, which wont exist,
             * because it will have been already killed by the local server.
             *
             * unfortunately, as we cant guarantee other servers will do the
             * "right thing" on a nick collision, we have to keep both kills.
             * ergo we need to ignore ERR_NOSUCHNICK. --fl_
             */

            /* quick comment. This _was_ tried. i.e. assume the other servers
             * will do the "right thing" and kill a nick that is colliding.
             * unfortunately, it did not work. --Dianora
             */

            /* note, now we send PING on server connect, we can
             * also get ERR_NOSUCHSERVER..
             */

            /* This message changed direction (nick collision?)
             * ignore it.
             */

        /* csircd will send out unknown umode flag for +a (admin), drop it here. */

        /* Fake it for server hiding, if its our client */

    /* bit of a hack.
     * I don't =really= want to waste a bit in a flag
     * number_of_nick_changes is only really valid after the client
     * is fully registered..
     */
