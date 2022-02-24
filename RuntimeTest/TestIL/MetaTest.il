.assembly
{
    name = "MetaTest"
    major = 1
    minor = 0
    groupname = "HexRT"
    guid = F0000000-0000-0000-0000-000000000000
}

.class public instance SimpleGeneric
.inherits [Core]Object
.generic T
{
    .field public instance !T X
    .field public instance array<!T> Y
}