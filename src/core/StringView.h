#pragma once

#include "Abort.h"

#include <compare>
#include <cstddef>
#include <cstring>
#include <iosfwd>
#include <type_traits>

// A view of chars owned by something else, like std::string_view: a pointer
// and a length. It doesn't copy or own the chars, so they must outlive the
// view. Unlike a C string, the chars aren't always followed by '\0', so pass
// GetData() only to functions that also take the length.
//
//     void Greet(HB::CStringView name);
//
//     Greet("world");                // a string literal
//     Greet(someCString);            // a CString
//     Greet(text.SubView(0, 5));     // part of another view
//
// Using an index or offset past the end stops the program.
namespace HB
{

class CStringView
{
public:
	using SizeType = std::size_t;

	// An empty view
	constexpr CStringView() = default;

	// The chars of text up to its '\0'. Not explicit, so that string literals
	// convert. Stops the program if text is nullptr.
	constexpr CStringView(char const* text) : m_Data(text), m_Length(text != nullptr ? LengthOf(text) : 0)
	{
		if (text == nullptr)
		{
			Abort();
		}
	}

	// length chars from data, which may contain '\0'. Stops the program if
	// data is nullptr and length isn't 0.
	constexpr CStringView(char const* data, SizeType length) : m_Data(data), m_Length(length)
	{
		if (data == nullptr && length > 0)
		{
			Abort();
		}
	}

	// A view of nullptr is a bug, so it doesn't compile
	CStringView(std::nullptr_t) = delete;

	// The first char. Not followed by '\0' unless the viewed chars are.
	constexpr char const* GetData() const
	{
		return m_Data;
	}

	// The number of chars
	constexpr SizeType GetLength() const
	{
		return m_Length;
	}

	constexpr bool IsEmpty() const
	{
		return m_Length == 0;
	}

	// The char at index. Stops the program if index is past the end.
	constexpr char const& operator[](SizeType index) const
	{
		if (index >= m_Length)
		{
			Abort();
		}
		return m_Data[index];
	}

	// count chars starting at offset, or fewer if the view ends first. Stops
	// the program if offset is past the end; offset == GetLength() gives an
	// empty view.
	constexpr CStringView SubView(SizeType offset, SizeType count) const
	{
		if (offset > m_Length)
		{
			Abort();
		}
		SizeType const rest = m_Length - offset;
		return CStringView(m_Data + offset, count < rest ? count : rest);
	}

	// Every char from offset to the end
	constexpr CStringView SubView(SizeType offset) const
	{
		return SubView(offset, m_Length);
	}

	constexpr bool StartsWith(CStringView prefix) const
	{
		return prefix.m_Length <= m_Length && SubView(0, prefix.m_Length) == prefix;
	}

	constexpr bool EndsWith(CStringView suffix) const
	{
		return suffix.m_Length <= m_Length && SubView(m_Length - suffix.m_Length) == suffix;
	}

	// For range-based for loops and standard algorithms
	constexpr char const* begin() const
	{
		return m_Data;
	}

	constexpr char const* end() const
	{
		return m_Data + m_Length;
	}

	// Two views are equal when they have the same chars, wherever they are
	friend constexpr bool operator==(CStringView left, CStringView right)
	{
		return left.m_Length == right.m_Length && Compare(left.m_Data, right.m_Data, left.m_Length) == 0;
	}

	// Compares chars one by one as unsigned values, like std::string_view and
	// memcmp. When one view starts with the other, the shorter one is less.
	friend constexpr std::strong_ordering operator<=>(CStringView left, CStringView right)
	{
		SizeType const common = left.m_Length < right.m_Length ? left.m_Length : right.m_Length;
		int const result = Compare(left.m_Data, right.m_Data, common);
		if (result != 0)
		{
			return result < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
		}
		return left.m_Length <=> right.m_Length;
	}

private:
	// strlen, which also works at compile time
	static constexpr SizeType LengthOf(char const* text)
	{
		if (std::is_constant_evaluated())
		{
			SizeType length = 0;
			while (text[length] != '\0')
			{
				++length;
			}
			return length;
		}
		return std::strlen(text);
	}

	// memcmp, which also works at compile time
	static constexpr int Compare(char const* left, char const* right, SizeType count)
	{
		if (std::is_constant_evaluated())
		{
			for (SizeType i = 0; i < count; ++i)
			{
				auto const l = static_cast<unsigned char>(left[i]);
				auto const r = static_cast<unsigned char>(right[i]);
				if (l != r)
				{
					return l < r ? -1 : 1;
				}
			}
			return 0;
		}
		// memcmp needs valid pointers, even for 0 chars, and an empty view
		// can have nullptr
		return count == 0 ? 0 : std::memcmp(left, right, count);
	}

	char const* m_Data = nullptr;
	SizeType m_Length = 0;
};

// Writes the chars, including any '\0' among them, like std::string_view
std::ostream& operator<<(std::ostream& stream, CStringView view);

} // namespace HB
