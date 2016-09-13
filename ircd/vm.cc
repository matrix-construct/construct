/*
 *  Copyright (C) 2016 Charybdis Development Team
 *  Copyright (C) 2016 Jason Volk <jason@zemos.net>
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

namespace ircd {


} // namespace ircd

void
ircd::vm::execute(client &client,
                  const uint8_t *const &ptr,
                  const size_t &len)
{
	execute(client, line(ptr, len));
}

void
ircd::vm::execute(client &client,
                  const std::string &l)
{
	execute(client, line(l));
}

void
ircd::vm::execute(client &client,
                  tape &reel)
{
	context([wp(weak_from(client)), &client, &reel]
	{
		// Hold the client for the lifetime of this context
		const lifeguard<struct client> lg(wp);

		while(!reel.empty()) try
		{
			auto &line(reel.front());
			const scope pop([&reel]
			{
				reel.pop_front();
			});

			if(line.empty())
				continue;

			auto &handle(cmds::find(command(line)));
			handle(client, std::move(line));
		}
		catch(const std::exception &e)
		{
			log::error("vm: %s", e.what());
			disconnect(client);
			finished(client);
			return;
		}

		recv_next(client);
	},
	ctx::SELF_DESTRUCT);
}

void
ircd::vm::execute(client &client,
                  line line)
{
	if(line.empty())
		return;

	auto &handle(cmds::find(command(line)));
	handle(client, std::move(line));
}
