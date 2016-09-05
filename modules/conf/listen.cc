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

#include <boost/lexical_cast.hpp>

using namespace ircd;

mapi::header IRCD_MODULE
{
	"P-Line - configuration directives for listening sockets"
};

struct P
:conf::top
{
	void set(client::client &, std::string label, std::string key, std::string val) override;

	using conf::top::top;
}

static P
{
	'P', "listen",
};


struct block
{
	std::string host;
	std::vector<uint16_t> port;
};

std::map<std::string, block> blocks;

void
P::set(client::client &,
       std::string label,
       std::string key,
       std::string val)
{
	switch(hash(key))
	{
		case hash("host"):
			blocks[label].host = val;
			break;

		case hash("port"):
			if(!val)
			{
				blocks[label].port.clear();
				break;
			}

			tokens(val, ", ", [&](const std::string &token)
			{
				const auto &val(boost::lexical_cast<uint16_t>(token));
				blocks[label].port.emplace_back(val);
			});
			break;

		default:
			conf::log.warning("Unknown P-Line key \"%s\"", key.c_str());
			break;
	}
}
