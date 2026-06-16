#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/sema.h"
#include "codegen/codegen.h"
#include "common/version.h"
#include "common/platform.h"
#include "importer/importer.h"


int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "bery: error: no input file\n";
        return 1;
    }

    if (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-v") {
        std::cout << "Bery " << BERY_VERSION << "\n";
        return 0;
    }


    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "bery: error: file not found\n";
        return 3;
    }

    std::string sourcePath = argv[1];
    std::string basePath = "";
    size_t lastSlash = sourcePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        basePath = sourcePath.substr(0, lastSlash + 1);
    }

    std::stringstream buf;
    buf << file.rdbuf();
    std::string source = buf.str();


    Lexer lexer(source);
    auto tokens = lexer.tokanize();

    Parser parser(tokens);
    auto ast = parser.parse();

    if (lexer.hasErrors() || parser.hasErrors()) { 
        std::cerr << "Bery: Compilation halted due to syntax errors.\n"; 
        return 5; 
    }
    Importer importer;
    importer.resolveImports(static_cast<ProgramNode*>(ast.get()), basePath);

    SemanticAnalyzer sema(ast.get());
    sema.analyze();

    if (sema.hasErrors()) { 
        std::cerr << "Bery: Compilation halted due to semantic errors.\n"; 
        return 6; 
    }

    std::string irFile = "bery_out.ll";
    CodeGen codegen(ast.get(), sema.symbolTable);
    codegen.generate(irFile);

    std::cout << "Bery: " << BERY_VERSION << " compiled successfully\n";
    return 0;
}
