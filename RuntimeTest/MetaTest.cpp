#include "pch.h"
#include <filesystem>
#include "CppUnitTest.h"
#include "../HexRuntime/Runtime/RuntimeAlias.h"
#include "../HexRuntime/Runtime/Core/Meta/MetaManager.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace RTM;

namespace RuntimeTest
{
	TEST_CLASS(MetaTest)
	{
	public:
		
		TEST_METHOD(MetaManagerLoadingTest)
		{
			MetaData = new MetaManager();
			auto context = MetaData->StartUp(Text("Core"));
			auto type = MetaData->GetTypeFromToken(context, 0);
		}
	};
}
