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
}