%{
    #include "y.tab.h"   
%}

keyword \.[a-z]+                
identifier [a-zA-Z_]?[_\w]*     
number \d+(\.\d+)?              
hexnumber 0x[0-9a-fA-F]+
string ".*"

%%
{keyword} { return KeywordT; }
{identifier} { return IdentifierT; }
{number} { return NumberT; }
{hexnumber} { return HexT; }
{string} { return StringT; }
. { return *yytext; }
%%


