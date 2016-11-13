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
#define HAVE_IRCD_JS_PRIV_H

///////////////////////////////////////////////////////////////////////////////
//
// To properly use the private slot: have your structure inherit from `priv_data` and construct
// it with std::make_shared(). When garbage collection of the js object occurs, the private data
// slot will be abstractly considered an `std::shared_ptr<priv_data>` and the reference count
// will be decremented. Note: The js object must have been created with flag JSCLASS_HAS_PRIVATE
//

namespace ircd {
namespace js   {

// ctor()/get()/set()/has()/del() have this overload to manipulate the private slot of an object.
IRCD_OVERLOAD(priv)

// Extend this class to store your data by the managed private slot of an object. Note: The js
// object must have been created with flag JSCLASS_HAS_PRIVATE.
struct priv_data
:std::enable_shared_from_this<priv_data>
{
	// The `delete` of the managed data will be on the type `priv_data`.
	// This vtable is anchored in ircd/js.cc.
	virtual ~priv_data() noexcept;
};

using priv_ptr = std::shared_ptr<priv_data>;

} // namespace js
} // namespace ircd
