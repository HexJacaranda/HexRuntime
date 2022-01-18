#pragma once
#include "..\RuntimeAlias.h"

namespace RTH
{
	class IRuntimeHost
	{
	public:
		virtual Int32 StartUp() = 0;
	};
}