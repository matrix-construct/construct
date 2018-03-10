// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_CTX_PROMISE_H

namespace ircd::ctx
{
	template<class T = void> class promise;
	template<> class promise<void>;

	template<class T> class future;
	template<> class future<void>;

	IRCD_EXCEPTION(ircd::ctx::error, future_error)
	IRCD_EXCEPTION(future_error, no_state)
	IRCD_EXCEPTION(future_error, broken_promise)
	IRCD_EXCEPTION(future_error, future_already_retrieved)
	IRCD_EXCEPTION(future_error, promise_already_satisfied)

	template<class T> size_t refcount(const shared_state<T> &);
	template<class T> void update(shared_state<T> &s);
	template<class T> void invalidate(shared_state<T> &);
	template<class T> void remove(shared_state<T> &, promise<T> &);
	template<class T> void update(promise<T> &new_, promise<T> &old);
	template<class T> void append(promise<T> &new_, promise<T> &old);
}

template<class T>
struct ircd::ctx::promise
{
	shared_state<T> *st {nullptr};               // Reference to the state resident in future
	mutable promise *next {nullptr};             // Promise fwdlist to support copy semantics

  public:
	using value_type                             = typename shared_state<T>::value_type;
	using pointer_type                           = typename shared_state<T>::pointer_type;
	using reference_type                         = typename shared_state<T>::reference_type;

	bool valid() const                           { return bool(st);                                }
	bool operator!() const                       { return !valid();                                }
	operator bool() const                        { return valid();                                 }

	const shared_state<T> &get_state() const     { assert(valid()); return *st;                    }
	shared_state<T> &get_state()                 { assert(valid()); return *st;                    }

	void set_exception(std::exception_ptr eptr);
	void set_value(const T &val);
	void set_value(T&& val);

	promise() = default;
	promise(promise &&o) noexcept;
	promise(const promise &);
	promise &operator=(const promise &) = delete;
	promise &operator=(promise &&) noexcept;
	~promise() noexcept;
};

template<>
struct ircd::ctx::promise<void>
{
	shared_state<void> *st {nullptr};            // Reference to the state resident in future
	mutable promise *next {nullptr};             // Promise fwdlist to support copy semantics

  public:
	using value_type                             = typename shared_state<void>::value_type;

	bool valid() const                           { return bool(st);                                }
	bool operator!() const                       { return !valid();                                }
	operator bool() const                        { return valid();                                 }

	const shared_state<void> &get_state() const  { assert(valid()); return *st;                    }
	shared_state<void> &get_state()              { assert(valid()); return *st;                    }

	void set_exception(std::exception_ptr eptr);
	void set_value();

	promise() = default;
	promise(promise &&o) noexcept;
	promise(const promise &);
	promise &operator=(const promise &) = delete;
	promise &operator=(promise &&) noexcept;
	~promise() noexcept;
};

template<class T>
ircd::ctx::promise<T>::promise(promise<T> &&o)
noexcept
:st{std::move(o.st)}
,next{std::move(o.next)}
{
	if(st)
	{
		update(*this, o);
		o.st = nullptr;
	}
}

inline
ircd::ctx::promise<void>::promise(promise<void> &&o)
noexcept
:st{std::move(o.st)}
,next{std::move(o.next)}
{
	if(st)
	{
		update(*this, o);
		o.st = nullptr;
	}
}

template<class T>
ircd::ctx::promise<T>::promise(const promise<T> &o)
:st{o.st}
,next{nullptr}
{
	append(*this, const_cast<promise<T> &>(o));
}

inline
ircd::ctx::promise<void>::promise(const promise<void> &o)
:st{o.st}
,next{nullptr}
{
	append(*this, const_cast<promise<void> &>(o));
}

template<class T>
ircd::ctx::promise<T> &
ircd::ctx::promise<T>::operator=(promise<T> &&o)
noexcept
{
	this->~promise();
	st = std::move(o.st);
	next = std::move(o.next);
	if(!st)
		return *this;

	update(*this, o);
	o.st = nullptr;
	return *this;
}

inline
ircd::ctx::promise<void> &
ircd::ctx::promise<void>::operator=(promise<void> &&o)
noexcept
{
	this->~promise();
	st = std::move(o.st);
	next = std::move(o.next);
	if(!st)
		return *this;

	update(*this, o);
	o.st = nullptr;
	return *this;
}

template<class T>
ircd::ctx::promise<T>::~promise()
noexcept
{
	if(!valid())
		return;

	if(refcount(*st) == 1)
		set_exception(std::make_exception_ptr(broken_promise()));
	else
		remove(*st, *this);
}

inline
ircd::ctx::promise<void>::~promise()
noexcept
{
	if(!valid())
		return;

	if(refcount(*st) == 1)
		set_exception(std::make_exception_ptr(broken_promise()));
	else
		remove(*st, *this);
}

template<class T>
void
ircd::ctx::promise<T>::set_value(T&& val)
{
	assert(valid());
	assert(pending(*st));
	auto *const st{this->st};
	invalidate(*st);
	set_ready(*st);
	st->val = std::move(val);
	st->cond.notify_all();
}

template<class T>
void
ircd::ctx::promise<T>::set_value(const T &val)
{
	assert(valid());
	assert(pending(*st));
	auto *const st{this->st};
	invalidate(*st);
	set_ready(*st);
	st->val = val;
	st->cond.notify_all();
}

inline void
ircd::ctx::promise<void>::set_value()
{
	assert(valid());
	assert(pending(*st));
	auto *const st{this->st};
	invalidate(*st);
	set_ready(*st);
	st->cond.notify_all();
}

template<class T>
void
ircd::ctx::promise<T>::set_exception(std::exception_ptr eptr)
{
	assert(valid());
	assert(pending(*st));
	auto *const st{this->st};
	invalidate(*st);
	set_ready(*st);
	st->eptr = std::move(eptr);
	st->cond.notify_all();
}

inline void
ircd::ctx::promise<void>::set_exception(std::exception_ptr eptr)
{
	assert(valid());
	assert(pending(*st));
	auto *const st{this->st};
	invalidate(*st);
	set_ready(*st);
	st->eptr = std::move(eptr);
	st->cond.notify_all();
}

template<class T>
void
ircd::ctx::append(promise<T> &new_,
                  promise<T> &old)
{
	if(!old.next)
	{
		old.next = &new_;
		return;
	}

	promise<T> *next{old.next};
	for(; next->next; next = next->next);
	next->next = &new_;
}

template<class T>
void
ircd::ctx::update(promise<T> &new_,
                  promise<T> &old)
{
	assert(old.st);
	auto &st{*old.st};
	if(!pending(st))
		return;

	if(st.p == &old)
	{
		st.p = &new_;
		return;
	}

	promise<T> *last{st.p};
	for(promise<T> *next{last->next}; next; last = next, next = last->next)
		if(next == &old)
		{
			last->next = &new_;
			break;
		}
}

template<class T>
void
ircd::ctx::remove(shared_state<T> &st,
                  promise<T> &p)
{
	if(!pending(st))
		return;

	if(st.p == &p)
	{
		st.p = p.next;
		return;
	}

	promise<T> *last{st.p};
	for(promise<T> *next{last->next}; next; last = next, next = last->next)
		if(next == &p)
		{
			last->next = p.next;
			break;
		}
}

template<class T>
void
ircd::ctx::invalidate(shared_state<T> &st)
{
	if(pending(st))
		for(promise<T> *p{st.p}; p; p = p->next)
			p->st = nullptr;
}

template<class T>
void
ircd::ctx::update(shared_state<T> &st)
{
	if(pending(st))
		for(promise<T> *p{st.p}; p; p = p->next)
			p->st = &st;
}

template<class T>
size_t
ircd::ctx::refcount(const shared_state<T> &st)
{
	size_t ret{0};
	if(pending(st))
		for(const promise<T> *p{st.p}; p; p = p->next)
			++ret;

	return ret;
}
