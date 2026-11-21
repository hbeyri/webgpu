#pragma once

#include "Abort.h"

#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

// A C++ version of Rust's Result<T, E>: the return type of a function that can
// fail. It holds either a value of type ValueType (success) or an error of
// type ErrorType.
//
//     HB::TResult<int, std::string> ParseDigit(char c)
//     {
//         if (c < '0' || c > '9')
//         {
//             return HB::ResultErr("not a digit");
//         }
//         return HB::ResultOk(c - '0');
//     }
//
//     auto digit = ParseDigit('7');
//     if (digit.IsOk())
//     {
//         Use(digit.GetValue());
//     }
//
// Use TResult<void, ErrorType> for a function that returns nothing on success,
// and return ResultOk() from it.
namespace HB
{

template <typename ValueType, typename ErrorType>
class TResult;

// What a function returns for success: `return ResultOk(value);`
template <typename ValueType>
struct ResultOk
{
	constexpr explicit ResultOk(ValueType v) : value(std::move(v))
	{
	}

	ValueType value;
};

// Success without a value, for TResult<void, ErrorType>: `return ResultOk();`
template <>
struct ResultOk<void>
{
};

ResultOk() -> ResultOk<void>;

// What a function returns for failure: `return ResultErr(error);`
template <typename ErrorType>
struct ResultErr
{
	constexpr explicit ResultErr(ErrorType e) : error(std::move(e))
	{
	}

	ErrorType error;
};

namespace Result::Internals
{
	template <typename A, typename B>
	concept EqualityComparableTo = requires(A const& a, B const& b) {
		{ a == b } -> std::convertible_to<bool>;
	};
} // namespace Result::Internals

// [[nodiscard]]: ignoring a TResult returned by a function is a warning
template <typename ValueType, typename ErrorType>
class [[nodiscard]] TResult
{
	static_assert(!std::is_reference_v<ValueType> && !std::is_reference_v<ErrorType>, "TResult cannot hold references");
	static_assert(!std::is_void_v<ErrorType>, "The error type of a TResult cannot be void");

public:
	// Not explicit, so that `return ResultOk(value);` and
	// `return ResultErr(error);` work. The value only has to be convertible:
	// ResultErr("text") works for a std::string error.
	template <typename U>
		requires std::constructible_from<ValueType, U>
	constexpr TResult(ResultOk<U> ok) : m_Storage(std::in_place_index<0>, std::move(ok.value))
	{
	}

	template <typename G>
		requires std::constructible_from<ErrorType, G>
	constexpr TResult(ResultErr<G> err) : m_Storage(std::in_place_index<1>, std::move(err.error))
	{
	}

	constexpr bool IsOk() const
	{
		return m_Storage.index() == 0;
	}

	constexpr bool IsErr() const
	{
		return m_Storage.index() == 1;
	}

	// The value. Stops the program if it holds an error.
	constexpr ValueType& GetValue() &
	{
		CheckIsOk();
		return std::get<0>(m_Storage);
	}

	constexpr ValueType const& GetValue() const&
	{
		CheckIsOk();
		return std::get<0>(m_Storage);
	}

	constexpr ValueType GetValue() &&
	{
		CheckIsOk();
		return std::get<0>(std::move(m_Storage));
	}

	// A pointer to the value, or nullptr if it holds an error:
	//
	//     if (auto* value = result.TryGetValue())
	//     {
	//         Use(*value);
	//     }
	constexpr ValueType* TryGetValue() &
	{
		return std::get_if<0>(&m_Storage);
	}

	constexpr ValueType const* TryGetValue() const&
	{
		return std::get_if<0>(&m_Storage);
	}

	// The pointer would outlive a temporary TResult
	ValueType* TryGetValue() && = delete;

	// The error. Stops the program if it holds a value.
	constexpr ErrorType& GetError() &
	{
		CheckIsErr();
		return std::get<1>(m_Storage);
	}

	constexpr ErrorType const& GetError() const&
	{
		CheckIsErr();
		return std::get<1>(m_Storage);
	}

	constexpr ErrorType GetError() &&
	{
		CheckIsErr();
		return std::get<1>(std::move(m_Storage));
	}

	// A pointer to the error, or nullptr if it holds a value
	constexpr ErrorType* TryGetError() &
	{
		return std::get_if<1>(&m_Storage);
	}

	constexpr ErrorType const* TryGetError() const&
	{
		return std::get_if<1>(&m_Storage);
	}

	// The pointer would outlive a temporary TResult
	ErrorType* TryGetError() && = delete;

	friend constexpr bool operator==(TResult const&, TResult const&) = default;

	template <typename U>
		requires Result::Internals::EqualityComparableTo<ValueType, U>
	friend constexpr bool operator==(TResult const& result, ResultOk<U> const& ok)
	{
		return result.IsOk() && std::get<0>(result.m_Storage) == ok.value;
	}

	template <typename G>
		requires Result::Internals::EqualityComparableTo<ErrorType, G>
	friend constexpr bool operator==(TResult const& result, ResultErr<G> const& err)
	{
		return result.IsErr() && std::get<1>(result.m_Storage) == err.error;
	}

private:
	constexpr void CheckIsOk() const
	{
		if (IsErr())
		{
			Abort();
		}
	}

	constexpr void CheckIsErr() const
	{
		if (IsOk())
		{
			Abort();
		}
	}

	std::variant<ValueType, ErrorType> m_Storage;
};

// The result of a function that returns nothing on success. It has no value,
// so there is no GetValue or TryGetValue.
template <typename ErrorType>
class [[nodiscard]] TResult<void, ErrorType>
{
	static_assert(!std::is_reference_v<ErrorType>, "TResult cannot hold references");
	static_assert(!std::is_void_v<ErrorType>, "The error type of a TResult cannot be void");

public:
	constexpr TResult(ResultOk<void>)
	{
	}

	template <typename G>
		requires std::constructible_from<ErrorType, G>
	constexpr TResult(ResultErr<G> err) : m_Error(std::in_place, std::move(err.error))
	{
	}

	constexpr bool IsOk() const
	{
		return !m_Error.has_value();
	}

	constexpr bool IsErr() const
	{
		return m_Error.has_value();
	}

	// The error. Stops the program if it holds no error.
	constexpr ErrorType& GetError() &
	{
		CheckIsErr();
		return *m_Error;
	}

	constexpr ErrorType const& GetError() const&
	{
		CheckIsErr();
		return *m_Error;
	}

	constexpr ErrorType GetError() &&
	{
		CheckIsErr();
		return std::move(*m_Error);
	}

	// A pointer to the error, or nullptr if it holds no error
	constexpr ErrorType* TryGetError() &
	{
		return m_Error ? &*m_Error : nullptr;
	}

	constexpr ErrorType const* TryGetError() const&
	{
		return m_Error ? &*m_Error : nullptr;
	}

	// The pointer would outlive a temporary TResult
	ErrorType* TryGetError() && = delete;

	friend constexpr bool operator==(TResult const&, TResult const&) = default;

	friend constexpr bool operator==(TResult const& result, ResultOk<void> const&)
	{
		return result.IsOk();
	}

	template <typename G>
		requires Result::Internals::EqualityComparableTo<ErrorType, G>
	friend constexpr bool operator==(TResult const& result, ResultErr<G> const& err)
	{
		return result.IsErr() && *result.m_Error == err.error;
	}

private:
	constexpr void CheckIsErr() const
	{
		if (IsOk())
		{
			Abort();
		}
	}

	std::optional<ErrorType> m_Error;
};

} // namespace HB
