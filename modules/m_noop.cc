/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
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

using namespace ircd;

mapi::header IRCD_MODULE
{
	"Matrix virtual Machine - No Operation"
};

namespace ircd::m::vm
{
	extern struct filter_event_id const filter_event_id;
	extern struct filter_types const filter_types;
	extern struct isa const isa;
}

struct ircd::m::vm::isa
:witness
{
	struct accumulator
	:vm::accumulator
	{

	};

	int add(accumulator *const &a, const event &event)
	{
		const event::prev &prev{event};

		return -1;
	}

	int del(accumulator *const &a, const event &event)
	{
		return -1;
	}

	std::unique_ptr<vm::accumulator> init() override final
	{
		return std::make_unique<accumulator>();
	}

	int add(vm::accumulator *const &a, const event &event) override final
	{
		return this->add(dynamic_cast<accumulator *>(a), event);
	}

	int del(vm::accumulator *const &a, const event &event) override final
	{
		return this->del(dynamic_cast<accumulator *>(a), event);
	}

	int test(const vm::accumulator *const &a, const query<> &q) override final
	{
		return -1;
	}

	isa()
	:witness
	{
		"instruction set architecture"
	}{}
}
const ircd::m::vm::isa;

struct ircd::m::vm::filter_event_id
:witness
{
	struct accumulator
	:vm::accumulator
	{
		std::set<std::string, std::less<>> ids;
	};

	int add(accumulator *const &a, const event &event)
	{
		const auto &event_id{at<"event_id"_>(event)};
		const auto iit
		{
			a->ids.emplace(std::string(event_id))
		};

		//std::cout << "added: " << iit.second << std::endl;
		return iit.second;
	}

	int del(accumulator *const &a, const event &event)
	{
		return -1;
	}

	int test(const accumulator *const &a, const query<> &q)
	{
		if(q.type != vm::where::equal)
			return -1;

		const auto &qq
		{
			dynamic_cast<const query<where::equal> &>(q)
		};

		const string_view &event_id
		{
			at<"event_id"_>(qq.value)
		};

		return a->ids.find(event_id) != end(a->ids);
	}

	std::unique_ptr<vm::accumulator> init() override final
	{
		return std::make_unique<accumulator>();
	}

	int add(vm::accumulator *const &a, const event &event) override final
	{
		return this->add(dynamic_cast<accumulator *>(a), event);
	}

	int del(vm::accumulator *const &a, const event &event) override final
	{
		return this->del(dynamic_cast<accumulator *>(a), event);
	}

	int test(const vm::accumulator *const &a, const query<> &q) override final
	{
		return this->test(dynamic_cast<const accumulator *>(a), q);
	}

	filter_event_id()
	:witness
	{
		"event_id does not exist"
	}{}
}
const ircd::m::vm::filter_event_id;

struct ircd::m::vm::filter_types
:witness
{
	struct accumulator
	:vm::accumulator
	{
		std::set<std::string, std::less<>> types;
	};

	int add(accumulator *const &a, const event &event)
	{
		const string_view &type{json::get<"type"_>(event)};
		if(empty(type))
			return -1;

		const auto iit
		{
			a->types.emplace(std::string(type))
		};

		return true;
	}

	int del(accumulator *const &a, const event &event)
	{
		return -1;
	}

	int test(const accumulator *const &a, const query<> &q)
	{
		if(q.type != vm::where::equal)
			return -1;

		const auto &qq
		{
			dynamic_cast<const query<where::equal> &>(q)
		};

		const string_view &type{json::get<"type"_>(qq.value)};
		if(empty(type))
			return -1;

		const auto count
		{
			a->types.count(type)
		};

		return count > 0;
	}

	ssize_t count(const accumulator *const &a, const query<> &q)
	{
		if(q.type != vm::where::equal)
			return -1;

		const auto &qq
		{
			dynamic_cast<const query<where::equal> &>(q)
		};

		const string_view &type{json::get<"type"_>(qq.value)};
		if(empty(type))
			return -1;

		if(!a->types.count(type))
			return 0;

		return -1;
	}

	std::unique_ptr<vm::accumulator> init() override final
	{
		return std::make_unique<accumulator>();
	}

	int add(vm::accumulator *const &a, const event &event) override final
	{
		return this->add(dynamic_cast<accumulator *>(a), event);
	}

	int del(vm::accumulator *const &a, const event &event) override final
	{
		return this->del(dynamic_cast<accumulator *>(a), event);
	}

	int test(const vm::accumulator *const &a, const query<> &query) override final
	{
		return this->test(dynamic_cast<const accumulator *>(a), query);
	}

	ssize_t count(const vm::accumulator *const &a, const query<> &query) override final
	{
		return this->count(dynamic_cast<const accumulator *>(a), query);
	}

	filter_types()
	:witness
	{
		"type has not been seen"
	}{}
}
const ircd::m::vm::filter_types;
