#pragma once
#include "..\..\RuntimeAlias.h"

namespace RTM
{
	template<class MDType>
	class Descriptor
	{
		friend class MetaManager;
	protected:
		MDType* mColdMD;
	public:
		MDType* GetMetadata()const { return mColdMD; }
	};
}