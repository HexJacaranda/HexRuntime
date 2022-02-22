#pragma once
#include "RuntimeAlias.h"
#include "Utility.h"
#include <array>

namespace RT
{
	struct Guid
	{
		UInt32 X;
		UInt16 Y;
		UInt16 Z;
		union {
			std::array<UInt16, 4> U = { 0, 0, 0, 0 };
			UInt16 U0;
			UInt16 U1;
			UInt16 U2;
			UInt16 U3;
		};		
	public:
		Guid() : X(0), Y(0), Z(0) {}
		Guid(UInt32 x, UInt16 y, UInt16 z, std::array<UInt16, 4> const& u)
			: X(x), Y(y), Z(z), U(u) {}

		inline UInt32 GetHashCode()const
		{
			return ComputeHashCode(*this);
		}
	};

	struct GuidHash
	{
		inline UInt32 operator()(Guid const& guid)const
		{
			return guid.GetHashCode();
		}
	};

	struct GuidEqual
	{
		inline bool operator()(Guid const& left, Guid const& right)const
		{
			return memcmp(&left, &right, sizeof(Guid)) == 0;
		}
	};
}