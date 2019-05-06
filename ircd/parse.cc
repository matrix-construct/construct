/*
 *  charybdis: an advanced ircd.
 *  parse.c: The message parser.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2005 ircd-ratbox development team
 *  Copyright (C) 2007-2016 William Pitcock
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
 */

decltype(ircd::spirit::rulebuf) thread_local
ircd::spirit::rulebuf;

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
