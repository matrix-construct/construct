// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/js/js.h>
#include <js/Initialization.h>
#include <mozilla/ThreadLocal.h>

namespace ircd {
namespace js   {

decltype(xdr::bytecode::info) xdr::bytecode::info
{
	0
};

} // namespace js
} // namespace ircd

std::ostream &
ircd::js::operator<<(std::ostream &s, const struct xdr::header &h)
{
	s << std::setw(34) << std::left << "build_id_length" << h.build_id_length << std::endl;
	s << std::setw(34) << std::left << "build_id" << h.build_id << std::endl;
	s << std::setw(34) << std::left << "length" << h.length << std::endl;
	s << std::setw(34) << std::left << "prologue_length" << h.prologue_length << std::endl;
	s << std::setw(34) << std::left << "version" << h.version << std::endl;
	s << std::setw(34) << std::left << "n_atoms" << h.n_atoms << std::endl;
	s << std::setw(34) << std::left << "n_srcnotes" << h.n_srcnotes << std::endl;
	s << std::setw(34) << std::left << "n_consts" << h.n_consts << std::endl;
	s << std::setw(34) << std::left << "n_objects" << h.n_objects << std::endl;
	s << std::setw(34) << std::left << "n_scopes" << h.n_scopes << std::endl;
	s << std::setw(34) << std::left << "n_try_notes" << h.n_try_notes << std::endl;
	s << std::setw(34) << std::left << "n_scope_notes" << h.n_scope_notes << std::endl;
	s << std::setw(34) << std::left << "n_yield_offsets" << h.n_yield_offsets << std::endl;
	s << std::setw(34) << std::left << "n_typesets" << h.n_typesets << std::endl;
	s << std::setw(34) << std::left << "fun_length" << h.fun_length << std::endl;
	s << std::setw(34) << std::left << "script_bits" << h.script_bits << std::endl;
	return s;
}

std::ostream &
ircd::js::operator<<(std::ostream &s, const struct xdr::sourcecode &h)
{
	s << std::setw(34) << std::left << "has_source" << bool(h.has_source) << std::endl;
	s << std::setw(34) << std::left << "retrievable" << bool(h.retrievable) << std::endl;
	s << std::setw(34) << std::left << "length" << h.length << std::endl;
	s << std::setw(34) << std::left << "compressed_length" << h.compressed_length << std::endl;
	s << std::setw(34) << std::left << "arguments_not_included" << bool(h.arguments_not_included) << std::endl;
	return s;
}

std::ostream &
ircd::js::operator<<(std::ostream &s, const struct xdr::source &h)
{
	s << std::setw(34) << std::left << "start" << h.start << std::endl;
	s << std::setw(34) << std::left << "end" << h.end << std::endl;
	s << std::setw(34) << std::left << "lineno" << h.lineno << std::endl;
	s << std::setw(34) << std::left << "column" << h.column << std::endl;
	s << std::setw(34) << std::left << "nfixed" << h.nfixed << std::endl;
	s << std::setw(34) << std::left << "nslots" << h.nslots << std::endl;
	return s;
}

__attribute__((optimize("-O0")))
ircd::js::xdr::xdr(const const_buffer &buf)
:header
{
	reinterpret_cast<const struct header *>(data(buf))
}
,sourcecode{[this]() -> const struct sourcecode *
{
	if(this->header->script_bits & ~(1 << 14))
		return nullptr;

	return reinterpret_cast<const struct sourcecode *>(this->header + 1);
}()}
,sourcemap{[this]
{
	//assert(this->sourcecode->has_source);
	assert(!this->sourcecode->compressed_length);
	const auto *const ptr
	{
		reinterpret_cast<const uint8_t *>(this->sourcecode->code + this->sourcecode->length)
	};

	return reinterpret_cast<const struct sourcemap *>(ptr);
}()}
,displayurl{[this]
{
	if(!this->sourcemap->have)
	{
		const auto ptr(reinterpret_cast<const uint8_t *>(this->sourcemap) + 1);
		return reinterpret_cast<const struct displayurl *>(ptr);
	}

	const auto ptr(reinterpret_cast<const uint8_t *>(this->sourcemap->url + this->sourcemap->len));
	return reinterpret_cast<const struct displayurl *>(ptr);
}()}
,filename{[this]
{
	if(!this->displayurl->have)
	{
		const auto ptr(reinterpret_cast<const uint8_t *>(this->displayurl) + 1);
		return reinterpret_cast<const struct filename *>(ptr);
	}

	const auto ptr(reinterpret_cast<const uint8_t *>(this->displayurl->url + this->displayurl->len));
	return reinterpret_cast<const struct filename *>(ptr);
}()}
,source{[this]
{
	if(!this->filename->have)
	{
		const auto ptr(reinterpret_cast<const uint8_t *>(this->filename) + 1);
		return reinterpret_cast<const struct source *>(ptr);
	}

	const auto ptr
	{
		reinterpret_cast<const uint8_t *>(this->filename->name + strlen(this->filename->name))
	};

	return reinterpret_cast<const struct source *>(ptr);
}()}
,body_scope_index{[this]
{
	return reinterpret_cast<const uint32_t *>(this->source + 1);
}()}
,bytecode{[this]
{
	return reinterpret_cast<const struct bytecode *>(this->body_scope_index + 1);
}()}
,srcnote{[this]
{
	const auto ptr
	{
		reinterpret_cast<const uint8_t *>(this->bytecode) + this->header->length
	};

	return reinterpret_cast<const struct srcnote *>(ptr);
}()}
,atom{[this]
{
	const auto ptr
	{
		this->srcnote + this->header->n_srcnotes
	};

	return reinterpret_cast<const struct atom *>(ptr);
}()}
,consts{[this]
{
	const auto *ptr
	{
		reinterpret_cast<const uint8_t *>(this->atom)
	};

	for_each_atom([&ptr](const struct atom &atom)
	{
		ptr += sizeof(struct atom) + atom.length;
	});

	return reinterpret_cast<const struct consts *>(ptr);
}()}
/*
,name
{
	reinterpret_cast<const struct atom *>(data(buf) + sizeof(struct header))
}
,binding{[this]
{
	const auto *ret
	{
		reinterpret_cast<const uint8_t *>(this->name)
	};

	for_each_name([&ret](const struct atom &atom)
	{
		ret += sizeof(struct atom) + atom.length;
	});

	return reinterpret_cast<const struct binding *>(ret);
}()}
,object{[this]
{
	const auto *ptr(reinterpret_cast<const uint8_t *>(this->consts));
	for_each_const([&ptr](const struct consts &c)
	{
		ptr += length(c);
	});

	return reinterpret_cast<const struct object *>(ptr);
}()}
*/
{
}

void
ircd::js::xdr::for_each_object(const std::function<void (const struct object &)> &cb)
const
{
	auto object(this->object);
	const auto *ptr(reinterpret_cast<const uint8_t *>(object));
	for(size_t i(0); i < header->n_objects; i++)
	{
		cb(*object);
		ptr += length(*object);
		object = reinterpret_cast<const struct object *>(ptr);
	}
}

void
ircd::js::xdr::for_each_const(const std::function<void (const struct consts &)> &cb)
const
{
	auto consts(this->consts);
	const auto *ptr
	{
		reinterpret_cast<const uint8_t *>(this->consts)
	};

	for(size_t i(0); i< header->n_consts; i++)
	{
		cb(*consts);
		ptr += length(*consts);
		consts = reinterpret_cast<const struct consts *>(ptr);
	}
}

void
ircd::js::xdr::for_each_bytecode(const std::function<void (const struct bytecode &)> &cb)
const
{
	const uint8_t *const start(reinterpret_cast<const uint8_t *>(bytecode));
	const uint8_t *ptr(reinterpret_cast<const uint8_t *>(bytecode));
	for(; ptr < start + header->length;)
	{
		const auto &bytecode(*reinterpret_cast<const struct bytecode *>(ptr));
		ptr += info(bytecode).length;
		cb(bytecode);
	}
}

void
ircd::js::xdr::for_each_binding(const std::function<void (const struct binding &)> &cb)
const
{
	const auto *binding(this->binding);
	const size_t bindings(header->num_names());
	for(; binding < this->binding + bindings; ++binding)
		cb(*binding);
}

void
ircd::js::xdr::for_each_atom(const std::function<void (const struct atom &)> &cb)
const
{
	const uint8_t *ptr(reinterpret_cast<const uint8_t *>(atom));
	for(size_t i(0); i < header->n_atoms; ++i)
	{
		const auto &atom
		{
			*reinterpret_cast<const struct atom *>(ptr)
		};

		cb(atom);
        ptr += sizeof(struct atom) + atom.length;
        static_assert(sizeof(struct atom) == 4);
    }
}

size_t
ircd::js::xdr::header::num_bindings()
const
{
	return num_names();
}

size_t
ircd::js::xdr::header::num_names()
const
{
	return 0;//return n_args + n_vars + n_body_level_lexicals;
}


size_t
ircd::js::length(const struct xdr::consts &consts)
{
	switch(consts.tag)
	{
		case 0:   return 4 + 4;         // ::js::SCRIPT_INT
		case 1:   return 4 + 8;         // ::js::SCRIPT_DOUBLE
		case 2:                         // ::js::SCRIPT_ATOM
			return 4 + 4;
			//return 4 + 4 + consts.atom.length;

		case 3:   return 4 + 0;         // ::js::SCRIPT_TRUE
		case 4:   return 4 + 0;         // ::js::SCRIPT_FALSE
		case 5:   return 4 + 0;         // ::js::SCRIPT_NULL
		case 6:                         // ::js::SCRIPT_OBJECT
			throw error("unsupported consts (object)");

		case 7:   return 4 + 0;         // ::js::SCRIPT_VOID
		case 8:   return 4 + 0;         // ::js::SCRIPT_HOLE

		default:
			throw error("unsupported consts tag [%u]", consts.tag);
	}
}

size_t
ircd::js::length(const struct xdr::object &object)
{
	switch(object.classk)
	{
		case 0:     return 4 + length(object.block);
		case 1:     return 4 + length(object.with);
		case 2:     return 4 + length(object.function);
		case 3:     return 4 + length(object.literal);
		default:
			throw error("unsupported object class kind [%u]", object.classk);
	}
}

size_t
ircd::js::length(const struct xdr::object::literal &literal)
{
	size_t ret(0);
	const auto *ptr(reinterpret_cast<const uint8_t *>(&literal) + 8);
	for(size_t i(0); i < literal.n_properties; ++i)
	{
		const auto prop(reinterpret_cast<const struct xdr::consts *>(ptr + ret));
		printf("i[%zu] atom [%u] length [%u]\n", i, prop->u32, prop->atom.length);
		const auto len(length(*prop));
		ret += 8;
	}

	return 8 + ret;
}

size_t
ircd::js::length(const struct xdr::object::function &function)
{
	return 12;
}

size_t
ircd::js::length(const struct xdr::object::with &with)
{
	return 0;
}

size_t
ircd::js::length(const struct xdr::object::block &block)
{
	return 0;
}

__attribute__((constructor))
void
init_opcodes()
{
	using xdr = ircd::js::xdr;

	#define IRCD_JS_INIT_OP(code, value, name, image, len, nuses, ndefs, format) \
		xdr::bytecode::info[value] = { name, uint8_t(len), uint8_t(ndefs), uint8_t(nuses) };

    static const auto js_undefined_str("");
    static const auto js_false_str("false");
    static const auto js_true_str("false");
    static const auto js_void_str("void");
    static const auto js_null_str("null");
    static const auto js_throw_str("throw");
    static const auto js_in_str("in");
    static const auto js_instanceof_str("instanceOf");
    static const auto js_typeof_str("typeOf");

	FOR_EACH_OPCODE(IRCD_JS_INIT_OP);
}
