#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/sema.h"
#include "codegen/codegen.h"


int main(int argc, char* argv[]) {
   if (argc < 2) {
       std::cerr << "bery: error: no input file\n";
       return 1;
   }


   std::ifstream file(argv[1]);
   if (!file.is_open()) {
       std::cerr << "bery: error: file not found\n";
       return 3;
   }

   std::stringstream buf;
   buf << file.rdbuf();
   std::string source = buf.str();


   Lexer lexer(source);
   auto tokens = lexer.tokanize();
   if (lexer.hasErrors()) { std::cerr << "bery: lexer errors\n"; return 4; }


   Parser parser(tokens);
   auto ast = parser.parse();
   if (parser.hasErrors()) { std::cerr << "bery: parser errors\n"; return 5; }


   SemanticAnalyzer sema(ast.get());
   sema.analyze();
   if (sema.hasErrors()) { std::cerr << "bery: semantic errors\n"; return 6; }


   std::string irFile = "bery_out.ll";
CodeGen codegen(ast.get());
codegen.generate(irFile);


std::cout << "Bery: Success !!\n";
return 0;
}
