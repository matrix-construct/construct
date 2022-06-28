// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SPIRIT_KARMA_RULE_H

#if defined(__clang__)
/// Custom reimplementation/interposition of the qi::rule template by
/// specializing the `char *` iterator, which is common to our generators.
///
/// See: qi_rule.h
///
template<class T1,
         class T2,
         class T3,
         class T4>
struct boost::spirit::karma::rule<char *, T1, T2, T3, T4>
:proto::extends
<
	typename proto::terminal<reference<rule<char *, T1, T2, T3, T4> const>>::type
	,rule<char *, T1, T2, T3, T4>
>
,generator<rule<char *, T1, T2, T3, T4>>
{
	using Iterator = char *;
	using iterator_type = Iterator;
	using properties = mpl::int_<generator_properties::all_properties>;
	using this_type = rule<iterator_type, T1, T2, T3, T4>;
	using reference_ = reference<const this_type>;
	using terminal = typename proto::terminal<reference_>::type;
	using base_type = proto::extends<terminal, this_type>;
	using template_params = mpl::vector<T1, T2, T3, T4>;
	using output_iterator = detail::output_iterator<iterator_type, properties>;
	using locals_type = typename spirit::detail::extract_locals<template_params>::type;
	using delimiter_type = typename spirit::detail::extract_component<karma::domain, template_params>::type;
	using encoding_type = typename spirit::detail::extract_encoding<template_params>::type;
	using sig_type = typename spirit::detail::extract_sig<template_params, encoding_type, karma::domain>::type;
	using attr_type = typename spirit::detail::attr_from_sig<sig_type>::type;
	using attr_reference_type = const attr_type &;
	using parameter_types = typename spirit::detail::params_from_sig<sig_type>::type;
	using parameter_types_size = typename fusion::result_of::size<parameter_types>::type;
	using context_type = context<fusion::cons<attr_reference_type, parameter_types>, locals_type>;
	using function_prototype = bool (output_iterator &, context_type &, const delimiter_type &);
	using function_type = function<function_prototype>;
	using encoding_modifier_type = typename mpl::if_
	<
		is_same<encoding_type, unused_type>,
		unused_type,
		tag::char_code<tag::encoding, encoding_type>
	>::type;

	template<class Context,
	         class Unused>
	struct attribute
	{
		using type = attr_type;
	};

	static constexpr const auto &params_size
	{
		parameter_types_size::value
	};

  private:
	BOOST_STATIC_ASSERT_MSG
	(
		!is_reference<attr_type>::value && !is_const<attr_type>::value,
		"Const/reference qualifiers on Karma rule attribute are meaningless"
	);

	template<class Auto,
	         class Expr>
	static decltype(auto) define(Expr&& expr, mpl::false_)
	{
		BOOST_SPIRIT_ASSERT_MATCH(karma::domain, Expr);
	}

	template<class Auto,
	         class Expr>
	[[warn_unused_result]]
	static decltype(auto) define(Expr&& expr, mpl::true_)
	{
		return detail::bind_generator<const Auto>
		(
			compile<karma::domain>(std::forward<Expr>(expr), encoding_modifier_type())
		);
	}

	template<class Expr>
	static decltype(auto) create(Expr&& expr)
	{
		return define<mpl::false_>
		(
			std::forward<Expr>(expr), traits::matches<karma::domain, Expr>()
		);
	}

	static constexpr const size_t buf_sz
	{
		512 - 32 // offsetof buf
	};

	function_type func;
	const char *_name;
	size_t _size;
	[[clang::loader_uninitialized]] char buf alignas(32) [buf_sz];

  public:
	using parameterized_subject_type = const rule;
	const rule &get_parameterized_subject() const
	{
		return *this;
	}

	reference_ alias() const noexcept
	{
	    return reference_(*this);
	}

	std::string_view name() const
	{
		return _name;
	}

	template<class Context>
	info what(Context&) const noexcept
	{
	    return info(std::string());
	}

	template<class C,
	         class D,
	         class A>
	bool generate(output_iterator&, C&, const D &, const A &) const;

	template<class C,
	         class D,
	         class A,
	         class P>
	bool generate(output_iterator&, C&, const D &, const A &, const P &) const;

	#include <boost/spirit/home/karma/nonterminal/detail/fcall.hpp>

    template<class Expr>
    rule(const Expr &expr, const char *const name = "<unnamed>") noexcept;

    template<class Expr>
    explicit rule(const rule &o, const char *const name) noexcept;

    rule(const rule &o)
    :rule(o, o._name)
    {}

    rule() = delete;
    rule(rule &&o) = delete;
    rule& operator=(rule &&) = delete;
    rule& operator=(const rule &) = delete;
    ~rule() noexcept = default;
};

template<class T1,
         class T2,
         class T3,
         class T4>
template<class Expr>
inline
boost::spirit::karma::rule<char *, T1, T2, T3, T4>::rule(const Expr &expr,
                                                         const char *const name)
noexcept
:base_type(terminal::make(reference_(*this)))
,func(new (buf) decltype(create(expr)) (create(expr)))
,_name(name)
,_size(sizeof(decltype(create(expr))))
{
	/// If this assertion has tripped first ensure you are not copying
	/// an instance of this rule, otherwise see the comment on buf_sz.
	static_assert
	(
		sizeof(decltype(create(expr))) <= buf_sz,
		"Instance buffer is insufficient for the binder object."
	);
}

template<class T1,
         class T2,
         class T3,
         class T4>
template<class Expr>
inline
boost::spirit::karma::rule<char *, T1, T2, T3, T4>::rule(const rule &o,
                                                         const char *const name)
noexcept
:base_type(terminal::make(reference_(*this)))
,func(o.func, buf)
,_name(name)
,_size(o._size)
{
	assert(_size <= buf_sz);
	memcpy(buf, o.buf, _size);
}

template<class T1,
         class T2,
         class T3,
         class T4>
template<class Context,
         class Delimiter,
         class Attribute>
inline bool
boost::spirit::karma::rule<char *, T1, T2, T3, T4>::generate(output_iterator &sink,
                                                             Context &,
                                                             const Delimiter &delim,
	                                                         const Attribute &attr)
const
{
	using transform = traits::transform_attribute<const Attribute, attr_type, domain>;
	typename transform::type attr_(transform::pre(attr));
	context_type context(attr_);

	assume(bool(func));
	const auto ret
	{
		func(sink, context, delim)
	};

	if constexpr(is_same<delimiter_type, unused_type>::value)
		if(ret)
			karma::delimit_out(sink, delim);

	return ret;
}

template<class T1,
         class T2,
         class T3,
         class T4>
template<class Context,
         class Delimiter,
         class Attribute,
         class Params>
inline bool
boost::spirit::karma::rule<char *, T1, T2, T3, T4>::generate(output_iterator &sink,
                                                             Context &caller_context,
                                                             const Delimiter &delim,
	                                                         const Attribute &attr,
	                                                         const Params &params)
const
{
	typedef traits::transform_attribute<Attribute const, attr_type, domain> transform;
	typename transform::type attr_ = transform::pre(attr);
	context_type context(attr_, params, caller_context);

	assume(bool(func));
	const auto ret
	{
		func(sink, context, delim)
	};

	if constexpr(is_same<delimiter_type, unused_type>::value)
		if(ret)
			karma::delimit_out(sink, delim);

	return ret;
}

#endif defined(__clang__)
