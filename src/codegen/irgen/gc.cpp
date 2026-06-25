#include "../codegen.h"
#include <stack>

void CodeGen::emitGCPush(const std::string& allocaReg, const std::string& lt, std::ostream& out) {
    emitBREDecl("declare void @bery_gc_push_root(i8**)", "bery_gc_push_root");
    std::string castReg = newReg();
    out << "    " << castReg << " = bitcast " << lt << "* " << allocaReg << " to i8**\n";
    out << "    call void @bery_gc_push_root(i8** " << castReg << ")\n";
    gcRootCounter++;
    gcRootScopeStack.top()++;
}

void CodeGen::emitGCPops(int count, std::ostream& out) {
    emitBREDecl("declare void @bery_gc_pop_root()", "bery_gc_pop_root");
    for (int i = 0; i < count; i++) {
        out << "    call void @bery_gc_pop_root()\n";
    }
}

void CodeGen::pushGCScope() {
    gcRootScopeStack.push(0);
}

int CodeGen::popGCScope() {
    if (gcRootScopeStack.empty()) return 0;
    int count = gcRootScopeStack.top();
    gcRootScopeStack.pop();
    return count;
}