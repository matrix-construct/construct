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

#pragma once
#define HAVE_IRCD_CTX_SHARED_STATE_H

namespace ircd::ctx
{
	struct shared_state_base;
	template<class T = void> struct shared_state;
	template<> struct shared_state<void>;
}

struct ircd::ctx::shared_state_base
{
	dock cond;
	std::exception_ptr eptr;
	uint promise_refcnt {0};
	bool finished {false};

	void reset();
};

template<class T>
struct ircd::ctx::shared_state
:shared_state_base
,std::enable_shared_from_this<shared_state<T>>
{
	using value_type      = T;
	using pointer_type    = T *;
	using reference_type  = T &;

	T val;

	std::shared_ptr<const shared_state<T>> share() const;
	std::shared_ptr<shared_state<T>> share();
};

template<>
struct ircd::ctx::shared_state<void>
:shared_state_base
,std::enable_shared_from_this<shared_state<void>>
{
	using value_type      = void;

	std::shared_ptr<const shared_state<void>> share() const;
	std::shared_ptr<shared_state<void>> share();
};

inline std::shared_ptr<ircd::ctx::shared_state<void>>
ircd::ctx::shared_state<void>::share()
{
	return this->shared_from_this();
}

template<class T>
std::shared_ptr<ircd::ctx::shared_state<T>>
ircd::ctx::shared_state<T>::share()
{
	return this->shared_from_this();
}

inline std::shared_ptr<const ircd::ctx::shared_state<void>>
ircd::ctx::shared_state<void>::share()
const
{
	return this->shared_from_this();
}

template<class T>
std::shared_ptr<const ircd::ctx::shared_state<T>>
ircd::ctx::shared_state<T>::share()
const
{
	return this->shared_from_this();
}

inline void
ircd::ctx::shared_state_base::reset()
{
	eptr = nullptr;
	finished = false;
}
