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

namespace ircd   {
namespace client {
namespace user   {

struct user
{
	chans_t chans;
	invites_t invites;
	std::string suser;
	std::string away;

	user(const std::string &suser = {});
};

user::user(const std::string &suser)
:suser(suser)
{
}

invites_t &
invites(user &user)
{
	return user.invites;
}

const invites_t &
invites(const user &user)
{
	return user.invites;
}

chans_t &
chans(user &user)
{
	return user.chans;
}

const chans_t &
chans(const user &user)
{
	return user.chans;
}

std::string &
away(user &user)
{
	return user.away;
}

const std::string &
away(const user &user)
{
	return user.away;
}

std::string &
suser(user &user)
{
	return user.suser;
}

const std::string &
suser(const user &user)
{
	return user.suser;
}

} // namespace user
} // namespace client
} // namespace ircd
