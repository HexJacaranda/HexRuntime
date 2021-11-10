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

            .ldc int32 2
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
}