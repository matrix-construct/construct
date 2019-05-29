// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_MAGICK_API_H

namespace ircd::magick
{
	static void handle_exception(const ::ExceptionType, const char *, const char *);
	static void handle_fatal(const ::ExceptionType, const char *, const char *) noexcept;
	static void handle_error(const ::ExceptionType, const char *, const char *) noexcept;
	static void handle_warning(const ::ExceptionType, const char *, const char *) noexcept;
	static void handle_log(const ::ExceptionType, const char *) noexcept;
	static uint handle_monitor(const char *, const int64_t, const uint64_t, ExceptionInfo *) noexcept;

	template<class R, class F, class... A> static R call(F&&, A&&...);
	template<class R, class F, class... A> static R callex(F&&, A&&...);
	template<class F, class... A> static void callpf(F&&, A&&...);

	static void init();
	static void fini();

	extern log::log log;
}

ircd::mapi::header
IRCD_MODULE
{
	"GraphicsMagick Library support for media manipulation",
	ircd::magick::init,
	ircd::magick::fini
};

decltype(ircd::magick::log)
ircd::magick::log
{
	"magick"
};

//
// init
//

void
ircd::magick::init()
{
	const auto version
	{
		magick::version()
	};

	log::debug
	{
		log, "Initializing Magick Library version inc:%lu [%s] lib:%lu [%s]",
		ulong(MagickLibVersion),
		MagickLibVersionText,
		std::get<0>(version),
		std::get<1>(version),
	};

	if(std::get<0>(version) != ulong(MagickLibVersion))
		log::warning
		{
			log, "Magick Library version mismatch headers:%lu library:%lu",
			ulong(MagickLibVersion),
			std::get<0>(version),
		};

	::InitializeMagick(nullptr);
	::SetLogMethod(handle_log);
	::SetWarningHandler(handle_warning);
	::SetErrorHandler(handle_error);
	::SetFatalErrorHandler(handle_fatal);
	::SetMonitorHandler(handle_monitor);
	::SetMagickResourceLimit(ThreadsResource, 1UL);

	log::debug
	{
		log, "resource settings: pixel max:%lu:%lu height:%lu:%lu width:%lu:%lu; threads:%lu:%lu",
		::GetMagickResource(PixelsResource),
		::GetMagickResourceLimit(PixelsResource),
		::GetMagickResource(HeightResource),
		::GetMagickResourceLimit(HeightResource),
		::GetMagickResource(WidthResource),
		::GetMagickResourceLimit(WidthResource),
		::GetMagickResource(ThreadsResource),
		::GetMagickResourceLimit(ThreadsResource),
	};
}

void
ircd::magick::fini()
{
	log::debug
	{
		"Shutting down Magick Library..."
	};

	::DestroyMagick();
}

std::tuple<ulong, ircd::string_view>
ircd::magick::version()
{
	ulong number(0);
	const char *const string
	{
		::GetMagickVersion(&number)
	};

	return { number, string };
}

//
// thumbnail
//

ircd::magick::thumbnail::thumbnail(const const_buffer &in,
                                   const mutable_buffer &out,
                                   const std::pair<size_t, size_t> &xy)
:const_buffer{[&in, &out, &xy]
{
	const custom_ptr<::ImageInfo> input_info
	{
		::CloneImageInfo(nullptr), ::DestroyImageInfo
	};

	const custom_ptr<::ImageInfo> output_info
	{
		::CloneImageInfo(nullptr), ::DestroyImageInfo
	};

	const custom_ptr<::Image> input
	{
		callex<::Image *>(::BlobToImage, input_info.get(), data(in), size(in)), ::DestroyImage // pollock
	};

	const custom_ptr<::Image> output
	{
		callex<::Image *>(::ThumbnailImage, input.get(), xy.first, xy.second), ::DestroyImage
	};

	size_t output_size(0);
	const auto output_data
	{
		callex<void *>(::ImageToBlob, output_info.get(), output.get(), &output_size)
	};

	const const_buffer result
	{
		reinterpret_cast<char *>(output_data), output_size
	};

	return const_buffer
	{
		data(out), copy(out, result)
	};
}()}
{
}

//
// util
//

template<class return_t,
         class function,
         class... args>
return_t
ircd::magick::callex(function&& f,
                     args&&... a)
{
	const auto error_handler
	{
		::SetErrorHandler(handle_exception)
	};

	const unwind reset{[&]
	{
		::SetErrorHandler(error_handler);
	}};

	::ExceptionInfo ei;
	GetExceptionInfo(&ei); // initializer
	const unwind destroy{[&ei]
	{
		::DestroyExceptionInfo(&ei);
	}};

	const auto ret
	{
		f(std::forward<args>(a)..., &ei)
	};

	// exception comes out of here; if this is not safe we'll have to
	// convey with a global or inspect ExceptionInfo manually.
	::CatchException(&ei);
	return ret;
}

template<class function,
         class... args>
void
ircd::magick::callpf(function&& f,
                     args&&... a)
{
	if(!call(f, std::forward<args>(a)...))
		throw error{};
}

template<class return_t,
         class function,
         class... args>
return_t
ircd::magick::call(function&& f,
                   args&&... a)
{
	return f(std::forward<args>(a)...);
}

uint
ircd::magick::handle_monitor(const char *text,
                             const int64_t quantum,
                             const uint64_t span,
                             ExceptionInfo *ei)
noexcept
{
	log::debug
	{
		log, "progress [%s] %ld:%ld",
		text,
		quantum,
		span
	};

	return true; // return false to interrupt
}

void
ircd::magick::handle_log(const ::ExceptionType type,
                         const char *const message)
noexcept
{
	log::debug
	{
		log, "%s :%s",
		::GetLocaleExceptionMessage(type, nullptr),
		message
	};
}

void
ircd::magick::handle_warning(const ::ExceptionType type,
                             const char *const reason,
                             const char *const description)
noexcept
{
	log::warning
	{
		log, "%s %s :%s",
		::GetLocaleExceptionMessage(type, nullptr),
		reason,
		description
	};
}

void
ircd::magick::handle_error(const ::ExceptionType type,
                           const char *const reason,
                           const char *const description)
noexcept
{
	log::error
	{
		log, "%s %s :%s",
		::GetLocaleExceptionMessage(type, nullptr),
		reason,
		description
	};
}

[[noreturn]] void
ircd::magick::handle_fatal(const ::ExceptionType type,
                           const char *const reason,
                           const char *const description)
noexcept
{
	log::critical
	{
		log, "%s %s :%s",
		::GetLocaleExceptionMessage(type, nullptr),
		reason,
		description
	};

	ircd::terminate();
}

[[noreturn]] void
ircd::magick::handle_exception(const ::ExceptionType type,
                               const char *const reason,
                               const char *const description)
{
	throw error
	{
		"%s %s :%s",
		::GetLocaleExceptionMessage(type, nullptr),
		reason,
		description
	};
}
