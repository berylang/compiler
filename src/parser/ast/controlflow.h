#pragma once
#include "node.h"
#include "blocknode.h"
#include <vector>
#include <memory>

struct IfStmtNode : public ASTNode
{
    std::unique_ptr<ASTNode> conditions;
    std::unique_ptr<BlockNode> ifBranch;
    std::unique_ptr<ASTNode> elseBranch;

    IfStmtNode(
        std::unique_ptr<ASTNode> c,
        std::unique_ptr<BlockNode> i,
        std::unique_ptr<ASTNode> e,
        int ln
    )
        : conditions(std::move(c)),
          ifBranch(std::move(i)),
          elseBranch(std::move(e))

    {
        type = NodeType::IF_STMT;
        line = ln;
    }
};

struct CaseBlock
{
    std::unique_ptr<ASTNode> value;
    std::vector<std::unique_ptr<ASTNode>> statements;
};

struct SwitchStmtNode : public ASTNode
{
    std::unique_ptr<ASTNode> condition;
    std::vector<CaseBlock> cases;

    std::vector<std::unique_ptr<ASTNode>> defaultBlock;
    bool hasDefault;

    SwitchStmtNode(int ln)
    {
        type = NodeType::SWITCH_STMT;
        line = ln;
        hasDefault = false;
    }
};

struct BreakStmtNode : public ASTNode
{
    BreakStmtNode(int ln)
    {
        type = NodeType::BREAK_STMT;
        line = ln;
    }
};

struct ContinueStmtNode : public ASTNode{
    ContinueStmtNode(int ln)
    {
        type = NodeType::CONTINUE_STMT;
        line = ln;
    }
};

struct PassStmtNode : public ASTNode {
    PassStmtNode(int ln){
        type = NodeType::PASS_STMT;
        line = ln;
    }
};

struct WhileStmtNode : public ASTNode{
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<BlockNode> body;

    WhileStmtNode(
        std::unique_ptr<ASTNode> c,
        std::unique_ptr<BlockNode> b,
        int ln
    ):    condition(std::move(c)),
          body(std::move(b))
    {
        type = NodeType::WHILE_STMT;
        line = ln;

    }
          
};

struct DoWhileStmtNode : public ASTNode{
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<BlockNode> body;

    DoWhileStmtNode(
        std::unique_ptr<ASTNode> c,
        std::unique_ptr<BlockNode> b,
        int ln
    ):    condition(std::move(c)),
          body(std::move(b))
    {
        type = NodeType::DOWHILE_STMT;
        line = ln;

    }
};

struct ForStmtNode : public ASTNode {
    std::unique_ptr<ASTNode> init;
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> update;
    std::unique_ptr<BlockNode> body;

    ForStmtNode(
        std::unique_ptr<ASTNode> i,
        std::unique_ptr<ASTNode> c,
        std::unique_ptr<ASTNode> inc,
        std::unique_ptr<BlockNode> b,
        int ln
    ) : init(std::move(i)),
        condition(std::move(c)),
        update(std::move(inc)),
        body(std::move(b))
    {
        type = NodeType::FOR_STMT;
        line = ln;
    }
};

struct ForInNode : public ASTNode {
    std::string varName;
    std::string varType;
    std::unique_ptr<ASTNode> iterableOrStart;
    std::unique_ptr<ASTNode> rangeEnd;
    std::unique_ptr<ASTNode> step;
    std::unique_ptr<BlockNode> body;

    ForInNode(std::string t, std::string var, std::unique_ptr<ASTNode> iterStart, std::unique_ptr<ASTNode> end, 
        std::unique_ptr<ASTNode> stp, std::unique_ptr<BlockNode> b, int ln)
        : varType(t), varName(var), iterableOrStart(std::move(iterStart)), 
        rangeEnd(std::move(end)), step(std::move(stp)), body(std::move(b)) {
        type = NodeType::FOR_IN_STMT;
        line = ln;
    }
};