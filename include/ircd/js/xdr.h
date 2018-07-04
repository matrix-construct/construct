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
#define HAVE_IRCD_JS_XDR_H

namespace ircd {
namespace js   {

struct xdr
{
	struct header;
	struct atom;
	struct binding;
	struct sourcecode;
	struct sourcemap;
	struct displayurl;
	struct filename;
	struct source;
	struct op;
	struct bytecode;
	struct srcnote;
	struct consts;
	struct object;

	const struct header *header;
	const struct sourcecode *sourcecode;
	const struct atom *name;
	const struct binding *binding;
	const struct sourcemap *sourcemap;
	const struct displayurl *displayurl;
	const struct filename *filename;
	const struct source *source;
	const struct bytecode *bytecode;
	const struct srcnote *srcnote;
	const struct atom *atom;
	const struct consts *consts;
	const struct object *object;

	void for_each_atom(const std::function<void (const struct atom &)> &) const;
	void for_each_name(const std::function<void (const struct atom &)> &) const;
	void for_each_binding(const std::function<void (const struct binding &)> &) const;
	void for_each_bytecode(const std::function<void (const struct bytecode &)> &) const;
	void for_each_const(const std::function<void (const struct consts &)> &) const;
	void for_each_object(const std::function<void (const struct object &)> &) const;

	xdr(const const_buffer &);
};

struct xdr::header
{
	uint32_t build_id_length;
	uint32_t build_id;
	uint32_t length;
	uint32_t prologue_length;
	uint32_t version;
	uint32_t n_atoms;
	uint32_t n_srcnotes;
	uint32_t n_consts;
	uint32_t n_objects;
	uint32_t n_scopes;
	uint32_t n_try_notes;
	uint32_t n_scope_notes;
	uint32_t n_yield_offsets;
	uint32_t n_typesets;
	uint32_t fun_length;
	uint32_t script_bits;

	size_t num_names() const;
	size_t num_bindings() const;

	// debug print
	friend std::ostream &operator<<(std::ostream &, const header &);
};

struct xdr::sourcecode
{
	uint8_t has_source;
	uint8_t retrievable;
	uint32_t length;
	uint32_t compressed_length;
	uint8_t arguments_not_included;
	const char16_t code[0];

	// debug print
	friend std::ostream &operator<<(std::ostream &, const sourcecode &);
}
__attribute__((packed));

struct xdr::sourcemap
{
	uint8_t have;
	uint32_t len;
	const char16_t url[0];
};

struct xdr::displayurl
{
	uint8_t have;
	uint32_t len;
	const char16_t url[0];
};

struct xdr::filename
{
	uint8_t have;
	const char name[0];
};

struct xdr::atom
{
	uint32_t encoding  : 1;
	uint32_t length    : 31; union
	{
		const char latin1[0];
		const char16_t two_byte[0];
	};
};
static_assert(sizeof(struct xdr::atom) == 4, "");

struct xdr::binding
{
	uint8_t aliased  : 1;
	uint8_t kind     : 7;
};
static_assert(sizeof(struct xdr::binding) == 1, "");

struct xdr::source
{
	uint32_t start;
	uint32_t end;
	uint32_t lineno;
	uint32_t column;
	uint32_t nfixed;
	uint32_t nslots;

	friend std::ostream &operator<<(std::ostream &, const source &);
};

struct xdr::bytecode
{
	struct info;
	static std::array<struct info, 256> info;

	uint8_t byte;
	uint8_t operand[0];
};

struct xdr::bytecode::info
{
	const char *name;
	uint8_t length;
	uint8_t push;
	uint8_t pop;
};

const struct xdr::bytecode::info &info(const struct xdr::bytecode &);

struct xdr::srcnote
{
	uint8_t note;
};

struct xdr::object
{
	struct block
	{
	};

	struct with
	{
	};

	struct function
	{
		uint32_t scope_index;
		uint32_t first_word;
		uint32_t flags_word;
		//struct xdr xdr;
	};

	struct literal
	{
		uint32_t is_array;
		uint32_t n_properties;

	};

	uint32_t classk; union
	{
		struct block block;
		struct with with;
		struct function function;
		struct literal literal;
	};
};

size_t length(const struct xdr::object::block &);
size_t length(const struct xdr::object::with &);
size_t length(const struct xdr::object::function &);
size_t length(const struct xdr::object::literal &);
size_t length(const struct xdr::object &);

struct xdr::consts
{
	uint32_t tag; union
	{
		uint32_t u32;
		double dbl;
		struct xdr::atom atom;
		struct xdr::object object;
	};
};

size_t length(const struct xdr::consts &consts);

} // namespace js
} // namespace ircd

inline const struct ircd::js::xdr::bytecode::info &
ircd::js::info(const struct xdr::bytecode &bytecode)
{
	return bytecode.info[bytecode.byte];
}
