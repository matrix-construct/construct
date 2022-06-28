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
#define HAVE_IRCD_SPIRIT_QI_RULE_H

#if defined(__clang__)
/// Custom reimplementation/interposition of the qi::rule template by
/// specializing the `const char *` iterator, which is common to our parsers.
///
/// The goal here is to support our custom lightweight function object (see
/// function.h). This implementation is optimized for our grammar's patterns,
/// which generally involve constant/immutable exprs and static global rules.
///
/// Dynamic memory has been removed which would otherwise impede optimization
/// at global scope:
///
/// - For the rule's name, which was formerly std::string. The name is now a
/// `const char *` which should point to a literal.
///
/// - To support the custom function object, a fixed array is contained
/// directly within this class. The parser_binder is placement new'ed there.
///
template<class T1,
         class T2,
         class T3,
         class T4>
struct boost::spirit::qi::rule<const char *, T1, T2, T3, T4>
:proto::extends
<
	typename proto::terminal<reference<const rule<const char *, T1, T2, T3, T4>>>::type
	,rule<const char *, T1, T2, T3, T4>
>
,parser
<
	rule<const char *, T1, T2, T3, T4>
>
{
	using Iterator = const char *;
	using iterator_type = Iterator;
	using this_type = rule<iterator_type, T1, T2, T3, T4>;
	using reference_ = reference<const this_type>;
	using terminal = typename proto::terminal<reference_>::type;
	using terminal_type = typename proto::terminal<this_type>::type;
	using parser_type = parser<this_type>;
	using base_type = proto::extends<terminal, this_type>;
	using template_params = mpl::vector<T1, T2, T3, T4>;
	using locals_type = typename spirit::detail::extract_locals<template_params>::type;
	using skipper_type = typename spirit::detail::extract_component<qi::domain, template_params>::type;
	using encoding_type = typename spirit::detail::extract_encoding<template_params>::type;
	using sig_type = typename spirit::detail::extract_sig<template_params, encoding_type, qi::domain>::type;
	using attr_type = typename spirit::detail::attr_from_sig<sig_type>::type;
	using attr_reference_type = attr_type &;
	using parameter_types = typename spirit::detail::params_from_sig<sig_type>::type;
	using parameter_types_size = typename fusion::result_of::size<parameter_types>::type;
	using context_type = context<fusion::cons<attr_reference_type, parameter_types>, locals_type>;
	using function_prototype = bool (Iterator &, const Iterator &, context_type &, const skipper_type &);
	using function_type = function<function_prototype>;
	using encoding_modifier_type = typename mpl::if_
	<
		is_same<encoding_type, unused_type>,
		unused_type,
		tag::char_code<tag::encoding, encoding_type>
	>::type;

	template<class Context,
	         class Iterator>
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
		!is_reference<attr_type>::value,
		"Reference qualifier on Qi rule attribute is meaningless"
	);

	template<class Auto,
	         class Expr>
	static decltype(auto) define(Expr&& expr, mpl::false_)
	{
		BOOST_SPIRIT_ASSERT_MATCH(qi::domain, Expr);
	}

	template<class Auto,
	         class Expr>
	[[warn_unused_result]]
	static decltype(auto) define(Expr&& expr, mpl::true_)
	{
		return detail::bind_parser<const Auto>
		(
			compile<qi::domain>(std::forward<Expr>(expr), encoding_modifier_type())
		);
	}

	template<class Expr>
	static decltype(auto) create(Expr&& expr)
	{
		return define<mpl::false_>
		(
			std::forward<Expr>(expr), traits::matches<qi::domain, Expr>()
		);
	}

	/// Slightly exceeds the worst-case for currently in use compiled
	/// expressions. It is possible to build an expression which exceeds
	/// this value, and in that case, feel free to increase this size.
	static constexpr const size_t buf_sz
	{
		2048 - 32 // offsetof buf
	};

	function_type func;
	const char *_name;
	size_t _size;
	[[clang::loader_uninitialized]] uint8_t buf alignas(32) [buf_sz];

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
		return info(std::string(name()));
	}

	template<class C,
	         class S,
	         class A>
	bool parse(Iterator &, const Iterator &, C &, const S &, A &) const;

	template<class C,
	         class S,
	         class A,
	         class P>
	bool parse(Iterator &, const Iterator &, C &, const S &, A &, const P &) const;

	// bring in the operator() overloads
	#include <boost/spirit/home/qi/nonterminal/detail/fcall.hpp>

	/// Expression compiler; primary construction.
	template<class Expr>
	rule(const Expr &expr, const char *const name = "<unnamed>") noexcept;

	/// Renaming constructor. This supports a special case when this rule is
	/// constructed from a single other rule, but a name is still provided as
	/// the second argument; prevents forced indirection and bad codegen.
	template<class Expr>
	explicit rule(const rule &o, const char *const name) noexcept;

	/// Copy construction is supported through the rename-ctor, just using the
	/// name from the rhs.
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
boost::spirit::qi::rule<const char *, T1, T2, T3, T4>::rule(const Expr &expr,
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
boost::spirit::qi::rule<const char *, T1, T2, T3, T4>::rule(const rule &o,
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
         class Skipper,
         class Attribute>
inline bool
boost::spirit::qi::rule<const char *, T1, T2, T3, T4>::parse(Iterator &first,
                                                             const Iterator &last,
                                                             Context &,
                                                             const Skipper &skipper,
                                                             Attribute &attr_param)
const
{
	BOOST_STATIC_ASSERT_MSG
	(
		(is_same<skipper_type, unused_type>::value || !is_same<Skipper, unused_type>::value),
		"The rule was instantiated with a skipper type but you have not pass any. "
		"Did you use `parse` instead of `phrase_parse`?"
	);

	BOOST_STATIC_ASSERT_MSG
	(
		(is_convertible<const Skipper &, skipper_type>::value),
		"The passed skipper is not compatible/convertible to one "
		"that the rule was instantiated with"
	);

	if constexpr(is_same<skipper_type, unused_type>::value)
		qi::skip_over(first, last, skipper);

	using transform = traits::transform_attribute<Attribute, attr_type, domain>;
	typename transform::type attr_(transform::pre(attr_param));
	context_type context(attr_);

	assume(bool(func));
	const bool ret
	{
		func(first, last, context, skipper)
	};

	if(ret)
		transform::post(attr_param, attr_);
	else
		transform::fail(attr_param);

	return ret;
}

template<class T1,
         class T2,
         class T3,
         class T4>
template<class Context,
         class Skipper,
         class Attribute,
         class Params>
inline bool
boost::spirit::qi::rule<const char *, T1, T2, T3, T4>::parse(Iterator &first,
                                                             const Iterator &last,
                                                             Context &caller_context,
                                                             const Skipper &skipper,
                                                             Attribute &attr_param,
                                                             const Params &params)
const
{
	BOOST_STATIC_ASSERT_MSG
	(
		(is_same<skipper_type, unused_type>::value || !is_same<Skipper, unused_type>::value),
		"The rule was instantiated with a skipper type but you have not pass any. "
		"Did you use `parse` instead of `phrase_parse`?"
	);

	BOOST_STATIC_ASSERT_MSG
	(
		(is_convertible<const Skipper &, skipper_type>::value),
		"The passed skipper is not compatible/convertible to one "
		"that the rule was instantiated with"
	);

	if constexpr(is_same<skipper_type, unused_type>::value)
		qi::skip_over(first, last, skipper);

	using transform = traits::transform_attribute<Attribute, attr_type, domain>;
	typename transform::type attr_(transform::pre(attr_param));
	context_type context(attr_, params, caller_context);

	assume(bool(func));
	const bool ret
	{
		func(first, last, context, skipper)
	};

	if(ret)
		transform::post(attr_param, attr_);
	else
		transform::fail(attr_param);

	return ret;
}

#endif defined(__clang__)
