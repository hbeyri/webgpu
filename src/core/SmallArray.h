#pragma once

#include "Array.h"

#include <cstddef>
#include <initializer_list>
#include <type_traits>

// A TArray that holds up to InlineCapacity elements inside itself, so it
// doesn't allocate until it grows past that. Once it does, every element moves
// to the heap and it works like a TArray from then on, even if it shrinks
// again.
//
//     HB::TSmallArray<int, 4> numbers = {1, 2, 3};
//     numbers.PushBack(4); // still no allocation
//     numbers.PushBack(5); // all 5 elements move to the heap
//
// It's for arrays that are usually small, as a local variable or a member.
// A TSmallArray is as big as its inline buffer, so keep InlineCapacity small.
namespace HB
{

template <typename T, std::size_t InlineCapacity>
class TSmallArray : public Array::Internals::TArrayBase<T, TSmallArray<T, InlineCapacity>>
{
	static_assert(InlineCapacity > 0, "A TSmallArray without inline elements is a TArray");

	using Base = Array::Internals::TArrayBase<T, TSmallArray<T, InlineCapacity>>;
	friend Base;

public:
	using typename Base::SizeType;

	TSmallArray()
	{
		this->UseInlineBuffer();
	}

	TSmallArray(std::initializer_list<T> elements)
	{
		this->UseInlineBuffer();
		this->AssignCopies(elements.begin(), elements.size());
	}

	// The copy uses the inline buffer if other's elements fit in it, even if
	// other's are on the heap
	TSmallArray(TSmallArray const& other) : Base()
	{
		this->UseInlineBuffer();
		this->AssignCopies(other.m_Data, other.m_Size);
	}

	// Takes other's heap memory if it has some; otherwise moves the elements
	// one by one. Leaves other empty, using its inline buffer.
	TSmallArray(TSmallArray&& other) noexcept(std::is_nothrow_move_constructible_v<T>) : Base()
	{
		this->UseInlineBuffer();
		this->MoveFrom(other);
	}

	TSmallArray& operator=(TSmallArray const& other)
	{
		if (this != &other)
		{
			this->AssignCopies(other.m_Data, other.m_Size);
		}
		return *this;
	}

	TSmallArray& operator=(TSmallArray&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
	{
		if (this != &other)
		{
			this->MoveFrom(other);
		}
		return *this;
	}

	~TSmallArray()
	{
		this->DestroyAll();
	}

	// The number of elements it holds without allocating
	static constexpr SizeType GetInlineCapacity()
	{
		return InlineCapacity;
	}

	// True while the elements are in the inline buffer, false once they've
	// moved to the heap
	bool IsInline() const
	{
		return this->UsesInlineBuffer();
	}

private:
	T* GetInlineData()
	{
		return reinterpret_cast<T*>(m_InlineBuffer);
	}

	T const* GetInlineData() const
	{
		return reinterpret_cast<T const*>(m_InlineBuffer);
	}

	// Raw memory: elements are constructed in it only as they're added
	alignas(T) std::byte m_InlineBuffer[sizeof(T) * InlineCapacity];
};

} // namespace HB
