#pragma once
#include "..\..\RuntimeAlias.h"
namespace RTE
{
	void Throw(RTString Message) noexcept(false);
}
#define THROW(MSG) throw (Text(MSG))