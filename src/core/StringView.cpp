#include "StringView.h"

#include <ostream>

namespace HB
{

std::ostream& operator<<(std::ostream& stream, CStringView view)
{
	// An empty view can have nullptr data
	if (!view.IsEmpty())
	{
		stream.write(view.GetData(), static_cast<std::streamsize>(view.GetLength()));
	}
	return stream;
}

} // namespace HB

// The app is compiled with DOCTEST_CONFIG_DISABLE and skips everything below
#ifndef DOCTEST_CONFIG_DISABLE

#include <doctest/doctest.h>

#include <algorithm>
#include <sstream>
#include <string>

using HB::CStringView;

namespace HB::StringView::Internals
{
	// A function that takes a CStringView, to check what converts to one
	CStringView::SizeType CountSpaces(CStringView text)
	{
		return static_cast<CStringView::SizeType>(std::count(text.begin(), text.end(), ' '));
	}
} // namespace HB::StringView::Internals

using HB::StringView::Internals::CountSpaces;

// CStringView works at compile time too, so these are checked when the file
// compiles
static_assert(CStringView("hello").GetLength() == 5);
static_assert(CStringView("hello") == "hello");
static_assert(CStringView("abc") < "abd");
static_assert(CStringView("hello world").SubView(6) == "world");
static_assert(CStringView("hello").StartsWith("he"));
static_assert(CStringView().IsEmpty());

TEST_CASE("CStringView starts empty")
{
	CStringView view;
	CHECK(view.IsEmpty());
	CHECK(view.GetLength() == 0);
	CHECK(view.begin() == view.end());
	CHECK(view == "");
}

TEST_CASE("CStringView constructors")
{
	SUBCASE("from a C string, up to its '\\0'")
	{
		char const* text = "hello";
		CStringView view = text;
		CHECK(view.GetData() == text);
		CHECK(view.GetLength() == 5);
	}

	SUBCASE("from a pointer and a length, which may include '\\0'")
	{
		CStringView view("ab\0cd", 5);
		CHECK(view.GetLength() == 5);
		CHECK(view[2] == '\0');
		CHECK(view[4] == 'd');
	}

	SUBCASE("from a pointer and a length that stops early")
	{
		CStringView view("hello", 2);
		CHECK(view == "he");
	}

	SUBCASE("from nullptr with length 0")
	{
		CStringView view(nullptr, 0);
		CHECK(view.IsEmpty());
	}

	SUBCASE("a function that takes a CStringView accepts a literal")
	{
		CHECK(CountSpaces("a b c") == 2);
	}
}

TEST_CASE("CStringView::operator[] and iteration")
{
	CStringView view = "abc";
	CHECK(view[0] == 'a');
	CHECK(view[2] == 'c');

	std::string copy;
	for (char c : view)
	{
		copy += c;
	}
	CHECK(copy == "abc");
}

TEST_CASE("CStringView::SubView")
{
	CStringView view = "hello world";

	SUBCASE("an offset and a count")
	{
		CStringView sub = view.SubView(6, 3);
		CHECK(sub == "wor");
		CHECK(sub.GetData() == view.GetData() + 6);
	}

	SUBCASE("a count past the end stops at the end")
	{
		CHECK(view.SubView(6, 100) == "world");
	}

	SUBCASE("only an offset goes to the end")
	{
		CHECK(view.SubView(6) == "world");
		CHECK(view.SubView(0) == view);
	}

	SUBCASE("an offset at the end gives an empty view")
	{
		CHECK(view.SubView(view.GetLength()).IsEmpty());
		CHECK(view.SubView(view.GetLength(), 5).IsEmpty());
	}
}

TEST_CASE("CStringView::StartsWith and EndsWith")
{
	CStringView view = "hello world";
	CHECK(view.StartsWith("hello"));
	CHECK(view.StartsWith(""));
	CHECK(view.StartsWith(view));
	CHECK_FALSE(view.StartsWith("world"));
	CHECK_FALSE(view.StartsWith("hello world!"));

	CHECK(view.EndsWith("world"));
	CHECK(view.EndsWith(""));
	CHECK_FALSE(view.EndsWith("hello"));
	CHECK_FALSE(view.EndsWith("!hello world"));

	CHECK(CStringView().StartsWith(""));
	CHECK(CStringView().EndsWith(""));
}

TEST_CASE("CStringView comparison")
{
	CStringView abc = "abc";

	SUBCASE("equality compares the chars, not the pointers")
	{
		char buffer[] = "abc";
		CHECK(abc == CStringView(buffer));
		CHECK(abc == "abc");
		CHECK("abc" == abc);
		CHECK(abc != "abd");
		CHECK(abc != "ab");
		CHECK(abc != "abcd");
	}

	SUBCASE("ordering compares chars one by one, then the length")
	{
		CHECK(abc < "abd");
		CHECK("abd" > abc);
		CHECK(abc < "abcd");
		CHECK("ab" < abc);
		CHECK(CStringView() < abc);
		CHECK((abc <=> "abc") == std::strong_ordering::equal);
	}

	SUBCASE("chars compare as unsigned values")
	{
		// '\x80' is negative as a signed char, but sorts after 'a', like memcmp
		CHECK(CStringView("a") < CStringView("\x80"));
	}

	SUBCASE("'\\0' inside the view counts")
	{
		CStringView withNull("abc\0", 4);
		CHECK(withNull != abc);
		CHECK(abc < withNull);
	}

	SUBCASE("empty views are equal, wherever they point")
	{
		CHECK(CStringView() == CStringView(""));
		CHECK(CStringView(nullptr, 0) == abc.SubView(3));
	}
}

TEST_CASE("CStringView writes to a std::ostream")
{
	std::ostringstream stream;
	stream << CStringView("hello world").SubView(0, 5) << '|' << CStringView("a\0b", 3) << '|' << CStringView();
	CHECK(stream.str() == std::string("hello|a\0b|", 10));
}

#endif // DOCTEST_CONFIG_DISABLE
