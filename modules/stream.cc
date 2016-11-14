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

#include <ircd/js/js.h>

namespace ircd  {
namespace js    {

struct stream
:trap
{
	struct read;

	void on_new(object::handle, object &, const args &) override;

	stream()
	:trap{"stream"}
	{
		parent_prototype = &trap::find("events");
	}
}
static stream;

struct stream::read
:trap::function
{
	value on_call(object::handle, value::handle, const args &) override;
	using trap::function::function;
}
static read{stream, "read"};

void
stream::on_new(object::handle callee,
               object &that,
               const args &args)
{
}

value
stream::read::on_call(object::handle callee,
                      value::handle _that,
                      const args &args)
{
	object that(_that);
	printf("read\n");

	return {};
}

} // namespace js
} // namespace ircd

using namespace ircd::js;
using namespace ircd;

mapi::header IRCD_MODULE
{
	"Abstract interface for working with streaming data"
};
