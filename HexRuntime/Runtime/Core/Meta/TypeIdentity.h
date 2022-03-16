#pragma once
#include "..\..\RuntimeAlias.h"
#include "TypeDescriptor.h"

namespace RTM
{
	struct TypeIdentity
	{
	public:
		TypeDescriptor* Canonical;
		Int32 ArgumentCount;
		union
		{
			TypeDescriptor** Arguments;
			TypeDescriptor* SingleArgument = nullptr;
		};

		TypeIdentity()
			:Canonical(nullptr), ArgumentCount(0) {}

		TypeIdentity(TypeDescriptor* normal)
			:Canonical(normal), ArgumentCount(0) {}

		TypeIdentity(TypeDescriptor* canonical, TypeDescriptor* singleArgument)
			:Canonical(canonical), ArgumentCount(1), SingleArgument(singleArgument) {}

		UInt32 GetHashCode()const
		{
			if (ArgumentCount == 0)
				return ComputeHashCode(Canonical);
			else if (ArgumentCount == 1)
				return ComputeHashCode(Canonical) + ComputeHashCode(SingleArgument);
			else
				return ComputeHashCode(Canonical) + ComputeHashCode(Arguments, sizeof(TypeDescriptor*) * ArgumentCount);
		}
	};

	struct TypeIdentityHash
	{
		inline UInt32 operator()(TypeIdentity const& identity)const
		{
			return identity.GetHashCode();
		}
	};

	struct TypeIdentityEqual
	{
		inline bool operator()(TypeIdentity const& left, TypeIdentity const& right)const
		{
			if (left.Canonical != right.Canonical)
				return false;
			if (left.ArgumentCount != right.ArgumentCount)
				return false;

			if (left.ArgumentCount == 0)
				return true;
			else if (left.ArgumentCount == 1)
				return left.SingleArgument == right.SingleArgument;
			else
				return std::memcmp(left.Arguments, right.Arguments, sizeof(TypeDescriptor*) * left.ArgumentCount) == 0;
		}
	};
}