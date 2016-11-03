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

struct future
:trap
{
	uint64_t id_ctr;

	void on_new(object::handle, object &, const args &args) override;

	future();
}
static future;

future::future()
:trap
{
	"future",
	JSCLASS_HAS_PRIVATE
}
,id_ctr{0}
{
}

void
future::on_new(object::handle, object &obj, const args &args)
{
	auto &task(task::get());

	if(args.has(0) && type(args[0]) == JSTYPE_FUNCTION)
		set(obj, "callback", args[0]);

	const auto id(++id_ctr);
	task.pending_add(id);
	set(obj, "id", id);
}

} // namespace js
} // namespace ircd

using namespace ircd;

mapi::header IRCD_MODULE
{
	"Fundamental yieldable object for asynchronicity.",
};
