#pragma once
#include "..\..\RuntimeAlias.h"
#include "Object.h"

namespace RTO
{
	class StringObject :public Object
	{
	private:
		Int32 mCount;
	public:
		StringObject(Int32 count); 
		StringObject(Int32 count, RTString value);
		Int32 GetCount()const;
		RTString GetContent()const;
	};
}