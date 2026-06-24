#pragma once
#include "platform.h"
#include <string>
#include <cstdlib>

struct BeryToolChain {
    std::string llc;
    std::string linker;
    std::string obectExt;
    std::string binaryExt;
    bool valid;
};

inline bool commandExists(const std::string& cmd) {
#ifdef BERY_WINDOWS
    std::string check = "where " + cmd + " >nul 2>&1";
#else
    std::string check = "which " + cmd + " >/dev/null 2>&1";
#endif
    return system(check.c_str()) == 0;
}

inline BeryToolChain detectToolchain() {
    BeryToolChain tc;
    tc.valid = false;

    if      (commandExists("llc"))    tc.llc = "llc";
    else if (commandExists("llc-22")) tc.llc = "llc-22";
    else if (commandExists("llc-20")) tc.llc = "llc-20";
    else if (commandExists("llc-19")) tc.llc = "llc-19";
    else if (commandExists("llc-18")) tc.llc = "llc-18";
    else {
        tc.llc = "";
        return tc;
    }

#ifdef BERY_WINDOWS
    tc.objectExt = ".obj";
    tc.binaryExt = ".exe";
    if      (commandExists("clang++")) tc.linker = "clang++";
    else if (commandExists("g++"))     tc.linker = "g++";
    else return tc;
#elif defined(BERY_MACOS)
    tc.objectExt = ".o";
    tc.binaryExt = "";
    if      (commandExists("clang++")) tc.linker = "clang++";
    else if (commandExists("g++"))     tc.linker = "g++";
    else return tc;
#elif defined(BERY_LINUX)
    tc.obectExt = ".o";
    tc.binaryExt = "";
    if      (commandExists("g++"))     tc.linker = "g++";
    else if (commandExists("clang++")) tc.linker = "clang++";
    else return tc;
#endif

    tc.valid = true;
    return tc;
}

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

inline std::string buildLinkCmd(const BeryToolChain& tc, const std::string& objFile, const std::string& breLib, const std::string& outBinary) {
    size_t slash = breLib.find_last_of("/\\");
    std::string libDir  = (slash == std::string::npos) ? "." : breLib.substr(0, slash);
    std::string libFile = (slash == std::string::npos) ? breLib : breLib.substr(slash + 1);
    std::string libName = libFile;
    if (libName.size() > 3 && libName.substr(0, 3) == "lib") libName = libName.substr(3);
    size_t dot = libName.find_last_of('.');
    if (dot != std::string::npos) libName = libName.substr(0, dot);

    return tc.linker + " \"" + objFile + "\""+ " -L\"" + libDir + "\"" + " -l" + libName+ " -o \"" + outBinary + "\"";
}