#pragma once

/*
    
    Bery Compiler Toolchain,

    It finds the compiler tools on the machine and builds the shell commands to compile and link a Bery program.

*/

#include "platform.h"
#include <string>
#include <cstdlib>

// Holds  the detected toolchain info needed to compile and link the Bery program inside machine.
struct BeryToolChain {
    // LLVM compiler which is used for converting .ll IR file into the
    // object file via LLVM backend.
    std::string llc;

    // Linker is tool which links objects and BRE (Bery Runtime Environment) into final binary
    // which could be 'g++' or 'clang++'
    std::string linker;


    // As extensions of files of objects and binary changes across different
    // Operating Systems we need to detect and store it.
    // which are .o / .obj 
    std::string objectExt;

    // binary file may have no exentension, or have '.exe'
    std::string binaryExt;

    // flag that toolchain is detected and built. false if detection failed that's why
    // caller of these methods should abort the compilation
    bool valid;
};


// Detection of commands exists on the system PATH
// this uses 'where' command to detect the commands in WINDOWS system.
// 'which' on Unix systems.
// It returns true if the command is found, otherwise false.
inline bool commandExists(const std::string& cmd) {
#ifdef BERY_WINDOWS
    std::string check = "where " + cmd + " >nul 2>&1";
#else
    std::string check = "which " + cmd + " >/dev/null 2>&1";
#endif
    return system(check.c_str()) == 0;
}

// Main detector. Detects the available LLVM tools on current machine
// Tries llc versions from latest (unversioned) down to llc-18
// Returns a BeryToolChain with valid=false if llc or a linker is not found
inline BeryToolChain detectToolchain() {
    BeryToolChain tc;
    tc.valid = false;

    // Try different llc versions in order, 
    // it stops as any one fount.
    if      (commandExists("llc"))    tc.llc = "llc";
    else if (commandExists("llc-22")) tc.llc = "llc-22";
    else if (commandExists("llc-20")) tc.llc = "llc-20";
    else if (commandExists("llc-19")) tc.llc = "llc-19";
    else if (commandExists("llc-18")) tc.llc = "llc-18";
    else {
        // no llc found, it cannot be compiled
        tc.llc = "";
        return tc;
    }

#ifdef BERY_WINDOWS
    // Windows prefers clang++ as a linker
    // if not found then it checks for g++
    tc.objectExt = ".obj";
    tc.binaryExt = ".exe";

    //WINDOWS: clang++ is preferred since it handles MSVC ABI better than minGW g++
    if      (commandExists("clang++")) tc.linker = "clang++";
    else if (commandExists("g++"))     tc.linker = "g++";
    else return tc;
#elif defined(BERY_MACOS)
    // MACOS prefers clang++ as a linker
    // if not found it finds g++
    tc.objectExt = ".o";
    // no extension for binary on linux/macos 

    // clang++ is a native toolchain in macos. g++ if it is not present.
    tc.binaryExt = ""; 
    if      (commandExists("clang++")) tc.linker = "clang++";
    else if (commandExists("g++"))     tc.linker = "g++";
    else return tc;
#elif defined(BERY_LINUX)
    tc.objectExt = ".o";
    // no file exentions for binaries in linux
    tc.binaryExt = ""; 
    // g++ is preffered since it's more commonly available.
    if      (commandExists("g++"))     tc.linker = "g++";
    else if (commandExists("clang++")) tc.linker = "clang++";
    else return tc;
#endif
    tc.valid = true;
    return tc;
}

// Comamnd building,
// Builds the llc commands that compile .ll IR file into a native object file.
// -mtriple flag tells llc the exact target architecture, llc can guess it wrong (it's default value) to convert it into binary
// on cross-compile setups or multi-target LLVM ubuilds
inline std::string buildCompileCmd(const BeryToolChain& tc,const std::string& irFile, const std::string& objFile) {
#ifdef BERY_WINDOWS
    return tc.llc + " -filetype=obj -mtriple=x86_64-pc-windows-msvc \""+ irFile + "\" -o \"" + objFile + "\"";
#elif defined(BERY_MACOS)
    return tc.llc + " -filetype=obj -mtriple=x86_64-apple-darwin \""+ irFile + "\" -o \"" + objFile + "\"";
#elif defined(BERY_LINUX)
    return tc.llc + " -filetype=obj -mtriple=x86_64-pc-linux-gnu \"" + irFile + "\" -o \"" + objFile + "\"";
#else
    return tc.llc + " -filetype=obj \"" + irFile + "\" -o \"" + objFile + "\"";
#endif
}

// Linker command,
// BUilds the linker command that links the object file and BRE runtime library

// brelib is a full path like "/<path>/libbre.a". that's why we need to split it into:
//      -L"/<path>/" (directory containing the lib)
//      -lbre  (lib name without "lib" prefix and ".a" extension)
// becuase -l<name> is  how unix linkers resolves library names via -L paths.
inline std::string buildLinkCmd(const BeryToolChain& tc, const std::string& objFile, const std::string& breLib, const std::string& outBinary) {
    // split brelib path into directory and filename
    // directory = "/path/"
    // filename = "libbre.a"
    size_t slash = breLib.find_last_of("/\\");
    std::string libDir  = (slash == std::string::npos) ? "." : breLib.substr(0, slash);
    std::string libFile = (slash == std::string::npos) ? breLib : breLib.substr(slash + 1);

    // stripping the 'lib' prefix -> becomes "bre.a"
    std::string libName = libFile;
    if (libName.size() > 3 && libName.substr(0, 3) == "lib") libName = libName.substr(3);
    // stripping the file extension  -> becomes "bre"
    size_t dot = libName.find_last_of('.');
    if (dot != std::string::npos) libName = libName.substr(0, dot);
    return tc.linker + " \"" + objFile + "\""+ " -L\"" + libDir + "\"" + " -l" + libName+ " -o \"" + outBinary + "\"";
}