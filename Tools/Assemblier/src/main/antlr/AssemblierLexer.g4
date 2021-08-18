lexer grammar AssemblierLexer;
@header {
    package hex;
}

WS : [ \t\n\r]+ -> skip;

COLON: ';';
LBRACE: '<';
RBRACE: '>';
LMID: '[';
RMID: ']';
BODY_BEGIN: '{';
BODY_END: '}';
PARAM_BEGIN: '(';
PARAM_END: ')';
EQ: '=';
DOT: '.';
COMMA: ',';
REF: '&';
JUNCTION: '::';

KEY_ASSEMBLY: '.assembly';
KEY_CLASS: '.class';
KEY_METHOD: '.method';
KEY_PROPERTY: '.property';

KEY_INHERIT: 'inherits';
KEY_IMPLEMENT: 'implements';

MODIFIER_ACCESS: MODIFIER_PUBLIC | MODIFIER_PRIVATE | MODIFIER_PROTECTED | MODIFIER_INTERNAL;
MODIFIER_LIFE: MODIFIER_INSTANCE | MODIFIER_STATIC;

MODIFIER_STATIC: 'static';
MODIFIER_INSTANCE: 'instance';

MODIFIER_NEST: 'nested';

MODIFIER_PUBLIC: 'public';
MODIFIER_PRIVATE: 'private';
MODIFIER_INTERNAL: 'internal';
MODIFIER_PROTECTED: 'protected';

CTOR: '.ctor' | '.cctor';
ARRAY : 'array';
VOID: 'void';

PRIMITIVE_TYPE: PRIMITIVE_INT |
        PRIMITIVE_LONG |
        PRIMITIVE_SHORT |
        PRIMITIVE_BYTE |
        PRIMITIVE_CHAR |
        PRIMITIVE_STRING;

//Support for primitive type abbr. form
PRIMITIVE_INT: 'int32';
PRIMITIVE_LONG: 'int64';
PRIMITIVE_SHORT: 'int16';
PRIMITIVE_BYTE: 'int8';
PRIMITIVE_CHAR: 'char';
PRIMITIVE_STRING: 'string';

METHOD_MANAGED: 'managed';
METHOD_IMPORT: 'import';

METHOD_LOCAL: '.local';
METHOD_CODE: '.code';

PROPERTY_GET: '.get';
PROPERTY_SET: '.set';
METHOD_PROPERTY: 'get' | 'set';

STRING: '"'.*?'"';
HEX: '0x'HEX_DIGIT+;
NUMBER: [0-9]+('.'[0-9]+)?;
INT: [0-9]+;
TOKEN: '0x'[0-9a-fA-F]{8};
GUID: HEX_DIGIT{8} '-' (HEX_DIGIT{4} '-'){3} HEX_DIGIT{12};

METHOD_OPCODE: '.'[a-zA-Z]+;

TYPE_REF: LMID IDENTIFIER RMID TYPE_NAME;
TYPE_NAME: (IDENTIFIER DOT)+ IDENTIFIER;

IDENTIFIER: [a-zA-Z_@][a-zA-Z0-9_]+;

PROPERTY_VALUE : STRING | NUMBER | TOKEN | GUID | HEX;
PROPERTY: IDENTIFIER EQ PROPERTY_VALUE COLON;
HEX_DIGIT: [0-9a-fA-F];