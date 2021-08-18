parser grammar assemblier;

options {
    // 表示解析token的词法解析器使用SearchLexer
    tokenVocab = assemblierLexer;
}

start: class_def*;

//Method
//1. Argument
method_argument: type IDENTIFIER;
method_argument_list: (method_argument COMMA method_argument_list) | ;
//2. Return type
method_return_type: VOID | type;
//3. name
method_name: CTOR | IDENTIFIER;
//4. special mark
method_import: METHOD_IMPORT LMID STRING COMMA STRING RMID;
method_source: METHOD_MANAGED | method_import;
//- DEF

method_local: LMID INT RMID type IDENTIFIER;

method_locals: METHOD_LOCAL
    BODY_BEGIN
        (method_local COLON)+
    BODY_END;

//IL
method_code: METHOD_CODE
    BODY_BEGIN
        method_il+
    BODY_END;

method_il: METHOD_OPCODE method_opcode_operand*;
method_opcode_operand: HEX | NUMBER | STRING | method_ref | field_ref | type | TYPE_NAME;

method_body: method_locals? method_code;
method_def: KEY_METHOD
    MODIFIER_ACCESS
    MODIFIER_LIFE
    METHOD_PROPERTY?
    method_return_type method_name PARAM_BEGIN method_argument_list PARAM_END method_source
    BODY_BEGIN
        method_body
    BODY_END;

method_ref: method_return_type TYPE_REF JUNCTION (IDENTIFIER|CTOR) PARAM_BEGIN type* PARAM_END;

//Field
field_def: MODIFIER_ACCESS MODIFIER_LIFE type IDENTIFIER;
field_ref: TYPE_REF JUNCTION IDENTIFIER;

//Property
property_get: PROPERTY_GET method_ref;
property_set: PROPERTY_SET method_ref;
property_def: KEY_PROPERTY
    BODY_BEGIN
        (property_get COLON)?
        (property_set COLON)?
    BODY_END;

//Class
type_ref_list: (TYPE_REF COMMA)* TYPE_REF;
implement_list: KEY_IMPLEMENT type_ref_list;

//Type part
type: (PRIMITIVE_TYPE | TYPE_REF | type_array | type_interior_ref);

type_array: type_nest_array | type_multidimension_array;
type_nest_array: ARRAY LBRACE type RBRACE;
type_multidimension_array: ARRAY LBRACE type COMMA INT RBRACE;
type_interior_ref: (PRIMITIVE_TYPE | TYPE_REF | type_array) REF;

class_body: (method_def | property_def | field_def COLON | class_def)*;

class_def: KEY_CLASS MODIFIER_NEST? MODIFIER_ACCESS MODIFIER_LIFE TYPE_NAME
(KEY_INHERIT TYPE_REF)?
implement_list?
'{'
    class_body
'}';


