#include <mh/text/codecvt.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

namespace
{
	std::string_view AsBytes(const std::u8string_view& u8)
	{
		return std::string_view(reinterpret_cast<const char*>(u8.data()), u8.size());
	}

	void CheckEncoding(const char8_t* u8lit)
	{
		const std::u8string_view u8{ u8lit };

		std::string asChar;
		REQUIRE_NOTHROW(asChar = mh::change_encoding<char>(u8));
		REQUIRE(asChar == AsBytes(u8));

		const auto backToU8 = mh::change_encoding<char8_t>(asChar);
		REQUIRE(backToU8 == u8);

		const auto asU16 = mh::change_encoding<char16_t>(asChar);
		const auto backToChar = mh::change_encoding<char>(std::u16string_view(asU16));
		REQUIRE(backToChar == asChar);
	}
}

TEST_CASE("change_encoding ASCII", "[encoding]")
{
	CheckEncoding(u8"hello");
}

TEST_CASE("change_encoding Latin-1", "[encoding]")
{
	CheckEncoding(u8"\u00E9"); // é
}

TEST_CASE("change_encoding CJK", "[encoding]")
{
	CheckEncoding(u8"\u65E5\u672C\u8A9E"); // 日本語
}

TEST_CASE("change_encoding astral-plane emoji", "[encoding]")
{
	CheckEncoding(u8"\U0001F621"); // 😡
}
