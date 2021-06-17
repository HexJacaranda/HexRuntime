#pragma once
#include "..\..\RuntimeAlias.h"
#include "..\Type\Type.h"
#include "..\Object\StringObject.h"
#include "MDRecords.h"
#include "MDImporter.h"

namespace RTM
{
	/// <summary>
	/// Manager that responsible for metadata management
	/// </summary>
	class MetaManager
	{
		MDImporter* mImporters;
	public:
		static Type* GetTypeFromToken(MDToken typeReference);
		static RTO::StringObject* GetStringFromToken(MDToken stringToken);
		
	};
}