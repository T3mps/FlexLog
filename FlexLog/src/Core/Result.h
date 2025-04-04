#pragma once

#include <concepts>
#include <format>
#include <functional>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace FlexLog
{
	template <typename T>
    concept ErrorType = requires(T e)
    {
        { e.Message() } -> std::convertible_to<std::string_view>;
        { e.Code() } 	-> std::convertible_to<int>;
    };

    class Error
	{
	public:
	    constexpr Error(int code, std::string_view message, std::source_location location = std::source_location::current()) noexcept : m_code(code), m_message(message), m_location(location) {}

		constexpr bool operator==(const Error& other) const noexcept { return m_code == other.m_code; }

		[[nodiscard]] constexpr int Code() const noexcept { return m_code; }
		[[nodiscard]] constexpr std::string_view Message() const noexcept { return m_message; }
		[[nodiscard]] constexpr const std::source_location& Source() const noexcept { return m_location; }

        [[nodiscard]] std::string FormattedMessage() const { return std::format("Error {}: {} [{}:{}:{}]", m_code, m_message, m_location.file_name(),m_location.line(),m_location.column()); }

    private:
	    int m_code = 0;
	    std::string m_message;
		std::source_location m_location;
	};

	class ResultException : public std::exception
	{
	public:
	    explicit ResultException(const Error& error) noexcept : m_error(error), m_formattedMessage(error.FormattedMessage()) {}
	    
	    [[nodiscard]] const char* what() const noexcept override { return m_formattedMessage.c_str(); }
		[[nodiscard]] const Error& error() const noexcept { return m_error; }
	    
	private:
	    Error m_error;
	    std::string m_formattedMessage;
	};

    template <typename T, ErrorType E>
	class Result;

    template <typename T, template <typename...> class Template>
	struct is_specialization : std::false_type {};

	template <template <typename...> class Template, typename... Args>
	struct is_specialization<Template<Args...>, Template> : std::true_type {};

	template <typename T, template <typename...> class Template>
	static constexpr bool is_specialization_v = is_specialization<T, Template>::value;

	template <typename T>
	struct is_result : std::false_type {};

	template <typename T, ErrorType E>
	struct is_result<Result<T, E>> : std::true_type {};

	template <typename T>
	static constexpr bool is_result_v = is_result<T>::value;

	template <typename T, ErrorType E = Error>
	class Result
	{
	public:
		using ValueType = T;
        using ErrorType = E;

	    constexpr Result(const T& value) noexcept : m_data(value) {}
	    constexpr Result(T&& value) noexcept : m_data(std::move(value)) {}
	    constexpr Result(const E& error) noexcept : m_data(error) {}
	    constexpr Result(E&& error) noexcept : m_data(std::move(error)) {}

	    template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
        constexpr Result(const Result<U, E>& other) noexcept
        {
            if (other.HasValue())
            {
                m_data = static_cast<T>(other.GetValue());
            }
            else
            {
                m_data = other.GetError();
            }
        }

	    [[nodiscard]] constexpr explicit operator bool() const noexcept { return HasValue(); }

		[[nodiscard]] constexpr bool HasValue() const noexcept { return std::holds_alternative<T>(m_data); }
		[[nodiscard]] constexpr bool HasError() const noexcept { return std::holds_alternative<E>(m_data); }

	    [[nodiscard]] constexpr const T& GetValue() const &
	    {
            if (!HasValue())
                throw ResultException(GetError());
            return std::get<T>(m_data); 
        }
        
        [[nodiscard]] constexpr T& GetValue() &
        {
            if (!HasValue())
                throw ResultException(GetError());
            return std::get<T>(m_data); 
        }
        
        [[nodiscard]] constexpr T&& GetValue() &&
        {
            if (!HasValue())
                throw ResultException(GetError());
            return std::move(std::get<T>(m_data)); 
        }

        [[nodiscard]] constexpr T Unwrap()
        {
            if (!HasValue())
                throw ResultException(GetError());
            return std::move(std::get<T>(m_data));
        }
        
        [[nodiscard]] constexpr T Expect(std::string_view message)
        {
            if (!HasValue())
                throw ResultException({GetError().Code(), message});
            return std::move(std::get<T>(m_data));
        }
        
        [[nodiscard]] constexpr T ValueOr(T&& defaultValue) const { return HasValue() ? GetValue() : std::forward<T>(defaultValue); }

        [[nodiscard]] constexpr const E& GetError() const &
        {
            if (!HasError())
            	throw ResultException({0, "Attempted to access error when result has value"});
            return std::get<E>(m_data);
        }

        [[nodiscard]] constexpr std::optional<std::reference_wrapper<const T>> TryValue() const { return HasValue() ? std::cref(GetValue()) : std::nullopt; }
        [[nodiscard]] constexpr std::optional<std::reference_wrapper<const E>> TryError() const { return HasError() ? std::cref(GetError()) : std::nullopt; }

	    template <typename Func>
        constexpr auto AndThen(Func&& func) const -> std::invoke_result_t<Func, T> { return HasValue() ? std::invoke(std::forward<Func>(func), GetValue()) : std::invoke_result_t<Func, T>(GetError()); }
        
        template <typename Func>
        constexpr auto OrElse(Func&& func) const -> Result<T, E> { return HasError() ? std::invoke(std::forward<Func>(func), GetError()) : *this; }
        
        template <typename Func>
        constexpr auto Map(Func&& func) const -> Result<std::invoke_result_t<Func, T>, E> { return HasValue() ? Result<std::invoke_result_t<Func, T>, E>(std::invoke(std::forward<Func>(func), GetValue())) : Result<std::invoke_result_t<Func, T>, E>(GetError()); }
        
        template <typename Func>
        constexpr auto MapError(Func&& func) const -> Result<T, std::invoke_result_t<Func, E>> { return HasError() ? Result<T, std::invoke_result_t<Func, E>>(std::invoke(std::forward<Func>(func), GetError())) : Result<T, std::invoke_result_t<Func, E>>(GetValue()); }
        
        template <typename Func>
		constexpr Result<T, E>& Inspect(Func&& func)
		{
		    if (HasValue())
		        std::invoke(std::forward<Func>(func), GetValue());
		    return *this;
		}

		template <typename Func>
		constexpr Result<T, E>& InspectError(Func&& func)
		{
		    if (HasError())
		        std::invoke(std::forward<Func>(func), GetError());
		    return *this;
		}

		template <typename SuccessFunc, typename ErrorFunc>
	    constexpr auto Match(SuccessFunc&& successFunc, ErrorFunc&& errorFunc) const -> std::common_type_t<std::invoke_result_t<SuccessFunc, T>, std::invoke_result_t<ErrorFunc, E>> 
		{ 
		    return HasValue() ? 
		        std::invoke(std::forward<SuccessFunc>(successFunc), GetValue()) : 
		        std::invoke(std::forward<ErrorFunc>(errorFunc), GetError()); 
		}

        template <typename U = T>
	    constexpr auto Transpose() const -> std::enable_if_t<is_specialization_v<U, std::optional>, std::optional<Result<typename U::value_type, E>>>
	    {
	        if (HasError())
	            return std::optional<Result<typename U::value_type, E>>(Result<typename U::value_type, E>(GetError()));
	        const auto& opt = GetValue();
            return opt.has_value() ? std::optional<Result<typename U::value_type, E>>(Result<typename U::value_type, E>(opt.value())) : std::nullopt;
	    }

	    template <typename U = T>
	    constexpr auto Flatten() const -> std::enable_if_t<is_result_v<U>, Result<typename U::ValueType, E>> { return HasError() ? Result<typename U::ValueType, E>(GetError()) : GetValue(); }

    private:
		std::variant<T, E> m_data;

    	friend class Error;
    };

	template <ErrorType E>
    class Result<void, E>
    {
    public:
        using ValueType = void;
        using ErrorType = E;
        
        constexpr Result() noexcept : m_error(std::nullopt) {}
        constexpr Result(const E& error) noexcept : m_error(error) {}
        constexpr Result(E&& error) noexcept : m_error(std::move(error)) {}
        
        [[nodiscard]] constexpr explicit operator bool() const noexcept { return HasValue(); }

		[[nodiscard]] constexpr bool HasValue() const noexcept { return !m_error.HasValue(); }
		[[nodiscard]] constexpr bool HasError() const noexcept { return m_error.HasValue(); }
        
        [[nodiscard]] constexpr const E& GetError() const
        {
            if (!HasError())
				throw ResultException({0, "Attempted to access error when result does not have an error"});
            return *m_error; 
        }
        
        constexpr void Unwrap() const
        {
            if (HasError())
                throw ResultException(GetError());
        }
        
        constexpr void Expect(std::string_view message) const
        {
            if (HasError())
                throw ResultException({GetError().Code(), message});
        }
        
        template <typename Func>
        constexpr auto AndThen(Func&& func) const -> std::invoke_result_t<Func> { return HasValue() ? std::invoke(std::forward<Func>(func)) : std::invoke_result_t<Func>(GetError()); }
        
        template <typename Func>
        constexpr auto OrElse(Func&& func) const -> Result<void, E> { return HasError() ? std::invoke(std::forward<Func>(func), GetError()) : *this; }
        
        template <typename Func>
        constexpr auto Map(Func&& func) const -> Result<std::invoke_result_t<Func>, E> { return HasValue() ? Result<std::invoke_result_t<Func>, E>(std::invoke(std::forward<Func>(func))) : Result<std::invoke_result_t<Func>, E>(GetError()); }
        
        template <typename Func>
        constexpr auto MapError(Func&& func) const -> Result<void, std::invoke_result_t<Func, E>> { return HasError() ? Result<void, std::invoke_result_t<Func, E>>(std::invoke(std::forward<Func>(func), GetError())) : Result<void, std::invoke_result_t<Func, E>>(); }
        
        template <typename Func>
		constexpr Result<void, E>& Inspect(Func&& func)
		{
		    if (HasValue())
		        std::invoke(std::forward<Func>(func));
		    return *this;
		}

		template <typename Func>
		constexpr Result<void, E>& InspectError(Func&& func)
		{
		    if (HasError())
		        std::invoke(std::forward<Func>(func), GetError());
		    return *this;
		}

        template <typename SuccessFunc, typename ErrorFunc>
		constexpr auto Match(SuccessFunc&& successFunc, ErrorFunc&& errorFunc) const -> std::common_type_t<std::invoke_result_t<SuccessFunc>, std::invoke_result_t<ErrorFunc, E>> 
		{ 
		    return HasValue() ? 
		        std::invoke(std::forward<SuccessFunc>(successFunc)) : 
		        std::invoke(std::forward<ErrorFunc>(errorFunc), GetError()); 
		}

    private:
        std::optional<E> m_error;

        friend class Error;
    };

    template <typename T>
    Result(const T& value) -> Result<T, Error>;

    template <typename T>
    Result(T&& value) -> Result<std::decay_t<T>, Error>;

    template <ErrorType E, typename T = void>
    Result(const E& error) -> Result<T, E>;

    template <ErrorType E, typename T = void>
    Result(E&& error) -> Result<T, std::decay_t<E>>;

    template <typename T, ErrorType E>
    Result(const Result<T, E>& other) -> Result<T, E>;

    template <typename T, ErrorType E>
    Result(Result<T, E>&& other) -> Result<T, E>;

    template <typename U, ErrorType E, typename T = U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
    Result(const Result<U, E>& other) -> Result<T, E>;

    Result() -> Result<void, Error>;

    template <ErrorType E>
    Result(const E& error) -> Result<void, E>;

    template <ErrorType E>
    Result(E&& error) -> Result<void, std::decay_t<E>>;

    template <typename T>
	constexpr auto Ok(T&& value) noexcept { return Result<std::decay_t<T>>(std::forward<T>(value)); }

	template <typename T = void>
    constexpr Result<T, Error> Err(int code, std::string_view message, std::source_location location = std::source_location::current()) noexcept { return Result<T, Error>(Error(code, message, location)); }

	constexpr Result<void> Ok() noexcept { return Result<void>(); }

    template <typename E>
    struct PropagateError { E error; };

    template <typename T, ErrorType E>
    class [[nodiscard]] TryOperator
    {
    public:
        explicit TryOperator(Result<T, E>&& result) noexcept : m_result(std::move(result)) {}
        
        template <typename U = T, std::enable_if_t<!std::is_void_v<U>, int> = 0>
        operator U() &&
        {
            if (m_result.HasError())
                throw PropagateError<E>{m_result.GetError()};
            return std::move(m_result).GetValue();
        }
        
        template <typename U = T, std::enable_if_t<std::is_void_v<U>, int> = 0>
        void operator()() &&
        {
            if (m_result.HasError())
                throw PropagateError<E>{m_result.GetError()};
        }
        
    private:
        Result<T, E> m_result;
    };
    
    template <typename T, ErrorType E>
    TryOperator<T, E> Try(Result<T, E>&& result) noexcept { return TryOperator<T, E>(std::move(result)); }
}

#define FLOG_TRY(expr) \
    ([&]() -> decltype(auto) { \
        using ResultType = std::remove_cvref_t<decltype(expr)>; \
        using ValueType = typename ResultType::ValueType; \
        using ErrorType = typename ResultType::ErrorType; \
        \
        try { \
            if constexpr (std::is_void_v<ValueType>) { \
                FlexLog::Try(expr)(); \
                return; \
            } else { \
                return static_cast<ValueType>(FlexLog::Try(expr)); \
            } \
        } catch (const FlexLog::PropagateError<ErrorType>& e) { \
            return ResultType(e.error); \
        } \
    })()

namespace std
{
    template <typename T, FlexLog::ErrorType E>
    struct tuple_size<FlexLog::Result<T, E>> : std::integral_constant<size_t, 3> {};

    template <size_t I, typename T, FlexLog::ErrorType E>
    struct tuple_element<I, FlexLog::Result<T, E>>
    {
        using type = std::conditional_t<
            I == 0, 
            bool, 
            std::conditional_t<I == 1, T, E>
        >;
    };
}
