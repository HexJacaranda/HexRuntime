#pragma once
#include "..\..\..\..\RuntimeAlias.h"
#include "..\..\..\..\LinkList.h"
#include "PlatformSpecialization.h"
#include "..\HexJITContext.h"
#include "..\JITFlow.h"
#include "..\IR.h"

namespace RTJ::Hex
{
	/// <summary>
	/// Responsible for attaching platform specific information for linearized IR
	/// </summary>
	class IPlatformSpecializer : public IHexJITFlow
	{
	public:

	};
}