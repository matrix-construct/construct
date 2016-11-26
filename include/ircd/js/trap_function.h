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
#define HAVE_IRCD_JS_TRAP_FUNCTION

namespace ircd {
namespace js   {

struct trap::function
{
	using closure = std::function<value (object::handle, value::handle, const args &)>;

	trap *member;
	const std::string name;
	const uint flags;
	const uint arity;
	closure lambda;

  protected:
	virtual value on_call(object::handle callee, value::handle that, const args &);

  private:
	static function &from(JSObject *const &);
	static bool handle_call(JSContext *, unsigned, JS::Value *) noexcept;

  public:
	js::function operator()(const object::handle &) const;

	function(trap &,
	         std::string name,
	         const uint &flags = 0,
	         const uint &arity = 0,
	         const closure & = {});

	function(function &&) = delete;
	function(const function &) = delete;
	virtual ~function() noexcept;
};

} // namespace js
} // namespace ircd
