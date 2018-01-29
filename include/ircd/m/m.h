/*
 * charybdis: 21st Century IRC++d
 *
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
 *
 */

#pragma once
#define HAVE_IRCD_M_H

/// Matrix Protocol System
namespace ircd::m
{
	struct init;

	extern struct user me;
	extern struct room my_room;
	extern struct room control;
	extern struct log::log log;

	IRCD_OVERLOAD(generate)
}

namespace ircd::m::self
{
	string_view host();
	bool host(const string_view &);
}

namespace ircd::m
{
	inline string_view my_host()                 { return self::host();        }
	inline bool my_host(const string_view &h)    { return self::host(h);       }
}

namespace ircd
{
	using m::my_host;
}

#include "name.h"
#include "error.h"
#include "id.h"
#include "event.h"
#include "query.h"
#include "cursor.h"
#include "dbs.h"
#include "state.h"
#include "room.h"
#include "user.h"
#include "request.h"
#include "session.h"
#include "v1/v1.h"
#include "io.h"
#include "vm.h"
#include "filter.h"
#include "keys.h"
#include "txn.h"

struct ircd::m::init
{
	json::object conf;

	void bootstrap();
	void listeners();
	void modules();

	dbs::init _dbs;
	keys::init _keys;

  public:
	init();
	~init() noexcept;
};
