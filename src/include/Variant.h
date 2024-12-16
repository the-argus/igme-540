#pragma once
#include <type_traits>
#include <string>
#include "short_numbers.h"
#include "errors.h"

namespace ggp
{
	class Variant
	{
	public:
		enum Type : u8
		{
			Null,
			String,
			Int,
			Float,
			Bool,
		};
	
		template <typename T>
		static constexpr bool contains_type = std::is_same_v<T, std::string>
			|| std::is_same_v<T, i64>
			|| std::is_same_v<T, f64>
			|| std::is_same_v<T, std::nullopt_t>
			|| std::is_same_v<T, bool>;

		template <typename T>
		static constexpr bool accepts_type = contains_type<T>
			|| std::is_same_v<T, i32>
			|| std::is_same_v<T, i16>
			|| std::is_same_v<T, i8>
			|| std::is_same_v<T, f32>;

		inline constexpr Variant()
			: m_tag(Type::Null)
		{
		}

		template <typename T>
		inline constexpr Variant(const T& t) requires accepts_type<T>
		{
			if constexpr (std::is_same_v<T, f64> || std::is_same_v<T, f32>)
			{
				m_contents.floating = f64(t);
				m_tag = Type::Float;
			}
			else if constexpr (std::is_same_v<T, i64> || std::is_same_v<T, i32> || std::is_same_v<T, i16> || std::is_same_v<T, i8>)
			{
				m_contents.integer = i64(t);
				m_tag = Type::Int;
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				m_contents.boolean = bool(t);
				m_tag = Type::Bool;
			}
			else if constexpr (std::is_same_v<T, std::nullopt_t>)
			{
				m_tag = Type::Null;
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				m_contents.string = std::string(t);
				m_tag = Type::String;
			}
		}

		inline constexpr Variant(std::string&& t)
			: m_tag(Type::String)
		{
			m_contents.string = std::move(t);
		}

		inline constexpr Variant(const Variant& other)
			: m_tag(other.m_tag)
		{
			if (other.m_tag == Type::String)
				m_contents.string = other.m_contents.string;
			else
				std::memcpy(&m_contents, &other.m_contents, sizeof(m_contents));
		}

		inline constexpr Variant& operator=(const Variant& other)
		{
			this->~Variant();
			new (this) Variant(other);
			return *this;
		}

		inline constexpr Variant(Variant&& other)
			: m_tag(other.m_tag)
		{
			if (other.m_tag == Type::String)
				m_contents.string = std::move(other.m_contents.string);
			else
				std::memcpy(&m_contents, &other.m_contents, sizeof(m_contents));
		}

		inline constexpr Variant& operator=(Variant&& other)
		{
			this->~Variant();
			new (this) Variant(std::move(other));
			return *this;
		}

		inline constexpr ~Variant()
		{
			using string = std::string;
			if (m_tag == Type::String)
				m_contents.string.~string();
		}

		template <typename T>
		inline constexpr bool is() const noexcept requires contains_type<T>
		{
			if constexpr (std::is_same_v<T, f64>)
			{
				return m_tag == Type::Float;
			}
			else if constexpr (std::is_same_v<T, i64>)
			{
				return m_tag == Type::Int;
			}
			else if constexpr (std::is_same_v<T, std::nullopt_t>)
			{
				return m_tag == Type::Null;
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				return m_tag == Type::Bool;
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				return m_tag == Type::String;
			}
			else
			{
				static_assert(contains_type<T>,
					"Invalid type not contained by Variant- it will never be present.");
			}
		}

		template <typename T>
		inline constexpr const T& value() const noexcept
			requires contains_type<T> && (!std::is_same_v<T, std::nullopt_t>)
		{
			abort_if(!is<T>(), "Bad variant cast");
			if constexpr (std::is_same_v<T, f64>)
				return m_contents.floating;
			else if constexpr (std::is_same_v<T, i64>)
				return m_contents.integer;
			else if constexpr (std::is_same_v<T, bool>)
				return m_contents.boolean;
			else if constexpr (std::is_same_v<T, std::string>)
				return m_contents.string;
		}

		template <typename T>
		inline constexpr T& value() noexcept
			requires contains_type<T> && (!std::is_same_v<T, std::nullopt_t>)
		{
			abort_if(!is<T>(), "Bad variant cast");
			if constexpr (std::is_same_v<T, f64>)
				return m_contents.floating;
			else if constexpr (std::is_same_v<T, i64>)
				return m_contents.integer;
			else if constexpr (std::is_same_v<T, bool>)
				return m_contents.boolean;
			else if constexpr (std::is_same_v<T, std::string>)
				return m_contents.string;
		}

		operator std::string() const&
		{
			abort_if(!is<std::string>(), "Attempt to convert non-string Variant into std::string");
			return m_contents.string;
		}

		operator std::string() &&
		{
			abort_if(!is<std::string>(), "Attempt to convert non-string Variant into std::string");
			return std::move(m_contents.string);
		}

		operator i64() const&
		{
			abort_if(!is<i64>(), "Attempt to convert non-integer Variant into i64");
			return m_contents.integer;
		}

		operator f64() const&
		{
			abort_if(!is<f64>(), "Attempt to convert non-floating point Variant into f64");
			return m_contents.floating;
		}

		operator bool() const&
		{
			abort_if(!is<bool>(), "Attempt to convert non-boolean point Variant into bool");
			return m_contents.boolean;
		}

		inline constexpr Type tag()
		{
			return m_tag;
		}

		template <typename T>
		inline constexpr friend bool operator==(const Variant& v, const T& other) requires accepts_type<T>
		{
			if constexpr (std::is_same_v<T, f64> || std::is_same_v<T, f32>)
			{
				return v.m_tag == Type::Float && v.m_contents.floating == other;
			}
			else if constexpr (std::is_same_v<T, i64> || std::is_same_v<T, i32> || std::is_same_v<T, i16> || std::is_same_v<T, i8>)
			{
				return (v.m_tag == Type::Int && v.m_contents.integer == other)
					|| (v.m_tag == Type::Float && v.m_contents.floating == other);
			}
			else if constexpr (std::is_same_v<T, bool>)
			{
				return v.m_tag == Type::Bool && v.m_contents.boolean == other;
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				return v.m_tag == Type::String && v.m_contents.string == other;
			}
			else if constexpr (std::is_same_v<T, std::nullopt_t>)
			{
				return v.m_tag == Type::Null;
			}
			else
			{
				static_assert(accepts_type<T>,
					"Invalid type not contained by Variant- it will never be present.");
			}
		}

		inline constexpr friend bool operator==(const Variant& v, const char* other)
		{
			return v.m_tag == Type::String && v.m_contents.string == other;
		}

	private:
		union contents_t
		{
			std::string string;
			i64 integer;
			f64 floating;
			bool boolean;
			inline constexpr ~contents_t() {}
			inline constexpr contents_t() {}
		} m_contents;
		Type m_tag;
	};
}