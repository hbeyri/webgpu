#include "String.h"

#include "Abort.h"

#include <cstring>
#include <functional>
#include <limits>

namespace HB
{

CString::CString(char const* text)
{
	if (text == nullptr)
	{
		Abort();
	}
	Append(std::string_view(text));
}

CString::CString(char const* text, SizeType length)
{
	if (text == nullptr && length > 0)
	{
		Abort();
	}
	Append(std::string_view(text, length));
}

CString::CString(std::string_view text)
{
	Append(text);
}

CString::SizeType CString::GetLength() const
{
	return m_Chars.IsEmpty() ? 0 : m_Chars.GetSize() - 1;
}

CString::SizeType CString::GetCapacity() const
{
	return m_Chars.GetCapacity() == 0 ? 0 : m_Chars.GetCapacity() - 1;
}

void CString::Reserve(SizeType capacity)
{
	if (capacity > 0)
	{
		m_Chars.Reserve(capacity + 1);
	}
}

char const* CString::GetCString() const
{
	return m_Chars.IsEmpty() ? "" : m_Chars.GetData();
}

char& CString::operator[](SizeType index)
{
	CheckIndex(index);
	return m_Chars[index];
}

char const& CString::operator[](SizeType index) const
{
	CheckIndex(index);
	return m_Chars[index];
}

void CString::Append(std::string_view text)
{
	if (text.empty())
	{
		return;
	}

	SizeType const oldLength = GetLength();
	if (text.size() > std::numeric_limits<SizeType>::max() - 1 - oldLength)
	{
		// The length and the '\0' wouldn't fit in SizeType
		Abort();
	}
	SizeType const newLength = oldLength + text.size();

	// The text may be part of this string, and growing moves the chars to new
	// memory. Remember where it is, to find it again afterwards.
	// std::less gives a total order even for pointers into different objects.
	char const* const oldData = m_Chars.GetData();
	std::less<char const*> const less;
	bool const isOwnText = !m_Chars.IsEmpty() && !less(text.data(), oldData)
		&& less(text.data(), oldData + m_Chars.GetSize());
	SizeType const ownOffset = isOwnText ? static_cast<SizeType>(text.data() - oldData) : 0;

	GrowFor(newLength);
	// Doesn't allocate after GrowFor. The new chars start as '\0', so the
	// last one is already the '\0' at the end.
	m_Chars.Resize(newLength + 1);

	char const* const source = isOwnText ? m_Chars.GetData() + ownOffset : text.data();
	// memmove, not memcpy: own text that includes the old '\0' overlaps the
	// first char written
	std::memmove(m_Chars.GetData() + oldLength, source, text.size());
}

void CString::Append(char c)
{
	Append(std::string_view(&c, 1));
}

char* CString::begin()
{
	return m_Chars.GetData();
}

char* CString::end()
{
	return m_Chars.GetData() + GetLength();
}

char const* CString::begin() const
{
	return GetCString();
}

char const* CString::end() const
{
	return GetCString() + GetLength();
}

void CString::GrowFor(SizeType length)
{
	SizeType const needed = length + 1;
	SizeType const capacity = m_Chars.GetCapacity();
	if (needed > capacity)
	{
		m_Chars.Reserve(needed > capacity * 2 ? needed : capacity * 2);
	}
}

void CString::CheckIndex(SizeType index) const
{
	if (index >= GetLength())
	{
		Abort();
	}
}

} // namespace HB

// The app is compiled with DOCTEST_CONFIG_DISABLE and skips everything below
#ifndef DOCTEST_CONFIG_DISABLE

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <utility>

using HB::CString;

namespace HB::String::Internals
{
	// A function that takes a std::string_view, to check that a CString can be
	// passed to it
	std::size_t CountSpaces(std::string_view text)
	{
		return static_cast<std::size_t>(std::count(text.begin(), text.end(), ' '));
	}
} // namespace HB::String::Internals

using HB::String::Internals::CountSpaces;

TEST_CASE("CString starts empty")
{
	CString text;
	CHECK(text.IsEmpty());
	CHECK(text.GetLength() == 0);
	CHECK(text.GetCapacity() == 0);
	REQUIRE(text.GetCString() != nullptr);
	CHECK(text.GetCString()[0] == '\0');
	CHECK(text == "");
	CHECK(text.begin() == text.end());
}

TEST_CASE("CString constructors")
{
	SUBCASE("from a C string")
	{
		CString text = "hello";
		CHECK(text.GetLength() == 5);
		CHECK(std::strcmp(text.GetCString(), "hello") == 0);
	}

	SUBCASE("from an empty C string")
	{
		CString text = "";
		CHECK(text.IsEmpty());
		CHECK(text.GetCapacity() == 0);
	}

	SUBCASE("from a pointer and a length, which may include '\\0'")
	{
		CString text("ab\0cd", 5);
		CHECK(text.GetLength() == 5);
		CHECK(text[2] == '\0');
		CHECK(text[4] == 'd');
		CHECK(text.GetCString()[5] == '\0');
	}

	SUBCASE("from nullptr with length 0")
	{
		CString text(nullptr, 0);
		CHECK(text.IsEmpty());
	}

	SUBCASE("from a std::string_view")
	{
		std::string_view view = "hello world";
		CString text(view.substr(6));
		CHECK(text == "world");
	}
}

TEST_CASE("CString::Append")
{
	CString text = "abc";

	SUBCASE("a C string, a CString and a char")
	{
		text.Append("de");
		text.Append(CString("fg"));
		text.Append('h');
		CHECK(text == "abcdefgh");
		CHECK(text.GetLength() == 8);
		CHECK(text.GetCString()[8] == '\0');
	}

	SUBCASE("an empty string changes nothing")
	{
		text.Append("");
		CHECK(text == "abc");
	}

	SUBCASE("to an empty string")
	{
		CString empty;
		empty.Append("x");
		CHECK(empty == "x");
	}

	SUBCASE("many chars, one at a time")
	{
		CString digits;
		for (int i = 0; i < 1000; ++i)
		{
			digits.Append(static_cast<char>('0' + i % 10));
		}
		CHECK(digits.GetLength() == 1000);
		CHECK(digits.GetCapacity() >= 1000);
		CHECK(digits[999] == '9');
		CHECK(digits.GetCString()[1000] == '\0');
	}

	SUBCASE("the string itself, while it grows")
	{
		REQUIRE(text.GetCapacity() == text.GetLength());
		text.Append(text);
		CHECK(text == "abcabc");
	}

	SUBCASE("part of the string itself, while it grows")
	{
		text.Append(text.GetView().substr(1, 2));
		CHECK(text == "abcbc");
	}

	SUBCASE("the string itself including its '\\0'")
	{
		text.Append(std::string_view(text.GetCString(), text.GetLength() + 1));
		CHECK(text.GetLength() == 7);
		CHECK(text.GetView() == std::string_view("abcabc\0", 7));
		CHECK(text.GetCString()[7] == '\0');
	}

	SUBCASE("the string itself, without growing")
	{
		text.Reserve(10);
		char const* data = text.GetCString();
		text.Append(text);
		CHECK(text == "abcabc");
		CHECK(text.GetCString() == data);
	}
}

TEST_CASE("CString += and +")
{
	CString text = "a";
	text += "b";
	text += 'c';
	text += CString("d");
	CHECK(text == "abcd");

	CHECK(text + "e" == "abcde");
	CHECK(text + 'e' == "abcde");
	CHECK(text + text == "abcdabcd");
	CHECK("x" + text == "xabcd");
	CHECK(text == "abcd");
}

TEST_CASE("CString::Reserve and Clear")
{
	CString text = "abc";

	SUBCASE("Reserve keeps the text")
	{
		text.Reserve(100);
		CHECK(text.GetCapacity() >= 100);
		CHECK(text == "abc");
	}

	SUBCASE("appending up to the reserved capacity doesn't move the text")
	{
		text.Reserve(100);
		char const* data = text.GetCString();
		while (text.GetLength() < 100)
		{
			text.Append('x');
		}
		CHECK(text.GetCString() == data);
	}

	SUBCASE("Reserve never makes the capacity smaller")
	{
		text.Reserve(1);
		CHECK(text.GetCapacity() == 3);
	}

	SUBCASE("Clear empties the string and keeps the capacity")
	{
		text.Clear();
		CHECK(text.IsEmpty());
		CHECK(text == "");
		CHECK(text.GetCString()[0] == '\0');
		CHECK(text.GetCapacity() == 3);
		text.Append("de");
		CHECK(text == "de");
	}
}

TEST_CASE("CString::operator[] and iteration")
{
	CString text = "abc";
	CHECK(text[0] == 'a');
	text[1] = 'X';
	CHECK(text == "aXc");

	std::string copy;
	for (char c : text)
	{
		copy += c;
	}
	CHECK(copy == "aXc");

	std::reverse(text.begin(), text.end());
	CHECK(text == "cXa");

	CString const& constText = text;
	CHECK(std::string(constText.begin(), constText.end()) == "cXa");
}

TEST_CASE("CString comparison")
{
	CString abc = "abc";
	CString abd = "abd";

	CHECK(abc == "abc");
	CHECK("abc" == abc);
	CHECK(abc == CString("abc"));
	CHECK(abc == std::string_view("abc"));
	CHECK(abc != abd);
	CHECK(abc != "ab");
	CHECK(abc != "abcd");

	CHECK(abc < abd);
	CHECK(abd > abc);
	CHECK(abc < "abcd");
	CHECK("ab" < abc);
	CHECK((abc <=> CString("abc")) == std::strong_ordering::equal);

	SUBCASE("'\\0' inside the text counts")
	{
		CString withNull("abc\0", 4);
		CHECK(withNull != abc);
		CHECK(abc < withNull);
	}
}

TEST_CASE("CString converts to std::string_view")
{
	CString text = "a b c";
	CHECK(CountSpaces(text) == 2);
	std::string_view view = text;
	CHECK(view == "a b c");
	CHECK(view.data() == text.GetCString());
}

TEST_CASE("CString copies and moves")
{
	CString text = "hello";

	SUBCASE("a copy has its own chars")
	{
		CString copy = text;
		copy[0] = 'j';
		CHECK(text == "hello");
		CHECK(copy == "jello");
	}

	SUBCASE("a move takes the chars and leaves the source empty")
	{
		char const* data = text.GetCString();
		CString moved = std::move(text);
		CHECK(moved.GetCString() == data);
		CHECK(text.IsEmpty()); // NOLINT(bugprone-use-after-move)
		CHECK(text.GetCString()[0] == '\0');
	}

	SUBCASE("assignment from a C string")
	{
		text = "bye";
		CHECK(text == "bye");
	}

	SUBCASE("in a TArray")
	{
		HB::TArray<CString> words = {"one", "two"};
		for (int i = 0; i < 20; ++i)
		{
			words.PushBack(words[0] + "!");
		}
		CHECK(words[1] == "two");
		CHECK(words[21] == "one!");
	}
}

#endif // DOCTEST_CONFIG_DISABLE
