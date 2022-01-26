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
}