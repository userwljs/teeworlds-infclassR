#pragma once

#define DECLARE_FLAGS(FlagsName, FlagsEnum) \
	class FlagsName \
	{ \
	public: \
		FlagsName() = default; \
		constexpr FlagsName(FlagsEnum Key1) : \
			FlagsName(static_cast<int>(Key1)) {} \
\
		constexpr FlagsName operator|(FlagsEnum Flag) const \
		{ \
			return FlagsName(m_Flags | static_cast<int>(Flag)); \
		} \
\
		constexpr bool operator&(FlagsEnum Flag) const \
		{ \
			return m_Flags & static_cast<int>(Flag); \
		} \
\
	private: \
		constexpr FlagsName(int Flags) : m_Flags(Flags) {} \
		int m_Flags{}; \
	}; \
\
	inline constexpr FlagsName operator|(FlagsEnum Flag1, FlagsEnum Flag2) \
	{ \
		return FlagsName(Flag1) | Flag2; \
	}
