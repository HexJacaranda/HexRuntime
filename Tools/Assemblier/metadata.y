%{
    #include <stdio.h>
    void yyerror(const char* msg) {}
    int yylex();
%}

%token AssemblyKeyword ".assebmly"
%token AssemblyName ".name"
%token AssemblyMajor ".major"
%token AssemblyMinor ".minor"
%token AssemblyGroup ".group"
%token AssemblyGUID ".guid"

%token KeywordT
%token IdentifierT
%token NumberT
%token HexT
%token StringT

%%
AssemblyHeadBody: 
'{' 
    ".name" StringT ';'
    ".group" StringT ';'
    ".major" HexT ';'
    ".minor" HexT ';'
    ".guid" StringT ';' 
'}' { printf("header body"); }

AssemblyHead :  ".assebmly" AssemblyHeadBody
%%

int main(int argc, char **argv)
{
    char buffer[BUFSIZ];
    while (1)
    {
      printf("****************\n");
      char* input = fgets(buffer, sizeof buffer, stdin);
      if (buffer == NULL) break;
      set_input(input);
      yyparse();
    }
    return 0;
}

void yyerror (char const *s) {
   fprintf (stderr, "%s\n", s);
}

