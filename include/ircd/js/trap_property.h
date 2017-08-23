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
#define HAVE_IRCD_JS_TRAP_PROPERTY

namespace ircd {
namespace js   {

struct trap::property
{
	friend struct trap;
	using function = js::function;

	struct trap *trap;
	std::string name;
	JSPropertySpec spec;

	virtual value on_set(function::handle, object::handle, value::handle);
	virtual value on_get(function::handle, object::handle);

  private:
	static bool handle_set(JSContext *c, unsigned argc, JS::Value *argv) noexcept;
	static bool handle_get(JSContext *c, unsigned argc, JS::Value *argv) noexcept;

  public:
	property(struct trap &, const std::string &name, const uint &flags = 0);
	property(property &&) = delete;
	property(const property &) = delete;
	virtual ~property() noexcept;
};

} // namespace js
} // namespace ircd
