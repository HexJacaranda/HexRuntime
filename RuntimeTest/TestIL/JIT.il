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

    .method public instance int32 LinearizeTest() managed
    {
        .local
        {
            int32 x
            int32 y
        }
        .code
        {
            .ldc int32 2
            .stloc x
            .ldc int32 3
            .stloc y

            .ldloc x
            .ldloc y
            .add
            .ldloc x
            .cmp .eq
            .jcc TRUE_BR

            .ldc int32 1
            .stloc x
            .ldloc x
            .ret

            .label TRUE_BR

            .ldc int32 2
            .stloc y
            .ldloc y
            .ret
        }
    }
}