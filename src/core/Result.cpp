// Result is a template, so all of its code is in Result.h. This file only
// holds its unit tests.
#include "Result.h"

// The app is compiled with DOCTEST_CONFIG_DISABLE and skips everything below
#ifndef DOCTEST_CONFIG_DISABLE

#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <type_traits>

using HB::ResultErr;
using HB::ResultOk;
using HB::Result;

namespace
{
	// A function that can fail, to use Result the way it is meant to be used
	Result<int, std::string> ParseDigit(char c)
	{
		if (c < '0' || c > '9')
		{
			return ResultErr("not a digit");
		}
		return ResultOk(c - '0');
	}

	// A function that returns nothing on success
	Result<void, std::string> CheckPositive(int number)
	{
		if (number <= 0)
		{
			return ResultErr("not positive");
		}
		return ResultOk();
	}
} // namespace

// Result works at compile time too, so these are checked when the file compiles
static_assert(Result<int, char>(ResultOk(2)).IsOk());
static_assert(Result<int, char>(ResultErr('e')).IsErr());
static_assert(Result<int, char>(ResultOk(2)).GetValue() == 2);
static_assert(Result<int, char>(ResultErr('e')).GetError() == 'e');
static_assert(Result<void, char>(ResultOk()).IsOk());

TEST_CASE("Result holds either a value or an error")
{
	SUBCASE("ResultOk holds a value")
	{
		Result<int, std::string> result = ResultOk(3);
		CHECK(result.IsOk());
		CHECK_FALSE(result.IsErr());
		CHECK(result == ResultOk(3));
	}

	SUBCASE("ResultErr holds an error")
	{
		Result<int, std::string> result = ResultErr(std::string("failed"));
		CHECK(result.IsErr());
		CHECK_FALSE(result.IsOk());
		CHECK(result.GetError() == "failed");
	}

	SUBCASE("the value and the error can have the same type")
	{
		Result<int, int> ok = ResultOk(1);
		Result<int, int> err = ResultErr(1);
		CHECK(ok.IsOk());
		CHECK(err.IsErr());
	}

	SUBCASE("ResultOk and ResultErr convert to the types of the Result")
	{
		Result<std::string, std::string> ok = ResultOk("text");
		Result<std::string, std::string> err = ResultErr("text");
		CHECK(ok == ResultOk(std::string("text")));
		CHECK(err.GetError() == std::string("text"));
	}

	SUBCASE("a function returns ResultOk or ResultErr")
	{
		CHECK(ParseDigit('7').IsOk());
		CHECK(ParseDigit('7') == ResultOk(7));
		CHECK(ParseDigit('x').IsErr());
		CHECK(ParseDigit('x').GetError() == "not a digit");
	}
}

TEST_CASE("Result compares with other results, ResultOk and ResultErr")
{
	Result<int, std::string> ok = ResultOk(3);
	Result<int, std::string> err = ResultErr("failed");

	CHECK(ok == ResultOk(3));
	CHECK(ok != ResultOk(4));
	CHECK(ok != ResultErr("failed"));
	CHECK(err == ResultErr("failed"));
	CHECK(err != ResultErr("other"));
	CHECK(err != ResultOk(3));

	CHECK(ok == Result<int, std::string>(ResultOk(3)));
	CHECK(ok != err);

	SUBCASE("the same content as a value and as an error is not equal")
	{
		Result<int, int> okOne = ResultOk(1);
		Result<int, int> errOne = ResultErr(1);
		CHECK(okOne != errOne);
	}
}

TEST_CASE("Result::GetValue and GetError")
{
	SUBCASE("GetValue gives a reference to the value")
	{
		Result<int, std::string> result = ResultOk(3);
		CHECK(result.GetValue() == 3);
		result.GetValue() = 5;
		CHECK(result == ResultOk(5));
	}

	SUBCASE("GetValue on a temporary moves the value out")
	{
		Result<std::unique_ptr<int>, std::string> result = ResultOk(std::make_unique<int>(3));
		std::unique_ptr<int> pointer = std::move(result).GetValue();
		REQUIRE(pointer != nullptr);
		CHECK(*pointer == 3);
	}

	SUBCASE("GetError gives a reference to the error")
	{
		Result<int, std::string> result = ResultErr("failed");
		result.GetError() += " twice";
		CHECK(result == ResultErr("failed twice"));
	}

	SUBCASE("GetError on a temporary moves the error out")
	{
		Result<int, std::unique_ptr<int>> result = ResultErr(std::make_unique<int>(3));
		std::unique_ptr<int> pointer = std::move(result).GetError();
		REQUIRE(pointer != nullptr);
		CHECK(*pointer == 3);
	}
}

TEST_CASE("Result::TryGetValue and TryGetError")
{
	Result<int, std::string> ok = ResultOk(3);
	Result<int, std::string> err = ResultErr("failed");

	SUBCASE("TryGetValue points to the value, or is null for an error")
	{
		REQUIRE(ok.TryGetValue() != nullptr);
		CHECK(*ok.TryGetValue() == 3);
		CHECK(err.TryGetValue() == nullptr);
	}

	SUBCASE("TryGetError points to the error, or is null for a value")
	{
		REQUIRE(err.TryGetError() != nullptr);
		CHECK(*err.TryGetError() == "failed");
		CHECK(ok.TryGetError() == nullptr);
	}

	SUBCASE("the pointer can change what the Result holds")
	{
		if (auto* value = ok.TryGetValue())
		{
			*value = 5;
		}
		CHECK(ok == ResultOk(5));
	}

	SUBCASE("a const Result gives a pointer to const")
	{
		Result<int, std::string> const& constOk = ok;
		static_assert(std::is_same_v<decltype(constOk.TryGetValue()), int const*>);
		CHECK(*constOk.TryGetValue() == 3);
	}
}

TEST_CASE("Result<void, E> is success without a value, or an error")
{
	SUBCASE("ResultOk() is success")
	{
		Result<void, std::string> result = ResultOk();
		CHECK(result.IsOk());
		CHECK_FALSE(result.IsErr());
		CHECK(result == ResultOk());
	}

	SUBCASE("ResultErr holds an error")
	{
		Result<void, std::string> result = ResultErr("failed");
		CHECK(result.IsErr());
		CHECK(result.GetError() == "failed");
		CHECK(result == ResultErr("failed"));
		CHECK(result != ResultOk());
	}

	SUBCASE("a function returns ResultOk() or ResultErr")
	{
		CHECK(CheckPositive(1).IsOk());
		CHECK(CheckPositive(-1) == ResultErr("not positive"));
	}

	SUBCASE("TryGetError points to the error, or is null for ResultOk()")
	{
		Result<void, std::string> err = ResultErr("failed");
		REQUIRE(err.TryGetError() != nullptr);
		CHECK(*err.TryGetError() == "failed");
		Result<void, std::string> ok = ResultOk();
		CHECK(ok.TryGetError() == nullptr);
	}
}

#endif // DOCTEST_CONFIG_DISABLE
