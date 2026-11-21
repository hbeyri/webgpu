#pragma once

#include "Abort.h"

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>

namespace HB
{

namespace Array::Internals
{
	// The code shared by TArray and TSmallArray (in SmallArray.h). The elements
	// are either in an inline buffer that Derived provides, or in memory
	// allocated on the heap. Derived provides:
	//
	//     static SizeType GetInlineCapacity();  // 0 when there's no buffer
	//     T* GetInlineData();                    // nullptr when there's no buffer
	//     T const* GetInlineData() const;
	//
	// and calls UseInlineBuffer() in its constructors and DestroyAll() in its
	// destructor.
	template <typename T, typename Derived>
	class TArrayBase
	{
		static_assert(std::is_object_v<T> && !std::is_const_v<T> && !std::is_array_v<T>,
			"TArray holds non-const objects, not references, functions or C arrays");

	public:
		using SizeType = std::size_t;

		// The number of elements
		SizeType GetSize() const
		{
			return m_Size;
		}

		// The number of elements the array can hold before it has to allocate
		// more memory
		SizeType GetCapacity() const
		{
			return m_Capacity;
		}

		bool IsEmpty() const
		{
			return m_Size == 0;
		}

		T* GetData()
		{
			return m_Data;
		}

		T const* GetData() const
		{
			return m_Data;
		}

		// The element at index. Stops the program if index is past the end.
		T& operator[](SizeType index)
		{
			CheckIndex(index);
			return m_Data[index];
		}

		T const& operator[](SizeType index) const
		{
			CheckIndex(index);
			return m_Data[index];
		}

		// Adds an element at the end. The element can be one of this array's
		// own, like `array.PushBack(array[0])`.
		void PushBack(T const& element)
		{
			EmplaceBack(element);
		}

		void PushBack(T&& element)
		{
			EmplaceBack(std::move(element));
		}

		// Removes the last element. Stops the program if the array is empty.
		void PopBack()
		{
			if (m_Size == 0)
			{
				Abort();
			}
			--m_Size;
			std::destroy_at(m_Data + m_Size);
		}

		// Makes room for at least newCapacity elements, so that adding elements
		// up to that count doesn't allocate. Never makes the capacity smaller.
		void Reserve(SizeType newCapacity)
		{
			if (newCapacity > m_Capacity)
			{
				Reallocate(newCapacity);
			}
		}

		// Changes the number of elements. New elements are value-initialized
		// (zero for numbers), and elements past newSize are destroyed.
		void Resize(SizeType newSize)
		{
			if (newSize > m_Size)
			{
				Reserve(newSize);
				std::uninitialized_value_construct_n(m_Data + m_Size, newSize - m_Size);
				m_Size = newSize;
			}
			else
			{
				ShrinkTo(newSize);
			}
		}

		// Same as Resize(newSize), but new elements are copies of value
		void Resize(SizeType newSize, T const& value)
		{
			if (newSize > m_Size)
			{
				if (newSize > m_Capacity)
				{
					// value may be one of the elements, so copy it before the
					// elements move to new memory
					T copy(value);
					Reallocate(newSize);
					std::uninitialized_fill_n(m_Data + m_Size, newSize - m_Size, copy);
				}
				else
				{
					std::uninitialized_fill_n(m_Data + m_Size, newSize - m_Size, value);
				}
				m_Size = newSize;
			}
			else
			{
				ShrinkTo(newSize);
			}
		}

		// Destroys every element. Keeps the memory, so the capacity doesn't
		// change.
		void Clear()
		{
			ShrinkTo(0);
		}

		// For range-based for loops and standard algorithms
		T* begin()
		{
			return m_Data;
		}

		T* end()
		{
			return m_Data + m_Size;
		}

		T const* begin() const
		{
			return m_Data;
		}

		T const* end() const
		{
			return m_Data + m_Size;
		}

	protected:
		TArrayBase() = default;

		// Only the derived classes know how to copy and move, because of the
		// inline buffer
		TArrayBase(TArrayBase const&) = delete;
		TArrayBase& operator=(TArrayBase const&) = delete;

		~TArrayBase() = default;

		// Makes the array empty, using the inline buffer. Doesn't destroy or
		// free anything, so it's only for an array that holds nothing.
		void UseInlineBuffer()
		{
			m_Data = GetDerived().GetInlineData();
			m_Size = 0;
			m_Capacity = Derived::GetInlineCapacity();
		}

		// True while the elements are in the inline buffer. Always true for an
		// array that has no inline buffer and hasn't allocated.
		bool UsesInlineBuffer() const
		{
			return m_Data == GetDerived().GetInlineData();
		}

		// Destroys every element and frees the heap memory, if any
		void DestroyAll()
		{
			Clear();
			FreeMemory();
		}

		// Replaces the elements with copies of count elements from source,
		// which must not be this array's own
		void AssignCopies(T const* source, SizeType count)
		{
			Clear();
			Reserve(count);
			std::uninitialized_copy_n(source, count, m_Data);
			m_Size = count;
		}

		// Replaces the elements with other's, leaving other empty. Steals
		// other's heap memory if it has some; otherwise moves the elements one
		// by one. other must not be this array.
		void MoveFrom(TArrayBase& other)
		{
			if (other.UsesInlineBuffer())
			{
				Clear();
				Reserve(other.m_Size);
				std::uninitialized_move_n(other.m_Data, other.m_Size, m_Data);
				m_Size = other.m_Size;
				other.Clear();
			}
			else
			{
				DestroyAll();
				m_Data = other.m_Data;
				m_Size = other.m_Size;
				m_Capacity = other.m_Capacity;
				other.UseInlineBuffer();
			}
		}

		T* m_Data = nullptr;
		SizeType m_Size = 0;
		SizeType m_Capacity = 0;

	private:
		Derived& GetDerived()
		{
			return static_cast<Derived&>(*this);
		}

		Derived const& GetDerived() const
		{
			return static_cast<Derived const&>(*this);
		}

		template <typename... Args>
		void EmplaceBack(Args&&... args)
		{
			if (m_Size < m_Capacity)
			{
				std::construct_at(m_Data + m_Size, std::forward<Args>(args)...);
			}
			else
			{
				// Build the new element in the new memory before moving the old
				// elements there, because args may refer to an old element
				SizeType newCapacity = GrowCapacity(m_Size + 1);
				T* newData = Allocate(newCapacity);
				std::construct_at(newData + m_Size, std::forward<Args>(args)...);
				MoveElementsTo(newData, newCapacity);
			}
			++m_Size;
		}

		// The capacity to grow to when there's no room for one more element:
		// doubling it means that adding n elements one by one copies each
		// element a constant number of times on average
		SizeType GrowCapacity(SizeType minCapacity) const
		{
			SizeType doubled = m_Capacity <= MaxCapacity() / 2 ? m_Capacity * 2 : MaxCapacity();
			return doubled > minCapacity ? doubled : minCapacity;
		}

		void Reallocate(SizeType newCapacity)
		{
			MoveElementsTo(Allocate(newCapacity), newCapacity);
		}

		// Moves the elements into newData, heap memory that holds newCapacity
		// elements, and frees the old memory if it was on the heap
		void MoveElementsTo(T* newData, SizeType newCapacity)
		{
			std::uninitialized_move_n(m_Data, m_Size, newData);
			std::destroy_n(m_Data, m_Size);
			FreeMemory();
			m_Data = newData;
			m_Capacity = newCapacity;
		}

		void ShrinkTo(SizeType newSize)
		{
			if (newSize < m_Size)
			{
				std::destroy(m_Data + newSize, m_Data + m_Size);
				m_Size = newSize;
			}
		}

		void FreeMemory()
		{
			if (!UsesInlineBuffer())
			{
				std::allocator<T>().deallocate(m_Data, m_Capacity);
			}
		}

		void CheckIndex(SizeType index) const
		{
			if (index >= m_Size)
			{
				Abort();
			}
		}

		static constexpr SizeType MaxCapacity()
		{
			return std::allocator_traits<std::allocator<T>>::max_size(std::allocator<T>());
		}

		static T* Allocate(SizeType capacity)
		{
			if (capacity > MaxCapacity())
			{
				Abort();
			}
			return std::allocator<T>().allocate(capacity);
		}
	};
} // namespace Array::Internals

// A growable array that owns its elements and keeps them next to each other in
// memory, like std::vector.
//
//     HB::TArray<int> numbers = {1, 2, 3};
//     numbers.PushBack(4);
//     numbers.PopBack();
//     for (int number : numbers)
//     {
//         Use(number);
//     }
//
// Using an index past the end, or PopBack on an empty array, stops the program.
// To keep a few elements without allocating, see TSmallArray in SmallArray.h.
template <typename T>
class TArray : public Array::Internals::TArrayBase<T, TArray<T>>
{
	using Base = Array::Internals::TArrayBase<T, TArray<T>>;
	friend Base;

public:
	using typename Base::SizeType;

	TArray() = default;

	TArray(std::initializer_list<T> elements)
	{
		this->AssignCopies(elements.begin(), elements.size());
	}

	TArray(TArray const& other) : Base()
	{
		this->AssignCopies(other.m_Data, other.m_Size);
	}

	TArray(TArray&& other) noexcept : Base()
	{
		this->MoveFrom(other);
	}

	TArray& operator=(TArray const& other)
	{
		if (this != &other)
		{
			this->AssignCopies(other.m_Data, other.m_Size);
		}
		return *this;
	}

	TArray& operator=(TArray&& other) noexcept
	{
		if (this != &other)
		{
			this->MoveFrom(other);
		}
		return *this;
	}

	~TArray()
	{
		this->DestroyAll();
	}

	void Swap(TArray& other) noexcept
	{
		std::swap(this->m_Data, other.m_Data);
		std::swap(this->m_Size, other.m_Size);
		std::swap(this->m_Capacity, other.m_Capacity);
	}

private:
	// A TArray has no inline buffer: before it allocates, its data is nullptr
	static constexpr SizeType GetInlineCapacity()
	{
		return 0;
	}

	static T* GetInlineData()
	{
		return nullptr;
	}
};

} // namespace HB
