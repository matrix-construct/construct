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

#pragma once
#define HAVE_IRCD_CMDS_H

#ifdef __cplusplus
namespace ircd {
namespace cmds {

IRCD_EXCEPTION(ircd::error, error)
IRCD_EXCEPTION(error, not_found)                 // Really should be throwing err::421 directly but..
IRCD_EXCEPTION(error, already_exists)

struct cmd
{
	std::string name;
	std::set<std::string> aliases;
	std::function<void (client &, line)> handler;    // Not-overriding calls this by default

  private:
	void emplace();

  public:
	virtual void operator()(client &, line);         // Handle this command by overriding

	cmd(const std::string &name, const decltype(handler) &handler = {});
	cmd(cmd &&) = delete;
	cmd(const cmd &) = delete;
	virtual ~cmd() noexcept;
};

// cmd structs are managed by their module (or wherever the instance resides)
extern std::map<std::string, cmd *, case_insensitive_less> cmds;

bool exists(const std::string &name);
cmd *find(const std::string &name, const std::nothrow_t &);
cmd &find(const std::string &name);

} // namespace cmds

using cmds::cmd;

} // namespace ircd
#endif
