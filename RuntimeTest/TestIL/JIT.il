.assembly
{
    name = "JIT"
    major = 1
    minor = 0
    groupname = "HexRT"
    guid = 00000000-0000-0000-0000-000000000002
}

.class public instance JITTest
.namespace Test
{
    .method public instance int32 PreTest() managed
    {
        .code
        {
            .ldc int32 1
            .ldc int32 1
            .add
            .ret
        }
    }

    .method public static int32 SimpleMethod(int32 x, int32 y) managed
    {
        .code
        {
            .ldc int32 1
            .ret
        }
    }

    .method public instance int32 LinearizeTest(int32 x, int32 y) managed
    {
        .code
        {
            .ldarg x
            .ldarg y
            .add
            .ldarg x
            .add
            .ldarg x
            .cmp .eq
            .jcc TRUE_BR

            .ldc int32 1
            .ret

            .label TRUE_BR

            .ldarg x
            .ldarg y
            .call [JIT][Test]JITTest::SimpleMethod(int32, int32)
            .ret
        }
    }

    .method public instance int32 SSABuildTest(int32 count) managed
    {
        .local
        {
            int32 i
            int32 sum
        }
        .code
        {
            .ldc int32 0
            .stloc i
            .ldc int32 0
            .stloc sum
            
            .jmp COND
            .label BODY_BEGIN
            
            .ldloc i
            .ldloc sum
            .add
            .stloc sum

            .ldloc i
            .ldc int32 1
            .add
            .stloc i

            .label COND
            .ldloc i
            .ldarg count
            .cmp .lt
            .jcc BODY_BEGIN

            .ldloc sum
            .ret
        }
    }

    .method public instance int32 SSAOptimizationTest(int32 x, int32 y) managed
    {
        .local
        {
            int32 a
            int32 b
        }
        .code
        {
            .ldc int32 1
            .stloc a
            .ldc int32 1
            .stloc b

            .ldc int32 2
            .stloc a

            .ldloc a
            .ldloc b
            .cmp .eq
            .jcc TRUE_BR

            .ldarg x
            .ret

            .label TRUE_BR
              
            .ldarg y
            .ret
        }
    }

    .method public static void CodeGenTest1() managed
    {
        .local
        {
            int32 a
            int32 b
            int32 c
        }
        .code
        {
            .ldc int32 2
            .stloc b
            
            .ldc int32 3
            .stloc c

            .ldc int32 1
            .stloc a

            .ldloc b
            .stloc a

            .ldloc b
            .ldloc c
            .add
            .stloc a

            .ret
        }
    }

    .method public static int32 CodeGenTest2(int32 x, int32 y, int32 z, int32 v) managed
    {
        .code
        {
           .ldarg z
           .ldarg v
           .add
           .ret
        }
    }

    .method public static int32 CodeGenTest3(int32 x, int32 y, int32 z, int32 v) managed
    {
        .local
        {
            int32 x0
            int32 x1
            int32 x2
            int32 x3
            int32 x4
            int32 x5
            int32 x6
            int32 x7
            int32 x8
            int32 x9
            int32 x10
            int32 x11
            int32 x12
            int32 x13
            int32 x14
            int32 x15
        }
        .code
        {
            .ldc int32 0
            .stloc x0
            .ldc int32 1
            .stloc x1
            .ldc int32 2
            .stloc x2
            .ldc int32 3
            .stloc x3
            .ldc int32 4
            .stloc x4
            .ldc int32 5
            .stloc x5
            .ldc int32 6
            .stloc x6
            .ldc int32 7
            .stloc x7
            .ldc int32 8
            .stloc x8
            .ldc int32 9
            .stloc x9
            .ldc int32 10
            .stloc x10
            .ldc int32 11
            .stloc x11
            .ldc int32 12
            .stloc x12
            .ldc int32 13
            .stloc x13
            .ldc int32 14
            .stloc x14
            .ldc int32 15
            .stloc x15

            .ldloc x0
            .ret
        }
    }

    .method public static float CodeGenFloatImm() managed
    {
        .code
        {
            .ldc float 3.14
            .ret
        }
    }

    .method public static double CodeGenDoubleImm() managed
    {
        .code
        {
            .ldc double 2.718
            .ret
        }
    }


    .method public static int32 CodeGenIntAdd(int32 x) managed
    {
        .code
        {
            .ldarg x
            .ldc int32 -1
            .add
            .ret
        }
    }

    .method public static int32 CodeGenIntSub(int32 x) managed
    {
        .code
        {
            .ldarg x
            .ldc int32 -1
            .sub
            .ret
        }
    }

    .method public static int32 CodeGenIntMul(int32 x) managed
    {
        .code
        {
            .ldarg x
            .ldc int32 3
            .mul
            .ret
        }
    }

    .method public static int32 CodeGenIntDiv(int32 x) managed
    {
        .code
        {
            .ldarg x
            .ldc int32 2
            .div
            .ret
        }
    }

    .method public static float CodeGenFloatAdd(float x) managed
    {
        .code
        {
            .ldarg x
            .ldc float -2.0
            .add
            .ret
        }
    }

    .method public static float CodeGenFloatSub(float x) managed
    {
        .code
        {
            .ldarg x
            .ldc float 2.0
            .sub
            .ret
        }
    }

    .method public static float CodeGenFloatMul(float x) managed
    {
        .code
        {
            .ldarg x
            .ldc float 2.0
            .mul
            .ret
        }
    }

    .method public static float CodeGenFloatDiv(float x) managed
    {
        .code
        {
            .ldarg x
            .ldc float 2.0
            .div
            .ret
        }
    }

    .method public static double CodeGenDoubleAdd(double x) managed
    {
        .code
        {
            .ldarg x
            .ldc double -2.0
            .add
            .ret
        }
    }

    .method public static double CodeGenDoubleSub(double x) managed
    {
        .code
        {
            .ldarg x
            .ldc double 2.0
            .sub
            .ret
        }
    }

    .method public static double CodeGenDoubleMul(double x) managed
    {
        .code
        {
            .ldarg x
            .ldc double 2.0
            .mul
            .ret
        }
    }

    .method public static double CodeGenDoubleDiv(double x) managed
    {
        .code
        {
            .ldarg x
            .ldc double 2.0
            .div
            .ret
        }
    }

    .method public static int32 CodeGenIntNeg(int32 x) managed
    {
        .code
        {
            .ldarg x
            .neg
            .ret
        }
    }

    .method public static float CodeGenFloatNeg(float x) managed
    {
        .code
        {
            .ldarg x
            .neg
            .ret
        }
    }

    .method public static double CodeGenDoubleNeg(double x) managed
    {
        .code
        {
            .ldarg x
            .neg
            .ret
        }
    }

    .method public static int32 CodeGenIntMod(int32 x, int32 y) managed
    {
        .code
        {
            .ldarg x
            .ldarg y
            .mod
            .ret
        }
    }

    .method public static int32 CodeGenJcc(int32 x, int32 y) managed
    {
        .code
        {
            .ldarg x
            .ldarg y
            .cmp .eq
            .jcc TRUE_BR
            .ldarg x
            .ldc int32 1
            .add
            .ret
            .label TRUE_BR
            .ldarg y
            .ldc int32 2
            .mul
            .ret
        }
    }

    .method public static int32 CodeGenJcc1(int32 x, int32 y) managed
    {
        .local
        {
            bool z
            bool u
        }
        .code
        {
            .ldarg x
            .ldarg y
            .cmp .eq
            .stloc z

            .ldarg x
            .ldarg y
            .cmp .le
            .stloc u

            .ldloc z
            .jcc TRUE_BR

            .ldarg x
            .ldc int32 1
            .add
            .ret

            .label TRUE_BR
            .ldarg y
            .ldc int32 2
            .mul
            .ret
        }
    }

    .method public static bool CodeGenBoolAnd(bool x, bool y) managed
    {
        .code
        {
            .ldarg x
            .ldarg y
            .and
            .ret
        }
    }

    .method public static bool CodeGenBoolOr(bool x, bool y) managed
    {
        .code
        {
            .ldarg x
            .ldarg y
            .or
            .ret
        }
    }

    .method public static bool CodeGenBoolXor(bool x, bool y) managed
    {
        .code
        {
            .ldarg x
            .ldarg y
            .xor
            .ret
        }
    }

    .method public static bool CodeGenPreserveArg(bool x, bool y) managed
    {
        .local
        {
            bool z
        }
        .code
        {
            .ldarg x
            .ldarg y
            .and
            .stloc z

            .ldarg x
            .ldarg y
            .or
            .ret
        }
    }

    .method public static int32 CodeGenI16ToI32(int16 x) managed
    {
        .code
        {
           .ldarg x
           .conv int32
           .ret
        }
    }

    .method public static int32 CodeGenI8ToI32(int8 x) managed
    {
        .code
        {
           .ldarg x
           .conv int32
           .ret
        }
    }

    .method public static int32 CodeGenDoubleToI32(double x) managed
    {
        .code
        {
            .ldarg x
            .conv int32
            .ret
        }
    }

    .method public static double CodeGenI32ToDouble(int32 x) managed
    {
        .code
        {
            .ldarg x
            .conv double
            .ret
        }
    }

    .method public static int16 CodeGenI32ToI16(int32 x) managed
    {
        .code
        {
            .ldarg x
            .conv int16
            .ret
        }
    }

    .method public static int32 CodeGenRightShift(int32 x) managed
    {
        .code
        {
            .ldarg x
            .ldc int32 2
            .shr
            .ret
        }
    }

    .method public static uint32 CodeGenUnsignedRightShift(uint32 x) managed
    {
        .code
        {
            .ldarg x
            .ldc int32 2
            .shr
            .ret
        }
    }

    .method public static int32 CodeGenLeftShift(int32 x) managed
    {
        .code
        {
            .ldarg x
            .ldc int32 2
            .shl
            .ret
        }
    }
}

.struct public instance Struct
.namespace Test
{
    .field public instance int32 X
    .field public instance float Y
    .field public instance [JIT][Test]Struct2 Z
}

.struct public instance Struct2
.namespace Test
{
    .field public instance float X
}

.class public instance JITTest2
.namespace Test
{
    .method public static float TestStructLocal() managed
    {
        .local
        {
            [JIT][Test]Struct X
        }
        .code
        {
            .ldloc X
            .ldc float 1.0
            .stfld [JIT][Test]Struct::Y

            .ldloc X
            .ldfld [JIT][Test]Struct::Y
            .ret
        }
    }

    .method public static float TestLocalStructAddress() managed
    {
        .local
        {
            [JIT][Test]Struct X
            float& Z
        }
        .code
        {
            .ldloc X
            .ldflda [JIT][Test]Struct::Z
            .ldflda [JIT][Test]Struct2::X
            .stloc Z
            
            .ldloc Z
            .ldc float 2.0
            .stta

            .ldloc Z
            .ldind
            .ret
        }
    }

    .method public static float TestNestLocalStruct() managed
    {
        .local
        {
            [JIT][Test]Struct X
            float& Z
        }
        .code
        {
            .ldloc X
            .ldflda [JIT][Test]Struct::Z
            .ldflda [JIT][Test]Struct2::X
            .stloc Z
            
            .ldloc Z
            .ldc float 2.0
            .stta

            .ldloc X
            .ldfld [JIT][Test]Struct::Z
            .ldfld [JIT][Test]Struct2::X
            .ret
        }
    }

    .method public static int32 TestStoreElimination(int32 x, int32 y) managed
    {
        .code
        {
            .ldc int32 2
            .ldarg x
            .add

            .conv float
            .ldc float 3.0
            .add
            .conv int32       
            .starg y

            .ldarg x
            .ret
        }
    }
}

