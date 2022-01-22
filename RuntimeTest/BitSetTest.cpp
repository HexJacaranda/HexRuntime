#include "pch.h"
#include "CppUnitTest.h"
#include "../HexRuntime/Runtime/RuntimeAlias.h"
#include "../HexRuntime/Runtime/Bit.h"

using namespace RT;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UtilityTest
{
	TEST_CLASS(BitSetTest)
	{
	public:
		TEST_METHOD(TestSet)
		{
			BitSet set(257);
			set.SetOne(256);
			Assert::AreEqual(set.Test(256), true);
		}

		TEST_METHOD(TestOne)
		{
			{
				BitSet set{ 2 };
				set.SetOne(0);
				set.SetOne(1);
				Assert::IsTrue(set.IsOne());
			}

			{
				BitSet set{ 2 };
				set.SetOne(0);
				Assert::IsFalse(set.IsOne());
			}

			{
				BitSet set{ 257 };
				for (Int32 i = 0; i < 257; ++i)
					set.SetOne(i);
				Assert::IsTrue(set.IsOne());
			}
		}

		TEST_METHOD(TestZero)
		{
			{
				BitSet set{ 2 };
				Assert::IsTrue(set.IsZero());
			}
			{
				BitSet set{ 257 };
				Assert::IsTrue(set.IsZero());
			}
			{
				BitSet set{ 2 };
				set.SetOne(0);
				Assert::IsFalse(set.IsZero());
			}
		}

		TEST_METHOD(ScanSmallest)
		{
			{
				BitSet set{ 16 };
				set.SetOne(8);

				set.SetOne(10);

				Assert::AreEqual(8, set.ScanSmallestSetIndex());
				Assert::AreEqual(0, set.ScanSmallestUnsetIndex());
			}
		}

		TEST_METHOD(ScanBiggest)
		{
			BitSet set{ 16 };
			set.SetOne(8);

			set.SetOne(10);

			Assert::AreEqual(10, set.ScanBiggestSetIndex());
			Assert::AreEqual(15, set.ScanBiggestUnsetIndex());
		}
	};
}