#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "sema/sema.h"
#include "codegen/codegen.h"
#include "common/version.h"
#include "common/platform.h"
#include "common/toolchain.h"
#include "importer/importer.h"

static void printUsage() {
    std::cerr << "Usage:\n";
    std::cerr << "  bery compile <file.bry>   Compile to native binary\n";
    std::cerr << "  bery run <file.bry>       Compile and run\n";
    std::cerr << "  bery --version            Print version\n";
}
static std::string stemOf(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string filename = (slash == std::string::npos) ? path : path.substr(slash + 1);
    size_t dot = filename.find_last_of('.');
    return (dot == std::string::npos) ? filename : filename.substr(0, dot);
}

static std::string dirOf(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return ".";
    return path.substr(0, slash);
}

static std::string getExeDir(const char* argv0) {
    std::string path = argv0;
    size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return ".";
    return path.substr(0, slash);
}

static std::string findBRELib(const std::string& exeDir) {
    const char* names[] = { "libbre.a", "bre.lib", nullptr };
    const std::string dirs[] = {
        exeDir,
        exeDir + "/../build",
        "build",
        ".",
        ""
    };
    for (int d = 0; !dirs[d].empty(); ++d) {
        for (int n = 0; names[n]; ++n) {
            std::string candidate = dirs[d] + BERY_PATH_SEP + names[n];
            std::ifstream f(candidate);
            if (f.good()) return candidate;
        }
    }
    return "";
}

static int runFrontend(const std::string& sourcePath,   const std::string& irPath) {
    std::ifstream file(sourcePath);
    if (!file.is_open()) {
        std::cerr << "Bery: Error: cannot open file '" << sourcePath << "'\n";
        return 3;
    }

    std::string basePath = dirOf(sourcePath) + BERY_PATH_SEP;

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
    CodeGen codegen(ast.get(), sema.symbolTable);
    codegen.generate(irPath);
    return 0;
}

static int cmdCompile(const std::string& sourcePath, std::string& outBinaryPath, const std::string& exeDir) {
    BeryToolChain tc = detectToolchain();
    if (!tc.valid) {
        std::cerr << "Bery: Error: no LLVM toolchain found.\n\tInstall llc and g++ (Linux), clang++ (macOS/Windows).\n";
        return 10;
    }

    std::string breLib = findBRELib(exeDir);
    if (breLib.empty()) {
        std::cerr << "Bery: Error: cannot find libbre.a.\n\tRun 'cmake --build build' first.\n";
        return 11;
    }

    std::string stem  = stemOf(sourcePath);
    std::string dir   = dirOf(sourcePath);
    std::string irFile  = dir + BERY_PATH_SEP + stem + ".ll";
    std::string objFile = dir + BERY_PATH_SEP + stem + tc.objectExt;
    outBinaryPath       = dir + BERY_PATH_SEP + stem + tc.binaryExt;
    int fe = runFrontend(sourcePath, irFile);
    if (fe != 0) return fe;
    std::string compileCmd = buildCompileCmd(tc, irFile, objFile);
    if (system(compileCmd.c_str()) != 0) {
        std::cerr << "Bery: Error: llc failed.\n";
        return 12;
    }
    
    std::string linkCmd = buildLinkCmd(tc, objFile, breLib, outBinaryPath);
    if (system(linkCmd.c_str()) != 0) {
        std::cerr << "Bery: Error: linker failed.\n";
        return 13;
    }
    //remove(irFile.c_str());
    remove(objFile.c_str());
    return 0;
}

static int cmdRun(const std::string& sourcePath, const std::string& exeDir) {
    std::string binaryPath;
    int r = cmdCompile(sourcePath, binaryPath, exeDir);
    if (r != 0) return r;

    std::string runCmd = "\"" + binaryPath + "\"";
    int exitCode = system(runCmd.c_str());
    remove(binaryPath.c_str());
    return exitCode;
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string cmd = argv[1];
    std::string exeDir = getExeDir(argv[0]);
    if (cmd == "--version" || cmd == "-v") {
        std::cout << "Bery " << BERY_VERSION << "\n";
        return 0;
    }

    if (cmd == "compile") {
        std::string outPath;
        return cmdCompile(argv[2], outPath, exeDir);
    }
    if (cmd == "run") {
        return cmdRun(argv[2], exeDir);
    }
    std::cerr << "Bery: Error: unknown command '" << cmd << "'\n";
    printUsage();
    return 1;
}