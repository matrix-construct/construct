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
#define HAVE_IRCD_CMD_H

#ifdef __cplusplus
namespace ircd {

struct line
:rfc1459::line
{
	using rfc1459::line::line;

	auto &operator[](const size_t &pos) const;
	auto &operator[](const size_t &pos);
};

inline auto &pfx(const line &line)               { return line.pfx;                                }
inline auto &pfx(line &line)                     { return line.pfx;                                }
inline auto &nick(const line &line)              { return pfx(line).nick;                          }
inline auto &nick(line &line)                    { return pfx(line).nick;                          }
inline auto &user(const line &line)              { return pfx(line).user;                          }
inline auto &user(line &line)                    { return pfx(line).user;                          }
inline auto &host(const line &line)              { return pfx(line).host;                          }
inline auto &host(line &line)                    { return pfx(line).host;                          }
inline auto &command(const line &line)           { return line.cmd;                                }
inline auto &command(line &line)                 { return line.cmd;                                }
inline auto &parv(const line &line)              { return line.parv;                               }
inline auto &parv(line &line)                    { return line.parv;                               }
inline auto parc(const line &line)               { return parv(line).size();                       }

inline auto &
line::operator[](const size_t &pos)
{
	return parv.at(pos);
}

inline auto &
line::operator[](const size_t &pos)
const
{
	return parv.at(pos);
}

} // namespace ircd
#endif


#ifdef __cplusplus
namespace ircd {
namespace cmds {

using client::client;

IRCD_EXCEPTION(ircd::error, error)
IRCD_EXCEPTION(error, not_found)                 // Really should be throwing err::421 directly but..
IRCD_EXCEPTION(error, already_exists)

struct cmd
{
	std::string name;
	std::set<std::string> aliases;
	std::function<void (client &, line)> handler;    // Used if not overloaded

  private:
	void emplace();

  public:
	virtual void operator()(client &, line);

	cmd(const std::string &name);
	cmd(const std::string &name, const decltype(handler) &handler);
	cmd(cmd &&) = delete;
	cmd(const cmd &) = delete;
	virtual ~cmd() noexcept;
};

// cmd structs are managed by their module (or wherever the instance resides)
extern std::map<std::string, cmd *, case_insensitive_less> cmds;

bool exists(const std::string &name);
cmd *find(const std::string &name, const std::nothrow_t &);
cmd &find(const std::string &name);

void execute(client &client, line);
void execute(client &client, const std::string &line);
void execute(client &client, const uint8_t *const &line, const size_t &len);

} // namespace cmds

using cmds::execute;
using cmds::cmd;

} // namespace ircd
#endif
