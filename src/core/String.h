#pragma once

#include "SmallArray.h"
#include "StringView.h"

#include <compare>
#include <cstddef>

// A string of chars that owns its memory, like std::string. The text is always
// followed by a '\0', so GetCString() can be passed to C functions.
//
//     HB::CString name = "Hello";
//     name += ", world";
//     name.Append('!');
//     if (name == "Hello, world!")
//     {
//         puts(name.GetCString());
//     }
//
// A CString converts to CStringView, so it can be passed to functions
// that take one. Using an index past the end stops the program.
//
// Small string optimization: a string of up to GetInlineCapacity() chars is
// kept inside the CString itself, without allocating. A longer one moves to
// the heap, and stays there even if it gets shorter again.
namespace HB
{

class CString
{
public:
	using SizeType = std::size_t;

	CString() = default;

	// Not explicit, so that `CString name = "text";` works. Stops the program
	// if text is nullptr.
	CString(char const* text);

	// The first length chars of text, which may contain '\0'
	CString(char const* text, SizeType length);

	explicit CString(CStringView text);

	// The number of chars, not counting the '\0' at the end
	SizeType GetLength() const;

	bool IsEmpty() const
	{
		return GetLength() == 0;
	}

	// The number of chars the string can hold before it has to allocate more
	// memory, not counting the '\0' at the end. Never less than
	// GetInlineCapacity().
	SizeType GetCapacity() const;

	// The longest string kept inside the CString without allocating
	static constexpr SizeType GetInlineCapacity()
	{
		return InlineChars - 1;
	}

	// Makes room for at least capacity chars, so that appending up to that
	// length doesn't allocate. Never makes the capacity smaller.
	void Reserve(SizeType capacity);

	// Makes the string empty. Keeps the memory, so the capacity doesn't change.
	void Clear()
	{
		m_Chars.Clear();
	}

	// The text followed by '\0'. Valid until the string changes or is
	// destroyed.
	char const* GetCString() const;

	CStringView GetView() const
	{
		return CStringView(GetCString(), GetLength());
	}

	operator CStringView() const
	{
		return GetView();
	}

	// The char at index. Stops the program if index is past the end.
	char& operator[](SizeType index);
	char const& operator[](SizeType index) const;

	// Adds text at the end. The text can be part of this string, like
	// `name.Append(name)`.
	void Append(CStringView text);
	void Append(char c);

	CString& operator+=(CStringView text)
	{
		Append(text);
		return *this;
	}

	CString& operator+=(char c)
	{
		Append(c);
		return *this;
	}

	friend CString operator+(CString left, CStringView right)
	{
		left.Append(right);
		return left;
	}

	friend CString operator+(CString left, char right)
	{
		left.Append(right);
		return left;
	}

	// Compare with another CString, a C string or a CStringView. Ordering
	// compares chars one by one, like CStringView. Each kind of operand
	// has its own overload, so that each comparison has one best match.
	friend bool operator==(CString const& left, CString const& right)
	{
		return left.GetView() == right.GetView();
	}

	friend bool operator==(CString const& left, CStringView right)
	{
		return left.GetView() == right;
	}

	friend bool operator==(CString const& left, char const* right)
	{
		return left.GetView() == CStringView(right);
	}

	friend std::strong_ordering operator<=>(CString const& left, CString const& right)
	{
		return left.GetView() <=> right.GetView();
	}

	friend std::strong_ordering operator<=>(CString const& left, CStringView right)
	{
		return left.GetView() <=> right;
	}

	friend std::strong_ordering operator<=>(CString const& left, char const* right)
	{
		return left.GetView() <=> CStringView(right);
	}

	// For range-based for loops and standard algorithms
	char* begin();
	char* end();
	char const* begin() const;
	char const* end() const;

private:
	// Makes room for at least length chars, growing the capacity by at least
	// double so that appending in a loop doesn't copy the text every time
	void GrowFor(SizeType length);

	void CheckIndex(SizeType index) const;

	// The size of the inline buffer, including the '\0'. 16 makes a CString
	// 40 bytes on 64-bit platforms: TSmallArray's three members, then the
	// buffer.
	static constexpr SizeType InlineChars = 16;

	// Empty for an empty string. Otherwise, the chars followed by '\0'.
	TSmallArray<char, InlineChars> m_Chars;
};

} // namespace HB
