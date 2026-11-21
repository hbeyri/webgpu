// TArray is a template, so all of its code is in Array.h. This file only holds
// its unit tests.
#include "Array.h"

// The app is compiled with DOCTEST_CONFIG_DISABLE and skips everything below
#ifndef DOCTEST_CONFIG_DISABLE

#include <doctest/doctest.h>

#include <memory>
#include <string>

using HB::TArray;

// The code shared with TSmallArray doesn't make a TArray any bigger
static_assert(sizeof(TArray<int>) == 3 * sizeof(void*));

namespace HB::Array::Internals
{
	// Counts how many objects are alive, to check that TArray destroys every
	// element it constructs
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
} // namespace HB::Array::Internals

using HB::Array::Internals::Counted;

TEST_CASE("TArray starts empty")
{
	TArray<int> array;
	CHECK(array.GetSize() == 0);
	CHECK(array.GetCapacity() == 0);
	CHECK(array.IsEmpty());
	CHECK(array.GetData() == nullptr);
	CHECK(array.begin() == array.end());
}

TEST_CASE("TArray from an initializer list")
{
	TArray<int> array = {1, 2, 3};
	REQUIRE(array.GetSize() == 3);
	CHECK(array.GetCapacity() == 3);
	CHECK(array[0] == 1);
	CHECK(array[1] == 2);
	CHECK(array[2] == 3);

	SUBCASE("an empty list makes an empty array")
	{
		TArray<int> empty = {};
		CHECK(empty.IsEmpty());
	}

	SUBCASE("range-based for visits the elements in order")
	{
		int sum = 0;
		for (int element : array)
		{
			sum = sum * 10 + element;
		}
		CHECK(sum == 123);
	}
}

TEST_CASE("TArray::PushBack and PopBack")
{
	TArray<std::string> array;

	SUBCASE("PushBack adds at the end and grows the capacity")
	{
		for (int i = 0; i < 100; ++i)
		{
			array.PushBack(std::to_string(i));
			CHECK(array.GetSize() == static_cast<std::size_t>(i + 1));
			CHECK(array.GetCapacity() >= array.GetSize());
		}
		CHECK(array[0] == "0");
		CHECK(array[99] == "99");
	}

	SUBCASE("PushBack copies an lvalue and moves an rvalue")
	{
		std::string text = "hello";
		array.PushBack(text);
		CHECK(text == "hello");
		array.PushBack(std::move(text));
		CHECK(array[0] == "hello");
		CHECK(array[1] == "hello");
	}

	SUBCASE("PushBack of one of the array's own elements, while it grows")
	{
		array = {"first"};
		REQUIRE(array.GetSize() == array.GetCapacity());
		array.PushBack(array[0]);
		REQUIRE(array.GetSize() == 2);
		CHECK(array[1] == "first");
	}

	SUBCASE("PopBack removes the last element and keeps the capacity")
	{
		array = {"a", "b", "c"};
		std::size_t capacity = array.GetCapacity();
		array.PopBack();
		REQUIRE(array.GetSize() == 2);
		CHECK(array[1] == "b");
		array.PopBack();
		array.PopBack();
		CHECK(array.IsEmpty());
		CHECK(array.GetCapacity() == capacity);
	}

	SUBCASE("move-only elements")
	{
		TArray<std::unique_ptr<int>> pointers;
		for (int i = 0; i < 10; ++i)
		{
			pointers.PushBack(std::make_unique<int>(i));
		}
		REQUIRE(pointers.GetSize() == 10);
		CHECK(*pointers[9] == 9);
	}
}

TEST_CASE("TArray::Reserve")
{
	TArray<int> array = {1, 2};

	SUBCASE("Reserve grows the capacity and keeps the elements")
	{
		array.Reserve(50);
		CHECK(array.GetCapacity() == 50);
		REQUIRE(array.GetSize() == 2);
		CHECK(array[0] == 1);
		CHECK(array[1] == 2);
	}

	SUBCASE("PushBack up to the reserved capacity doesn't move the elements")
	{
		array.Reserve(50);
		int const* data = array.GetData();
		while (array.GetSize() < 50)
		{
			array.PushBack(0);
		}
		CHECK(array.GetData() == data);
	}

	SUBCASE("Reserve never makes the capacity smaller")
	{
		array.Reserve(1);
		CHECK(array.GetCapacity() == 2);
		CHECK(array.GetSize() == 2);
	}
}

TEST_CASE("TArray::Resize")
{
	TArray<int> array = {1, 2, 3};

	SUBCASE("growing value-initializes the new elements")
	{
		array.Resize(5);
		REQUIRE(array.GetSize() == 5);
		CHECK(array[2] == 3);
		CHECK(array[3] == 0);
		CHECK(array[4] == 0);
	}

	SUBCASE("growing with a value copies it into the new elements")
	{
		array.Resize(5, 7);
		REQUIRE(array.GetSize() == 5);
		CHECK(array[2] == 3);
		CHECK(array[3] == 7);
		CHECK(array[4] == 7);
	}

	SUBCASE("growing with one of the array's own elements, while it grows")
	{
		REQUIRE(array.GetCapacity() == 3);
		array.Resize(6, array[0]);
		REQUIRE(array.GetSize() == 6);
		CHECK(array[5] == 1);
	}

	SUBCASE("shrinking removes elements from the end and keeps the capacity")
	{
		array.Resize(1);
		REQUIRE(array.GetSize() == 1);
		CHECK(array[0] == 1);
		CHECK(array.GetCapacity() == 3);
	}

	SUBCASE("resizing to 0 empties the array")
	{
		array.Resize(0);
		CHECK(array.IsEmpty());
	}
}

TEST_CASE("TArray copies and moves")
{
	TArray<std::string> array = {"a", "b"};

	SUBCASE("a copy has its own elements")
	{
		TArray<std::string> copy = array;
		copy[0] = "changed";
		CHECK(array[0] == "a");
		CHECK(copy.GetSize() == 2);
	}

	SUBCASE("a move takes the elements and leaves the source empty")
	{
		std::string const* data = array.GetData();
		TArray<std::string> moved = std::move(array);
		CHECK(moved.GetData() == data);
		CHECK(moved.GetSize() == 2);
		CHECK(array.IsEmpty()); // NOLINT(bugprone-use-after-move)
		CHECK(array.GetCapacity() == 0);
	}

	SUBCASE("assignment replaces the elements")
	{
		TArray<std::string> other = {"x"};
		other = array;
		CHECK(other.GetSize() == 2);
		CHECK(other[1] == "b");
	}

	SUBCASE("assigning an array to itself keeps its elements")
	{
		TArray<std::string>& self = array;
		array = self;
		REQUIRE(array.GetSize() == 2);
		CHECK(array[0] == "a");
	}
}

TEST_CASE("TArray destroys every element it constructs")
{
	Counted::s_Alive = 0;
	{
		TArray<Counted> array = {Counted(1), Counted(2)};
		for (int i = 0; i < 20; ++i)
		{
			array.PushBack(Counted(i));
		}
		CHECK(Counted::s_Alive == 22);
		array.PopBack();
		array.Resize(30);
		array.Resize(5, Counted(9));
		CHECK(Counted::s_Alive == 5);

		TArray<Counted> copy = array;
		CHECK(Counted::s_Alive == 10);
		array.Clear();
		CHECK(Counted::s_Alive == 5);
	}
	CHECK(Counted::s_Alive == 0);
}

#endif // DOCTEST_CONFIG_DISABLE
