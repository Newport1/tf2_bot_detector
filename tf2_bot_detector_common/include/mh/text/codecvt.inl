#ifdef MH_COMPILE_LIBRARY
#include "codecvt.hpp"
#endif

#include <cassert>
#include <cwchar>
#include <stdexcept>
#include <type_traits>

#ifndef MH_HAS_CUCHAR
#define MH_HAS_CUCHAR (__has_include(<cuchar>))
#endif

#if MH_HAS_CUCHAR
#include <cuchar>
#endif

#ifndef MH_COMPILE_LIBRARY_INLINE
#define MH_COMPILE_LIBRARY_INLINE inline
#endif

namespace mh
{
	namespace detail::codecvt_hpp
	{
		template<typename From, typename To, typename TEnable = void> struct change_encoding_impl;

		template<typename T> constexpr bool is_utf_v =
#if MH_HAS_CHAR8
			std::is_same_v<T, char8_t> ||
#endif
#if MH_HAS_UNICODE
			std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t> ||
#endif
			false;

#if MH_HAS_CHAR8
		[[nodiscard]] MH_COMPILE_LIBRARY_INLINE char32_t convert_to_u32(
			const char8_t*& it, const char8_t* end)
		{
			unsigned continuationBytes = 0;

			uint32_t retVal{};

			// https://en.wikipedia.org/wiki/UTF-8#Examples
			const uint8_t firstByte = uint8_t(*it++);
			if ((firstByte & 0b1111'0000) == 0b1111'0000)
			{
				retVal = uint32_t(0b0000'0111 & firstByte) << 18;
				continuationBytes = 3;
			}
			else if ((firstByte & 0b1110'0000) == 0b1110'0000)
			{
				retVal = uint32_t(0b0000'1111 & firstByte) << 12;
				continuationBytes = 2;
			}
			else if ((firstByte & 0b1100'0000) == 0b1100'0000)
			{
				retVal = uint32_t(0b0001'1111 & firstByte) << 6;
				continuationBytes = 1;
			}
			else //if ((firstByte & 0b1000'0000) == 0b0000'0000)
			{
				return char32_t(0b0111'1111 & firstByte);
				//retVal = 0b0111'1111 & firstByte;
				//continuationBytes = 0;
			}

			// Stop at continuationBytes == 0 or reaching the end iterator, whichever is sooner
			for (; it != end && continuationBytes--; ++it)
			{
				retVal |= uint32_t((*it) & 0b0011'1111) << (6 * continuationBytes);
			}

			return char32_t(retVal);
		}
#endif

#if MH_HAS_UNICODE
		MH_COMPILE_LIBRARY_INLINE char32_t convert_to_u32(const char16_t*& it, const char16_t* end)
		{
			// https://en.wikipedia.org/wiki/UTF-16#Description
			constexpr uint16_t TOP6_MASK = 0xFC00;

			const uint16_t firstByte = uint16_t(*it++);
			if ((firstByte & TOP6_MASK) == 0xD800)
			{
				uint32_t retVal = uint32_t(firstByte & (~TOP6_MASK)) << 10;

				if (it != end)
					retVal |= uint32_t(uint32_t(*it++) & (~TOP6_MASK));

				return 0x10000 + retVal;
			}

			return firstByte;
		}

		template<typename T>
		[[nodiscard]] MH_COMPILE_LIBRARY_INLINE constexpr size_t convert_to_u8(char32_t in, T out[4])
		{
			const uint32_t in_raw = in;

			if (in_raw <= 0x7F)
			{
				out[0] = in_raw & 0b0111'1111;
				return 1;
			}
			else if (in_raw <= 0x7FF)
			{
				out[0] = 0b1100'0000 | ((in_raw >> 6) & 0b0001'1111);
				out[1] = 0b1000'0000 | (in_raw & 0b0011'1111);
				return 2;
			}
			else if (in_raw <= 0xFFFF)
			{
				out[0] = 0b1110'0000 | ((in_raw >> 12) & 0b0000'1111);
				out[1] = 0b1000'0000 | ((in_raw >> 6) & 0b0011'1111);
				out[2] = 0b1000'0000 | ((in_raw >> 0) & 0b0011'1111);
				return 3;
			}
			else if (in_raw <= 0x10FFFF)
			{
				out[0] = 0b1111'0000 | ((in_raw >> 18) & 0b0000'0111);
				out[1] = 0b1000'0000 | ((in_raw >> 12) & 0b0011'1111);
				out[2] = 0b1000'0000 | ((in_raw >> 6) & 0b0011'1111);
				out[3] = 0b1000'0000 | ((in_raw >> 0) & 0b0011'1111);
				return 4;
			}

			return -1;
		}
		MH_COMPILE_LIBRARY_INLINE size_t convert_to_u16(char32_t in, char16_t out[2])
		{
			constexpr uint16_t TOP6_MASK = 0xFC00;
			constexpr uint16_t BYTE0_MARKER = 0xD800;
			constexpr uint16_t BYTE1_MARKER = 0xDC00;

			uint32_t in_raw = in;
			if (in_raw > 0xFFFF || ((in_raw & TOP6_MASK) == BYTE0_MARKER))
			{
				in_raw -= 0x10000;

				// Two bytes
				out[0] = BYTE0_MARKER | ((in_raw >> 10) & (~TOP6_MASK));
				out[1] = BYTE1_MARKER | (in_raw & (~TOP6_MASK));
				return 2;
			}
			else
			{
				// One byte
				out[0] = in_raw & 0xFFFF;
				return 1;
			}
		}

		template<typename From, typename To>
		struct change_encoding_impl<From, To, std::enable_if_t<!std::is_same_v<From, To>&& is_utf_v<From>&& is_utf_v<To>>>
		{
			std::basic_string<To> operator()(const From* begin, const From* end) const
			{
				std::basic_string<To> retVal;

				for (auto it = begin; it != end; )
				{
					char32_t u32;

					if constexpr (std::is_same_v<From, char32_t>)
						u32 = *it++;
					else
						u32 = convert_to_u32(it, end);

#if MH_HAS_CHAR8
					if constexpr (std::is_same_v<To, char8_t>)
					{
						char8_t buf[4];
						const size_t chars = convert_to_u8(u32, buf);
						retVal.append(buf, chars);
					}
					else
#endif
						if constexpr (std::is_same_v<To, char16_t>)
					{
						char16_t buf[2];
						const auto chars = convert_to_u16(u32, buf);
						retVal.append(buf, chars);
					}
					else //if constexpr (std::is_same_v<To, char32_t>)
					{
						retVal += u32;
					}
				}

				return retVal;
			}
		};
#endif // MH_HAS_UNICODE

#if MH_HAS_CHAR8
		// UTF-8 to UTF-8: identity byte copy. char is UTF-8 in this library.
		template<>
		struct change_encoding_impl<char8_t, char>
		{
			std::basic_string<char> operator()(const char8_t* begin, const char8_t* end) const
			{
				return std::basic_string<char>(
					reinterpret_cast<const char*>(begin),
					static_cast<std::size_t>(end - begin));
			}
		};

		template<>
		struct change_encoding_impl<char, char8_t>
		{
			std::basic_string<char8_t> operator()(const char* begin, const char* end) const
			{
				return std::basic_string<char8_t>(
					reinterpret_cast<const char8_t*>(begin),
					static_cast<std::size_t>(end - begin));
			}
		};
#endif

#if MH_HAS_UNICODE
		template<typename From>
		struct change_encoding_impl<From, char, std::enable_if_t<is_utf_v<From>
#if MH_HAS_CHAR8
			&& !std::is_same_v<From, char8_t>
#endif
			>>
		{
			std::string operator()(const From* begin, const From* end) const
			{
				const auto u8 = change_encoding_impl<From, char8_t>{}(begin, end);
				return change_encoding_impl<char8_t, char>{}(u8.data(), u8.data() + u8.size());
			}
		};

		template<typename To>
		struct change_encoding_impl<char, To, std::enable_if_t<is_utf_v<To>
#if MH_HAS_CHAR8
			&& !std::is_same_v<To, char8_t>
#endif
			>>
		{
			std::basic_string<To> operator()(const char* begin, const char* end) const
			{
				const auto u8 = change_encoding_impl<char, char8_t>{}(begin, end);
				return change_encoding_impl<char8_t, To>{}(u8.data(), u8.data() + u8.size());
			}
		};
#endif

		template<> struct change_encoding_impl<char, wchar_t>
		{
			std::basic_string<wchar_t> operator()(const char* begin, const char* end) const
			{
				// wchar_t is UTF-16 on Windows and UTF-32 everywhere else, so the code
				// unit values are identical to char16_t / char32_t. Copy element-wise
				// rather than reinterpret_cast-ing the buffer: this is built with -flto
				// and strict aliasing on, which is exactly where type punning between
				// distinct types is allowed to miscompile.
				if constexpr (sizeof(wchar_t) == 2)
				{
					const auto converted = change_encoding_impl<char, char16_t>{}(begin, end);
					return std::wstring(converted.begin(), converted.end());
				}
				else
				{
					const auto converted = change_encoding_impl<char, char32_t>{}(begin, end);
					return std::wstring(converted.begin(), converted.end());
				}
			}
		};

		template<> struct change_encoding_impl<wchar_t, char>
		{
			std::basic_string<char> operator()(const wchar_t* begin, const wchar_t* end) const
			{
				// Element-wise copy, not a reinterpret_cast of the buffer -- see the note
				// in change_encoding_impl<char, wchar_t> above.
				if constexpr (sizeof(wchar_t) == 2)
				{
					const std::u16string converted(begin, end);
					return change_encoding_impl<char16_t, char>{}(
						converted.data(), converted.data() + converted.size());
				}
				else
				{
					const std::u32string converted(begin, end);
					return change_encoding_impl<char32_t, char>{}(
						converted.data(), converted.data() + converted.size());
				}
			}
		};

		template<typename From>
		struct change_encoding_impl<From, wchar_t, std::enable_if_t<!std::is_same_v<wchar_t, From>>>
		{
			std::basic_string<wchar_t> operator()(const From* begin, const From* end) const
			{
				auto converted = change_encoding_impl<From, char>{}(begin, end);
				return change_encoding_impl<char, wchar_t>{}(converted.data(), converted.data() + converted.size());
			}
		};

		template<typename To>
		struct change_encoding_impl<wchar_t, To, std::enable_if_t<!std::is_same_v<wchar_t, To>>>
		{
			std::basic_string<To> operator()(const wchar_t* begin, const wchar_t* end) const
			{
				auto converted = change_encoding_impl<wchar_t, char>{}(begin, end);
				return change_encoding_impl<char, To>{}(converted.data(), converted.data() + converted.size());
			}
		};
		template<typename T>
		struct change_encoding_impl<T, T, std::enable_if_t<std::is_same_v<T, T>>>
		{
			std::basic_string<T> operator()(const T* begin, const T* end) const
			{
				return std::basic_string<T>(begin, end - begin);
			};
		};
	}

	template<typename To, typename From, typename FromTraits>
	MH_COMPILE_LIBRARY_INLINE std::basic_string<To> change_encoding(const std::basic_string_view<From, FromTraits>& input)
	{
		const detail::codecvt_hpp::template change_encoding_impl<From, To> impl;
		return impl(input.data(), input.data() + input.size());
	}

#ifdef MH_COMPILE_LIBRARY
	template std::string change_encoding<char, char>(const std::string_view&);
	template std::string change_encoding<char, wchar_t>(const std::wstring_view&);

	template std::wstring change_encoding<wchar_t, char>(const std::string_view&);
	template std::wstring change_encoding<wchar_t, wchar_t>(const std::wstring_view&);

#if MH_HAS_UNICODE
#if MH_HAS_CUCHAR
	template std::u16string change_encoding<char16_t, char>(const std::string_view&);
	template std::u16string change_encoding<char16_t, wchar_t>(const std::wstring_view&);
	template std::u32string change_encoding<char32_t, char>(const std::string_view&);
	template std::u32string change_encoding<char32_t, wchar_t>(const std::wstring_view&);

	template std::string change_encoding<char, char16_t>(const std::u16string_view&);
	template std::string change_encoding<char, char32_t>(const std::u32string_view&);
	template std::wstring change_encoding<wchar_t, char16_t>(const std::u16string_view&);
	template std::wstring change_encoding<wchar_t, char32_t>(const std::u32string_view&);
#endif  // MH_HAS_CUCHAR

	template std::u16string change_encoding<char16_t, char16_t>(const std::u16string_view&);
	template std::u16string change_encoding<char16_t, char32_t>(const std::u32string_view&);
	template std::u32string change_encoding<char32_t, char16_t>(const std::u16string_view&);
	template std::u32string change_encoding<char32_t, char32_t>(const std::u32string_view&);

#if MH_HAS_CHAR8
#if MH_HAS_CUCHAR
	template std::u8string change_encoding<char8_t, char>(const std::string_view&);
	template std::u8string change_encoding<char8_t, wchar_t>(const std::wstring_view&);

	template std::string change_encoding<char, char8_t>(const std::u8string_view&);
	template std::wstring change_encoding<wchar_t, char8_t>(const std::u8string_view&);
#endif  // MH_HAS_CUCHAR

	template std::u8string change_encoding<char8_t, char8_t>(const std::u8string_view&);
	template std::u8string change_encoding<char8_t, char16_t>(const std::u16string_view&);
	template std::u8string change_encoding<char8_t, char32_t>(const std::u32string_view&);

	template std::u16string change_encoding<char16_t, char8_t>(const std::u8string_view&);

	template std::u32string change_encoding<char32_t, char8_t>(const std::u8string_view&);
#endif  // MH_HAS_CHAR8
#endif  // MH_HAS_UNICODE
#endif  // MH_COMPILE_LIBRARY
}
