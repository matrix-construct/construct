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
#define HAVE_IRCD_JS_CALL_H

namespace ircd {
namespace js   {

inline value
call(const object &obj,
     const JS::HandleFunction &func,
     const JS::HandleValueArray &args = JS::HandleValueArray::empty())
{
	value ret;
	if(!JS_CallFunction(*cx, obj, func, args, &ret))
		throw internal_error("Failed to call function");

	return ret;
}

inline value
call(const object &obj,
     const JS::HandleValue &val,
     const JS::HandleValueArray &args)
{
	value ret;
	if(!JS_CallFunctionValue(*cx, obj, val, args, &ret))
		throw internal_error("Failed to apply function value to object");

	return ret;
}

inline value
call(const object &obj,
     const char *const &name,
     const JS::HandleValueArray &args)
{
	value ret;
	if(!JS_CallFunctionName(*cx, obj, name, args, &ret))
		throw reference_error("Failed to call function \"%s\"", name);

	return ret;
}

inline value
call(const object &obj,
     const std::string &name,
     const JS::HandleValueArray &args)
{
	return call(obj, name.c_str(), args);
}

} // namespace js
} // namespace ircd
