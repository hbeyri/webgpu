// TSmallArray is a template, so all of its code is in SmallArray.h. This file
// only holds its unit tests.
#include "SmallArray.h"

// The app is compiled with DOCTEST_CONFIG_DISABLE and skips everything below
#ifndef DOCTEST_CONFIG_DISABLE

#include <doctest/doctest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

using HB::TSmallArray;

namespace HB::SmallArray::Internals
{
	// Counts how many objects are alive, to check that TSmallArray destroys
	// every element it constructs
	struct Counted
	{
		static inline int s_Alive = 0;

		explicit Counted(int v = 0) : value(v)
		{
			++s_Alive;
		}

		Counted(Counted const& other) : value(other.value)
		{
			++s_Alive;
		}

		Counted(Counted&& other) noexcept : value(other.value)
		{
			++s_Alive;
		}

		Counted& operator=(Counted const&) = default;
		Counted& operator=(Counted&&) = default;

		~Counted()
		{
			--s_Alive;
		}

		int value;
	};

	struct alignas(32) Aligned
	{
		int value = 0;
	};

	// True if pointer points inside object, which is where the inline
	// buffer is
	template <typename Object, typename T>
	bool PointsInside(Object const& object, T const* pointer)
	{
		auto address = reinterpret_cast<std::uintptr_t>(pointer);
		auto begin = reinterpret_cast<std::uintptr_t>(&object);
		return address >= begin && address < begin + sizeof(Object);
	}
} // namespace HB::SmallArray::Internals

using HB::SmallArray::Internals::Aligned;
using HB::SmallArray::Internals::Counted;
using HB::SmallArray::Internals::PointsInside;

// The inline buffer is inside the array, after TArray's three members
static_assert(sizeof(TSmallArray<int, 8>) >= sizeof(HB::TArray<int>) + 8 * sizeof(int));
static_assert(TSmallArray<int, 8>::GetInlineCapacity() == 8);

TEST_CASE("TSmallArray starts empty, using its inline buffer")
{
	TSmallArray<int, 4> array;
	CHECK(array.IsEmpty());
	CHECK(array.GetCapacity() == 4);
	CHECK(array.IsInline());
	CHECK(PointsInside(array, array.GetData()));
}

TEST_CASE("TSmallArray moves to the heap when it grows past its inline capacity")
{
	TSmallArray<std::string, 4> array;

	SUBCASE("PushBack up to the inline capacity doesn't allocate")
	{
		for (int i = 0; i < 4; ++i)
		{
			array.PushBack(std::to_string(i));
		}
		CHECK(array.IsInline());
		CHECK(array.GetCapacity() == 4);
	}

	SUBCASE("one more PushBack moves every element to the heap")
	{
		for (int i = 0; i < 5; ++i)
		{
			array.PushBack(std::to_string(i));
		}
		CHECK_FALSE(array.IsInline());
		CHECK_FALSE(PointsInside(array, array.GetData()));
		CHECK(array.GetCapacity() >= 5);
		REQUIRE(array.GetSize() == 5);
		for (int i = 0; i < 5; ++i)
		{
			CHECK(array[static_cast<std::size_t>(i)] == std::to_string(i));
		}
	}

	SUBCASE("PushBack of one of its own elements, while it moves to the heap")
	{
		array = {"a", "b", "c", "d"};
		REQUIRE(array.IsInline());
		array.PushBack(array[0]);
		CHECK_FALSE(array.IsInline());
		CHECK(array[4] == "a");
	}

	SUBCASE("it stays on the heap after it shrinks")
	{
		array.Resize(10);
		array.Resize(1);
		array.PopBack();
		CHECK_FALSE(array.IsInline());
		CHECK(array.GetCapacity() >= 10);
	}

	SUBCASE("Reserve up to the inline capacity changes nothing")
	{
		array.Reserve(3);
		CHECK(array.IsInline());
		CHECK(array.GetCapacity() == 4);
	}

	SUBCASE("Reserve past the inline capacity moves to the heap")
	{
		array.PushBack("x");
		array.Reserve(20);
		CHECK_FALSE(array.IsInline());
		CHECK(array.GetCapacity() == 20);
		CHECK(array[0] == "x");
	}

	SUBCASE("Resize past the inline capacity moves to the heap")
	{
		array.Resize(6, "y");
		CHECK_FALSE(array.IsInline());
		CHECK(array.GetSize() == 6);
		CHECK(array[5] == "y");
	}
}

TEST_CASE("TSmallArray from an initializer list")
{
	SUBCASE("that fits in the inline buffer")
	{
		TSmallArray<int, 4> array = {1, 2, 3};
		CHECK(array.IsInline());
		REQUIRE(array.GetSize() == 3);
		CHECK(array[2] == 3);
	}

	SUBCASE("that doesn't fit goes straight to the heap")
	{
		TSmallArray<int, 2> array = {1, 2, 3};
		CHECK_FALSE(array.IsInline());
		CHECK(array.GetCapacity() == 3);
		CHECK(array[2] == 3);
	}
}

TEST_CASE("TSmallArray copies")
{
	SUBCASE("an inline array copies into the copy's inline buffer")
	{
		TSmallArray<std::string, 4> array = {"a", "b"};
		TSmallArray<std::string, 4> copy = array;
		CHECK(copy.IsInline());
		CHECK(copy.GetData() != array.GetData());
		copy[0] = "changed";
		CHECK(array[0] == "a");
	}

	SUBCASE("a heap array copies into its own heap memory")
	{
		TSmallArray<std::string, 2> array = {"a", "b", "c"};
		TSmallArray<std::string, 2> copy = array;
		CHECK_FALSE(copy.IsInline());
		CHECK(copy.GetData() != array.GetData());
		REQUIRE(copy.GetSize() == 3);
		CHECK(copy[2] == "c");
	}

	SUBCASE("a heap array whose elements fit copies into the inline buffer")
	{
		TSmallArray<std::string, 2> array = {"a", "b", "c"};
		array.PopBack();
		TSmallArray<std::string, 2> copy = array;
		CHECK(copy.IsInline());
		CHECK(copy[1] == "b");
	}

	SUBCASE("assignment replaces the elements")
	{
		TSmallArray<std::string, 2> array = {"a", "b", "c"};
		TSmallArray<std::string, 2> other = {"x"};
		other = array;
		REQUIRE(other.GetSize() == 3);
		CHECK(other[2] == "c");
	}

	SUBCASE("assigning an array to itself keeps its elements")
	{
		TSmallArray<std::string, 2> array = {"a"};
		TSmallArray<std::string, 2>& self = array;
		array = self;
		REQUIRE(array.GetSize() == 1);
		CHECK(array[0] == "a");
	}
}

TEST_CASE("TSmallArray moves")
{
	SUBCASE("an inline array moves its elements one by one")
	{
		TSmallArray<std::unique_ptr<int>, 4> array;
		array.PushBack(std::make_unique<int>(1));
		array.PushBack(std::make_unique<int>(2));
		TSmallArray<std::unique_ptr<int>, 4> moved = std::move(array);
		CHECK(moved.IsInline());
		REQUIRE(moved.GetSize() == 2);
		CHECK(*moved[1] == 2);
		CHECK(array.IsEmpty()); // NOLINT(bugprone-use-after-move)
		CHECK(array.IsInline());
	}

	SUBCASE("a heap array gives away its heap memory")
	{
		TSmallArray<std::unique_ptr<int>, 2> array;
		for (int i = 0; i < 3; ++i)
		{
			array.PushBack(std::make_unique<int>(i));
		}
		std::unique_ptr<int> const* data = array.GetData();
		TSmallArray<std::unique_ptr<int>, 2> moved = std::move(array);
		CHECK(moved.GetData() == data);
		CHECK(moved.GetSize() == 3);
		CHECK(array.IsEmpty()); // NOLINT(bugprone-use-after-move)
		CHECK(array.IsInline());
		CHECK(array.GetCapacity() == 2);

		array.PushBack(std::make_unique<int>(7));
		CHECK(*array[0] == 7);
	}

	SUBCASE("move assignment of a heap array into a heap array")
	{
		TSmallArray<std::string, 1> array = {"a", "b"};
		TSmallArray<std::string, 1> other = {"x", "y", "z"};
		std::string const* data = array.GetData();
		other = std::move(array);
		CHECK(other.GetData() == data);
		CHECK(other.GetSize() == 2);
		CHECK(array.IsInline()); // NOLINT(bugprone-use-after-move)
	}

	SUBCASE("move assignment of an inline array into a heap array keeps the heap memory")
	{
		TSmallArray<std::string, 2> array = {"a"};
		TSmallArray<std::string, 2> other = {"x", "y", "z"};
		std::string const* otherData = other.GetData();
		other = std::move(array);
		CHECK(other.GetData() == otherData);
		REQUIRE(other.GetSize() == 1);
		CHECK(other[0] == "a");
	}
}

TEST_CASE("TSmallArray keeps over-aligned elements aligned")
{
	TSmallArray<Aligned, 3> array;
	CHECK(reinterpret_cast<std::uintptr_t>(array.GetData()) % 32 == 0);
	array.Resize(5);
	CHECK(reinterpret_cast<std::uintptr_t>(array.GetData()) % 32 == 0);
}

TEST_CASE("TSmallArray destroys every element it constructs")
{
	Counted::s_Alive = 0;
	{
		TSmallArray<Counted, 4> array = {Counted(1), Counted(2)};
		array.PushBack(Counted(3));
		CHECK(Counted::s_Alive == 3);

		TSmallArray<Counted, 4> inlineCopy = array;
		CHECK(Counted::s_Alive == 6);

		for (int i = 0; i < 10; ++i)
		{
			array.PushBack(Counted(i));
		}
		CHECK(Counted::s_Alive == 16);

		TSmallArray<Counted, 4> moved = std::move(inlineCopy);
		CHECK(Counted::s_Alive == 16);

		TSmallArray<Counted, 4> heapMoved = std::move(array);
		CHECK(Counted::s_Alive == 16);
		heapMoved.Resize(2);
		CHECK(Counted::s_Alive == 5);
	}
	CHECK(Counted::s_Alive == 0);
}

#endif // DOCTEST_CONFIG_DISABLE
