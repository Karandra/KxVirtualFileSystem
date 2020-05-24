#pragma once
#include <utility>
#include <type_traits>

namespace KxVFS
{
	template<class T>
	struct IsFlagSet: std::false_type {};

	template<class T>
	inline constexpr bool IsFlagSet_v = IsFlagSet<T>::value;

	#define KxVFS_DeclareFlagSet(T) template<> struct IsFlagSet<T>: std::true_type { static_assert(std::is_enum_v<T>, "enum type required"); }
}

namespace KxVFS
{
	template<class TInt, class TEnum>
	constexpr std::enable_if_t<std::is_enum_v<TEnum>, TInt> ToInt(TEnum value) noexcept
	{
		return static_cast<TInt>(value);
	}

	template<class TEnum, class TInt = std::underlying_type_t<TEnum>>
	constexpr std::enable_if_t<std::is_enum_v<TEnum>, TInt> ToInt(TEnum value) noexcept
	{
		return static_cast<TInt>(value);
	}

	template<class TEnum, class TInt>
	constexpr std::enable_if_t<std::is_enum_v<TEnum>, TEnum> FromInt(TInt value) noexcept
	{
		return static_cast<TEnum>(value);
	}

	template<class TEnum>
	constexpr std::enable_if_t<IsFlagSet_v<TEnum>, TEnum> operator|(TEnum left, TEnum right) noexcept
	{
		using Tint = std::underlying_type_t<TEnum>;
		return static_cast<TEnum>(static_cast<Tint>(left) | static_cast<Tint>(right));
	}

	template<class TEnum, class... Args>
	constexpr std::enable_if_t<(IsFlagSet_v<std::remove_const_t<std::remove_reference_t<Args>>> && ...), TEnum> CombineFlags(Args&&... arg) noexcept
	{
		return static_cast<TEnum>((static_cast<std::underlying_type_t<std::remove_const_t<std::remove_reference_t<Args>>>>(arg) | ...));
	}
}

namespace KxVFS
{
	template<class TEnum_>
	class FlagSet final
	{
		public:
			using TEnum = TEnum_;
			using TInt = std::underlying_type_t<TEnum_>;

		private:
			TEnum m_Value = static_cast<TEnum>(0);

		public:
			constexpr FlagSet() noexcept = default;
			constexpr FlagSet(TEnum values) noexcept
				:m_Value(values)
			{
			}
			constexpr FlagSet(const FlagSet&) noexcept = default;
			constexpr FlagSet(FlagSet&&) noexcept = default;

		public:
			constexpr bool IsNull() const noexcept
			{
				return ToInt() == 0;
			}
			constexpr FlagSet& Clear() noexcept
			{
				FromInt(0);
				return *this;
			}
			constexpr FlagSet Clone() const noexcept
			{
				return *this;
			}
			
			constexpr TEnum GetValue() const noexcept
			{
				return m_Value;
			}
			constexpr FlagSet& SetValue(TEnum value) noexcept
			{
				m_Value = value;
				return *this;
			}
			constexpr TEnum operator*() const noexcept
			{
				return m_Value;
			}

			template<class T = TInt>
			constexpr T ToInt() const noexcept
			{
				static_assert(std::is_integral_v<T> || std::is_enum_v<T>, "integer type required");

				return static_cast<T>(m_Value);
			}

			constexpr FlagSet& FromInt(TInt value) noexcept
			{
				m_Value = static_cast<TEnum>(value);
				return *this;
			}

			constexpr bool Contains(FlagSet other) const noexcept
			{
				return (ToInt() & other.ToInt()) != 0;
			}
			
			constexpr bool Add(FlagSet other) noexcept
			{
				FromInt(ToInt() | other.ToInt());
				return *this;
			}
			constexpr FlagSet& Add(FlagSet other, bool condition) noexcept
			{
				if (condition)
				{
					Add(other);
				}
				return *this;
			}
			
			constexpr FlagSet& Remove(FlagSet other) noexcept
			{
				FromInt(ToInt() & ~other.ToInt());
				return *this;
			}
			constexpr FlagSet& Remove(FlagSet other, bool condition) noexcept
			{
				if (condition)
				{
					Remove(other);
				}
				return *this;
			}
			
			constexpr FlagSet& Toggle(FlagSet other) noexcept
			{
				FromInt(ToInt() ^ other.ToInt());
				return *this;
			}
			constexpr FlagSet& Mod(FlagSet other, bool enable) noexcept
			{
				if (enable)
				{
					Add(other);
				}
				else
				{
					Remove(other);
				}
				return *this;
			}

		public:
			constexpr operator bool() const noexcept
			{
				return !IsNull();
			}
			constexpr bool operator!() const noexcept
			{
				return IsNull();
			}

			constexpr bool operator==(const FlagSet& other) const noexcept
			{
				return m_Value == other.m_Value;
			}
			constexpr bool operator!=(const FlagSet& other) const noexcept
			{
				return !(*this == other);
			}

			constexpr FlagSet& operator=(const FlagSet&) noexcept = default;
			constexpr FlagSet& operator=(FlagSet&&) noexcept = default;

			constexpr FlagSet& operator|=(FlagSet other) noexcept
			{
				FromInt(ToInt() | other.ToInt());
				return *this;
			}
			constexpr FlagSet& operator&=(FlagSet other) noexcept
			{
				FromInt(ToInt() & other.ToInt());
				return *this;
			}
			constexpr FlagSet& operator^=(FlagSet other) noexcept
			{
				FromInt(ToInt() ^ other.ToInt());
				return *this;
			}

			constexpr FlagSet operator|(FlagSet other) const noexcept
			{
				auto clone = Clone();
				clone |= other;
				return clone;
			}
			constexpr FlagSet operator&(FlagSet other) const noexcept
			{
				auto clone = Clone();
				clone &= other;
				return clone;
			}
			constexpr FlagSet operator^(TEnum other) const noexcept
			{
				auto clone = Clone();
				clone ^= other;
				return clone;
			}
			constexpr FlagSet operator~() const noexcept
			{
				return Clone().FromInt(~ToInt());
			}
	};
}
