
#include "parser.h"
#include "ast/programnode.h"
#include "ast/literals.h"
#include "ast/vardecl.h"
#include "ast/arraydeclare.h"
#include <stdexcept>
#include <iostream>
#include "ast/expressions.h"
#include "ast/controlflow.h"
#include "ast/functions.h"
#include "ast/blocknode.h"

Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens), current(0), errors(false) {}

std::unique_ptr<ASTNode> Parser::parse() {
    auto program = std::make_unique<ProgramNode>(); 
    while (!isAtEnd() && !check(TokenType::TOKEN_RUN)) {
        try {
            if (check(TokenType::TOKEN_FUNC)) {
                program->globals.push_back(parseFunctionDef());
                continue;
            } else if (check(TokenType::TOKEN_ENUM)) {
                program->globals.push_back(parseEnumDecl());
            } 
            else {
                bool isConst = false;
                if (check(TokenType::TOKEN_CONST)) { advance(); isConst = true; }
                if (isTypeToken(peek().type)) {
                    if (!isConst && isArrayDecl()) {
                        auto decl = parseArrayDecl();
                        program->globals.push_back(std::move(decl));
                    } else {
                        auto decls = parseVarDecl(isConst);
                        for (auto& d : decls) program->globals.push_back(std::move(d));
                    }
                } else {
                    std::cerr << "Bery:Error [Line " << peek().line << "]: Unexpected token '" << peek().lexeme << "'\n";
                    errors = true;
                    throw ParseError();
                }
            }
        } catch(ParseError& e) {
            synchronize();
        }
    }

    if (check(TokenType::TOKEN_RUN)) {
        advance();
        int runline = previous().line;
        consume(TokenType::TOKEN_LBRACE, "Expected '{' after run");
        auto runBlock = std::make_unique<RunBlockNode>(runline);
        while (!isAtEnd() && !check(TokenType::TOKEN_RBRACE)) {
            try {
                runBlock->statements.push_back(parseStatement());
            } catch(ParseError& e) {
                synchronize();
            }
        }
        consume(TokenType::TOKEN_RBRACE, "Expected '}' after run block");
        program->runBlock = std::move(runBlock);
    } else {
        std::cerr << "Bery:Error [Line " <<peek().line << "]: no run{} block found\n";
        errors = true;
    }
    return program;
}

std::vector<std::unique_ptr<ASTNode>> Parser::parseVarDecl(bool isConst) {
    std::vector<std::unique_ptr<ASTNode>> decls;
    Token typeToken = advance();
    std::string varType = typeToken.lexeme;
    do {
        Token name = consume(TokenType::TOKEN_IDENT, "Expected identifier");
        std::unique_ptr<ASTNode> value = nullptr;
        if (check(TokenType::TOKEN_EQUAL)) {
            advance();
            value = parseExpression();
        }
        decls.push_back(std::make_unique<VarDeclNode>(varType, name.lexeme, std::move(value),name.line, isConst));
    } while (!isAtEnd() && check(TokenType::TOKEN_COMMA) && (advance(), true));

    consume(TokenType::TOKEN_SEMICOLON, "Expected ';'");
    return decls;
}

std::unique_ptr<ASTNode> Parser::parseLiteral() {
    Token t = peek();

    switch(t.type) {
        case TokenType::TOKEN_INT_LIT: 
            advance();
            return std::make_unique<IntLitNode>(std::stoll(t.lexeme), t.line);
        
        case TokenType::TOKEN_DECIMAL_LIT: 
            advance();
            return std::make_unique<DecimalLitNode>(std::stod(t.lexeme), t.line);
        
        case TokenType::TOKEN_TRUE:
            advance();
            return std::make_unique<BoolLitNode>(true, t.line);
        case TokenType::TOKEN_FALSE:
            advance();
            return std::make_unique<BoolLitNode>(false, t.line);
        case TokenType::TOKEN_CHAR_LIT:
            advance();
            return std::make_unique<CharLitNode>(t.lexeme[0], t.line);
        case TokenType::TOKEN_STRING_LIT:
            advance();
            return std::make_unique<StringLitNode>(t.lexeme, t.line);
        case TokenType::TOKEN_NULL:
            advance();
            return std::make_unique<NullLitNode>(t.line);
        default:
            errors = true;
            std::cerr << "Bery:Error [Line " << t.line << "]: Expected valid expression or literal\n";
            throw ParseError();
    }
}

std::unique_ptr<ASTNode> Parser::parseExpression(){
    auto expr= parseTernary();
    auto isassignmentOperator= [](TokenType t){
        return t==TokenType::TOKEN_EQUAL || t== TokenType::TOKEN_ADD_ASSIGN || t== TokenType::TOKEN_SUB_ASSIGN 
        || t== TokenType::TOKEN_MUL_ASSIGN || t== TokenType::TOKEN_DIV_ASSIGN  || t== TokenType::TOKEN_DSTAR_ASSIGN 
        || t== TokenType::TOKEN_MODULE_ASSIGN || t== TokenType::TOKEN_AND_ASSIGN || t== TokenType::TOKEN_OR_ASSIGN 
        || t== TokenType::TOKEN_XOR_ASSIGN || t== TokenType::TOKEN_LSHIFT_ASSIGN || t== TokenType::TOKEN_RSHIFT_ASSIGN ;
    };
    if(isassignmentOperator(peek().type)) {
        Token optoken = advance();
        auto value = parseExpression();
        
        if(expr->type == NodeType::IDENT || expr->type == NodeType::INDEX_EXPR) {
            return std::make_unique<AssignmentExprNode>(std::move(expr), std::move(value), optoken.lexeme, optoken.line);
        }
        std::cerr<<"Bery:Error [Line "<< optoken.line<<"]: Invalid Assignment Target \n";
        errors= true;
        throw ParseError();
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parsePrimary(){
    Token t = peek();
    if (t.type == TokenType::TOKEN_IDENT) {
        advance();
        std::string fullName = t.lexeme;
        while (check(TokenType::TOKEN_DOT)) {
            advance();
            Token nextIdent = consume(TokenType::TOKEN_IDENT, "Expected identifier after '.'");
            fullName += "." + nextIdent.lexeme;
        }
        if (check(TokenType::TOKEN_LPARAN)) {
            Token stitchedToken = t;
            stitchedToken.lexeme = fullName;
            return parseCallExpr(stitchedToken);
        }
        if (check(TokenType::TOKEN_LBRACKET)) {
            std::vector<std::unique_ptr<ASTNode>> indices;
            while (check(TokenType::TOKEN_LBRACKET)) {
                advance();
                indices.push_back(parseExpression());
                consume(TokenType::TOKEN_RBRACKET, "Expected ']' after array index");
            }
            return std::make_unique<IndexExprNode>(fullName, std::move(indices), t.line);
        }
        return std::make_unique<IdentNode>(fullName, "", t.line);
    }
    if (t.type == TokenType::TOKEN_LPARAN) {
        advance();
        auto expression = parseExpression();
        consume(TokenType::TOKEN_RPARAN, "Expected ')'");
        return std::make_unique<GroupedExprNode>(std::move(expression), t.line);
    }
    return parseLiteral();
}



std::unique_ptr<ASTNode> Parser::parsePostfix(){
    auto expr = parsePrimary();
    if(check(TokenType::TOKEN_INC) || check(TokenType::TOKEN_DEC)){
        advance();
        std::string optr = "post"+previous().lexeme;

        return std::make_unique<UnaryExprNode>(optr, std::move(expr), previous().line);
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseUnary(){

    if (check(TokenType::TOKEN_LPARAN) &&
        current + 1 < (int)tokens.size() && isTypeToken(tokens[current + 1].type) && 
        current + 2 < (int)tokens.size() && tokens[current + 2].type == TokenType::TOKEN_RPARAN) {
            advance();
            Token typeToken = advance();
            advance();

            auto expr = parseUnary();
            return std::make_unique<CastExprNode>(typeToken.lexeme, std::move(expr), typeToken.line);
        } 

    if(check(TokenType::TOKEN_BANG) ||
        check(TokenType::TOKEN_TILDE)||
        check(TokenType::TOKEN_INC)||
        check(TokenType::TOKEN_DEC)||
        check(TokenType::TOKEN_MINUS)){
            advance();
            std::string optr = previous().lexeme;
            auto operand = parsePostfix();
            return std::make_unique<UnaryExprNode>(optr, std::move(operand), previous().line);
        }
    return parsePostfix();
}

std::unique_ptr<ASTNode> Parser::parseMultiplicative(){

    auto left = parseUnary();
    
    while(check(TokenType::TOKEN_STAR) ||
        check(TokenType::TOKEN_FSLASH)||
        check(TokenType::TOKEN_PERCENT)||
        check(TokenType::TOKEN_POWER)){
            
        std::string optr = peek().lexeme;
        advance();

        auto right = parseUnary();

        left = std::make_unique<BinaryExprNode>(
            optr,
            std::move(left),
            std::move(right),
            previous().line
        );
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseAdditive(){
    auto left = parseMultiplicative();
    
    while(check(TokenType::TOKEN_MINUS) ||
        check(TokenType::TOKEN_PLUS)){
            
        std::string optr = peek().lexeme;
        advance();

        auto right = parseMultiplicative();

        left = std::make_unique<BinaryExprNode>(
            optr,
            std::move(left),
            std::move(right),
            previous().line
        );
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseShift(){
    auto left = parseAdditive();

    while(check(TokenType::TOKEN_LSHIFT) || check(TokenType::TOKEN_RSHIFT)){

        std::string op = peek().lexeme;
        advance();

        auto right = parseAdditive();

        left = std::make_unique<BinaryExprNode>(
            op,
            std::move(left),
            std::move(right), 
            previous().line
        );
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseBitwise(){
    auto left = parseShift();

    while(check(TokenType::TOKEN_AMPERSAND) || check(TokenType::TOKEN_CARET) || check(TokenType::TOKEN_PIPE)){

        std::string op = peek().lexeme;
        advance();

        auto right = parseShift();

        left = std::make_unique<BinaryExprNode>(
            op,
            std::move(left),
            std::move(right), 
            previous().line
        );
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseBetween(){
    auto value = parseBitwise();
    if(check(TokenType::TOKEN_BETWEEN) || check(TokenType::TOKEN_NOT_BETWEEN)){
        bool isNegated = check(TokenType::TOKEN_NOT_BETWEEN);
       advance();
       int opLine = previous().line;
        auto lower = parseBitwise();
        consume(TokenType::TOKEN_COMMA, "Expected ',' after lower bound");
        auto upper = parseBitwise();
        return std::make_unique<BetweenExprNode>(
            std::move(value),
            std::move(lower),
            std::move(upper),
            isNegated, 
            opLine
        );
    }
    return value;
}


std::unique_ptr<ASTNode> Parser::parseRelational(){
    auto left =parseBetween();
   
    while(check(TokenType::TOKEN_GTHAN)|| check(TokenType::TOKEN_LTHAN)||check(TokenType::TOKEN_GTEQUAL)||check(TokenType::TOKEN_LTEQUAL)){
        std::string op= peek().lexeme;
        advance();
        auto right=parseBetween();
        left=std::make_unique<BinaryExprNode>(op,std::move(left),std::move(right), previous().line);    
    }
    return left;
}
std::unique_ptr<ASTNode> Parser::parseEquality(){
    auto left = parseRelational();
    while(check(TokenType::TOKEN_EQUAL_EQUAL)|| check(TokenType::TOKEN_NOT_EQUAL)){
        std::string op=peek().lexeme;
        advance();
        auto right=parseRelational();
        left=std::make_unique<BinaryExprNode>(op,std::move(left),std::move(right), previous().line);
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseLogicalAnd(){
    auto left = parseEquality();
    while(check(TokenType::TOKEN_AND)){
        std::string op=peek().lexeme;
        advance();
        auto right=parseEquality();
        left=std::make_unique<BinaryExprNode>(op,std::move(left),std::move(right), previous().line);
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseLogicalOr(){
    auto left = parseLogicalAnd();
    while(check(TokenType::TOKEN_OR)){
        std::string op=peek().lexeme;
        advance();
        auto right=parseLogicalAnd();
        left=std::make_unique<BinaryExprNode>(op,std::move(left),std::move(right), previous().line);
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseTernary() {
    auto expr = parseLogicalOr();
    if (check(TokenType::TOKEN_QUESTION)) {
        advance();
        int opLine = previous().line;

        auto trueExpr = parseExpression();
        consume(TokenType::TOKEN_COLON, "Expected ':' in ternary operator");
        auto falseExpr = parseTernary();

        return std::make_unique<TernaryExprNode>(std::move(expr), std::move(trueExpr), std::move(falseExpr), opLine);
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseStatement() {
    if (check(TokenType::TOKEN_IF)) {
        return parseIfStmt();
    }
    if (check(TokenType::TOKEN_ELSE)){
        advance();
        return parseBlock();
    }
    if(check(TokenType::TOKEN_WHILE)){
        return parseWhileStmt();
    }
    if(check(TokenType::TOKEN_DOWHILE)){
        return parseDoWhileStmt();
    }
    if(check(TokenType::TOKEN_FOR)){
        return parseForStmt();
    }
    bool isConst = false;
    if(check(TokenType::TOKEN_CONST)){
        advance();
        isConst = true;
    }
    if (check(TokenType::TOKEN_SWITCH)) {
        return parseSwitchStmt();
    }
    if (check(TokenType::TOKEN_BREAK)) {
        return parseBreakStmt();
    }
    if(check(TokenType::TOKEN_CONTINUE)) {
        return parseContinueStmt();
    }
    if(check(TokenType::TOKEN_PASS)) {
        return parsePassStmt();
    }
    if(check(TokenType::TOKEN_RETURN)){
        return parseReturnStmt();
    }
    if (check(TokenType::TOKEN_ENUM)) {
        return parseEnumDecl();
    }

    if(isTypeToken(peek().type)){
        if(!isConst && isArrayDecl())return parseArrayDecl();
        std::vector<std::unique_ptr<ASTNode>>decls = parseVarDecl(isConst);
        if(decls.size() == 1){
            return std::move(decls[0]);    
        };
        auto multiDeclBlock = std::make_unique<BlockNode>(previous().line);
        
        for(auto& d:decls){
            multiDeclBlock->statements.push_back(std::move(d));
        }
        return multiDeclBlock;
    }
    auto expr = parseExpression();
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after expression");
    return expr;

}

std::unique_ptr<BlockNode> Parser::parseBlock() {

    int line = previous().line;

    auto block = std::make_unique<BlockNode>(line);
    while(!isAtEnd() && !check(TokenType::TOKEN_RBRACE)){

        try
        {
            block->statements.push_back(parseStatement());
        }
        catch(ParseError& e)
        {
            synchronize();
        }
        
    }
    consume(TokenType::TOKEN_RBRACE, "Expected '}' after block");
    return block;
    
}

std::unique_ptr<ASTNode> Parser::parseIfStmt() {
    advance();
    int line = previous().line;

    consume(TokenType::TOKEN_LPARAN, "Expected '(' after if");
    auto condition = parseExpression();
    consume(TokenType::TOKEN_RPARAN, "Expected ')' after condition");
    consume(TokenType::TOKEN_LBRACE, "Expected '{' before if Body");
    auto ifBranch = parseBlock();    
    std::unique_ptr<ASTNode> elseBranch = nullptr;
    if(check(TokenType::TOKEN_ELSE)){
        advance();
        if(check(TokenType::TOKEN_IF)){
            elseBranch = parseIfStmt();
        }else{
            consume(TokenType::TOKEN_LBRACE, "Expected '{' before if else Body");
            elseBranch = parseBlock();
        }
    }

    return std::make_unique<IfStmtNode>(
        std::move(condition),
        std::move(ifBranch),
        std::move(elseBranch),
        line
); 

}

std::unique_ptr<ASTNode> Parser::parseSwitchStmt() {
    advance();
    int line = previous().line;

    consume(TokenType::TOKEN_LPARAN, "Expected '(' after switch");
    auto expr = parseExpression();
    consume(TokenType::TOKEN_RPARAN, "Expected ')' after condition");

    consume(TokenType::TOKEN_LBRACE, "Expected '{' before switch-body");

    auto sw = std::make_unique<SwitchStmtNode>(line);
    sw->condition = std::move(expr);

    while (!isAtEnd() && !check(TokenType::TOKEN_RBRACE)) {

        if (check(TokenType::TOKEN_CASE)) {
            advance();
            CaseBlock cb;
            cb.value = parseExpression();

            consume(TokenType::TOKEN_COLON, "expected ':' after case");

            while (!isAtEnd() && !check(TokenType::TOKEN_CASE) &&
                   !check(TokenType::TOKEN_DEFAULT) && !check(TokenType::TOKEN_RBRACE)) {
                cb.statements.push_back(parseStatement());
            }
            sw->cases.push_back(std::move(cb));
        }
        else if (check(TokenType::TOKEN_DEFAULT)) {
            advance();
            consume(TokenType::TOKEN_COLON, "Expected ':'");
            sw->hasDefault = true;

            while (!isAtEnd() && !check(TokenType::TOKEN_CASE) && !check(TokenType::TOKEN_RBRACE)) {
                sw->defaultBlock.push_back(parseStatement());
            }
        }
        else {
            std::cerr << "Bery:Error [Line " << peek().line << "]: Expected 'case' or 'default'\n";
            errors = true;
            advance();
        }
    }
    consume(TokenType::TOKEN_RBRACE, "Expected '}' after switch body");
    return sw;
}

std::unique_ptr<ASTNode> Parser::parseBreakStmt() {
    int line = previous().line;
    advance();
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after 'break'");
    return std::make_unique<BreakStmtNode>(line);
}

std::unique_ptr<ASTNode> Parser::parseContinueStmt() {
    int line = previous().line;
    advance();
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after 'continue'");
    return std::make_unique<ContinueStmtNode>(line);
}

std::unique_ptr<ASTNode> Parser::parsePassStmt() {
    int line = previous().line;
    advance();
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after 'pass'");
    return std::make_unique<PassStmtNode>(line);
}

std::unique_ptr<ASTNode> Parser::parseWhileStmt(){
    advance();
    int line = previous().line;
    consume(TokenType::TOKEN_LPARAN,"Expected '(' after 'while'");
    auto condition = parseExpression();
    consume(TokenType::TOKEN_RPARAN, "Expected ')' after condition");
    consume(TokenType::TOKEN_LBRACE, "Expected '{' before while-body");
    auto body = parseBlock();
    return std::make_unique<WhileStmtNode>(std::move(condition), std::move(body), line);
}
std::unique_ptr<ASTNode> Parser::parseDoWhileStmt(){
    advance();
    int line = previous().line;
    consume(TokenType::TOKEN_LBRACE, "Expected '{' before do-while-body");
    auto body = parseBlock();
    consume(TokenType::TOKEN_WHILE, "Expected 'while' condition after do-while-body");
    consume(TokenType::TOKEN_LPARAN,"Expected '(' after 'while'");
    auto condition = parseExpression();
    consume(TokenType::TOKEN_RPARAN, "Expected ')' after condition");
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after condition-end");
    return std::make_unique<DoWhileStmtNode>(std::move(condition), std::move(body), line);
}

std::unique_ptr<ASTNode> Parser::parseForStmt() {
    advance();
    int line = previous().line;
    consume(TokenType::TOKEN_LPARAN, "Expected '(' after 'for'");

    bool isForIn = false;
    bool hasExplicitType = false;

    if (peek().type == TokenType::TOKEN_IDENT && current + 1 < tokens.size() && tokens[current + 1].type == TokenType::TOKEN_IN) {
        isForIn = true;
    } 
    else if (isTypeToken(peek().type) && current + 1 < tokens.size() && tokens[current + 1].type == TokenType::TOKEN_IDENT && current + 2 < tokens.size() && tokens[current + 2].type == TokenType::TOKEN_IN) {
        isForIn = true;
        hasExplicitType = true;
    }

    if (isForIn) {
        std::string varType = "unknown";
        if (hasExplicitType) {
            varType = advance().lexeme;
        }
        
        Token varTok = consume(TokenType::TOKEN_IDENT, "Expected identifier");
        advance();

        auto iterableOrStart = parseExpression();
        std::unique_ptr<ASTNode> rangeEnd = nullptr;
        std::unique_ptr<ASTNode> step = nullptr;

        if (check(TokenType::TOKEN_RANGE)) {
            advance();
            rangeEnd = parseExpression();
        }

        if (check(TokenType::TOKEN_COMMA)) {
            advance();
            step = parseExpression();
        }

        consume(TokenType::TOKEN_RPARAN, "Expected ')' after for-in declaration");
        consume(TokenType::TOKEN_LBRACE, "Expected '{' before loop body");
        auto body = parseBlock();

        return std::make_unique<ForInNode>(varType, varTok.lexeme, std::move(iterableOrStart), std::move(rangeEnd), std::move(step), std::move(body), line);
    } 
    else {
        std::unique_ptr<ASTNode> init = nullptr;
        
        if (!check(TokenType::TOKEN_SEMICOLON)) {
            if (isTypeToken(peek().type)) {
                auto decls = parseVarDecl(false);
                
                if (decls.size() == 1) {
                    init = std::move(decls[0]);
                } else {
                    auto initBlock = std::make_unique<BlockNode>(line);
                    for (auto& d : decls) initBlock->statements.push_back(std::move(d));
                    init = std::move(initBlock);
                }
            } else {
                init = parseExpression(); 
                consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after loop initialization");
            }
        } else {
            consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after loop initialization");
        }

        std::unique_ptr<ASTNode> cond = nullptr;
        if (!check(TokenType::TOKEN_SEMICOLON)) cond = parseExpression();
        consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after loop condition");

        std::unique_ptr<ASTNode> update = nullptr;
        if (!check(TokenType::TOKEN_RPARAN)) update = parseExpression();
        consume(TokenType::TOKEN_RPARAN, "Expected ')' after loop update");

        consume(TokenType::TOKEN_LBRACE, "Expected '{' before loop body");
        auto body = parseBlock();

        return std::make_unique<ForStmtNode>(std::move(init), std::move(cond), std::move(update), std::move(body), line);
    }
}

Token Parser::advance() {if (!isAtEnd()) current++;
    return previous();}

Token Parser::peek() {
    if (current >= (int)tokens.size()) return tokens.back(); 
    return tokens[current];}

Token Parser::previous() {return tokens[current - 1];}

bool Parser::isAtEnd() {return peek().type == TokenType::TOKEN_EOF;}

bool Parser::check(TokenType type) { return peek().type == type;}

Token Parser::consume(TokenType type, const std::string& msg) {
    if (check(type)) return advance();
    errors = true;
    std::cerr<< "Bery:Error [Line " << peek().line << "]: " << msg << "\n";
    throw ParseError();
}

bool Parser::isTypeToken(TokenType t) {
    return t == TokenType::TOKEN_INT ||
            t == TokenType::TOKEN_BIGINT ||
           t == TokenType::TOKEN_BOOL ||
           t == TokenType::TOKEN_FLOAT ||
           t == TokenType::TOKEN_DOUBLE ||
           t == TokenType::TOKEN_CHAR ||
           t == TokenType::TOKEN_STRING; 
}

bool Parser::isArrayDecl() {
    if (isTypeToken(peek().type)) {
        if (current + 2 < tokens.size() &&
            tokens[current + 1].type == TokenType::TOKEN_IDENT &&
            tokens[current + 2].type == TokenType::TOKEN_LBRACKET) {
            return true;
        }
    }
    return false;
}

std::unique_ptr<ASTNode> Parser::parseArrayDecl() {
    Token typeToken = consume(peek().type, "Expected type");
    std::string elementType = typeToken.lexeme;
    Token nameToken = consume(TokenType::TOKEN_IDENT, "Expected identifier");
    std::string name = nameToken.lexeme;

    std::vector<int> dimensions;
    while (check(TokenType::TOKEN_LBRACKET)) {
        advance();
        if (check(TokenType::TOKEN_INT_LIT)) {
            Token sizeToken = advance();
            dimensions.push_back(std::stoi(sizeToken.lexeme));
        } else {
            dimensions.push_back(-1); 
        }
        consume(TokenType::TOKEN_RBRACKET, "Expected ']'");
    }

    std::vector<std::unique_ptr<ASTNode>> initializers;
    if (check(TokenType::TOKEN_EQUAL)) {
        advance(); 
        if (!check(TokenType::TOKEN_LBRACE)) {
            std::cerr << "Bery:Error: [Line " << peek().line << "]: Arrays must be initialized with list inside '{}'\n";
            errors = true;
            while (!isAtEnd() && !check(TokenType::TOKEN_SEMICOLON)) advance();
            return std::make_unique<ArrayDeclNode>(elementType, name, dimensions, std::move(initializers), nameToken.line);
        }
        parseArrayInitializer(initializers);
    }
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';'");
    return std::make_unique<ArrayDeclNode>(elementType, name, dimensions, std::move(initializers), nameToken.line);
}
void Parser::parseArrayInitializer(std::vector<std::unique_ptr<ASTNode>>& initializers) {
    consume(TokenType::TOKEN_LBRACE, "Expected '{'");
    if (!check(TokenType::TOKEN_RBRACE)) {
        do {
            if (check(TokenType::TOKEN_LBRACE)) {
                parseArrayInitializer(initializers);
            } else {
                initializers.push_back(parseExpression());
            }
        } while (!isAtEnd() && check(TokenType::TOKEN_COMMA) && (advance(), true));
    }
    consume(TokenType::TOKEN_RBRACE, "Expected '}'");
}

std::unique_ptr<ASTNode> Parser::parseFunctionDef() {
    advance(); 
    int line = previous().line;
    Token nameToken = consume(TokenType::TOKEN_IDENT, "Expected function name");
    consume(TokenType::TOKEN_LPARAN, "Expected '(' after function name");

    std::vector<std::pair<std::string, std::string>> params;
    if (!check(TokenType::TOKEN_RPARAN)) {
        do {
            Token typeTok = advance();
            Token nameTok = consume(TokenType::TOKEN_IDENT, "Expected parameter name");
            params.push_back({typeTok.lexeme, nameTok.lexeme});
        } while (!isAtEnd() && check(TokenType::TOKEN_COMMA) && (advance(), true));
    }
    consume(TokenType::TOKEN_RPARAN, "Expected ')' after parameters");

    std::string returnType = "void"; 
    if (check(TokenType::TOKEN_ARROW)) {
        advance();
        returnType = advance().lexeme;
    }

    consume(TokenType::TOKEN_LBRACE, "Expected '{' before function body");
    auto body = parseBlock();

    return std::make_unique<FunctionDefNode>(nameToken.lexeme, std::move(params), returnType, std::move(body), line);
}

std::unique_ptr<ASTNode> Parser::parseCallExpr(const Token& identifierToken) {
    consume(TokenType::TOKEN_LPARAN, "Expected '('");
    
    std::vector<std::unique_ptr<ASTNode>> arguments;
    if (!check(TokenType::TOKEN_RPARAN)) {
        do {
            arguments.push_back(parseExpression());
        } while (!isAtEnd() && check(TokenType::TOKEN_COMMA) && (advance(), true));
    }
    consume(TokenType::TOKEN_RPARAN, "Expected ')' after arguments");
    
    return std::make_unique<CallExprNode>(identifierToken.lexeme, std::move(arguments), identifierToken.line);
}

std::unique_ptr<ASTNode> Parser::parseReturnStmt() {
    advance(); 
    int line = previous().line;
    std::unique_ptr<ASTNode> value = nullptr;
    if (!check(TokenType::TOKEN_SEMICOLON)) {
        value = parseExpression();
    }
    
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after return statement");
    return std::make_unique<ReturnStmtNode>(std::move(value), line);
}

bool Parser::hasErrors() {return errors;}

void Parser::synchronize() {
    advance();
    while (!isAtEnd()) {
        if (previous().type == TokenType::TOKEN_SEMICOLON) return;
        switch (peek().type) {
            case TokenType::TOKEN_INT:
            case TokenType::TOKEN_FLOAT:
            case TokenType::TOKEN_BIGINT:
            case TokenType::TOKEN_DOUBLE:
            case TokenType::TOKEN_STRING:
            case TokenType::TOKEN_BOOL:
            case TokenType::TOKEN_CHAR:
            case TokenType::TOKEN_CONST:
            case TokenType::TOKEN_RUN:
                return;
            default:
                break; 
        }
        advance();
    }
}

std::unique_ptr<ASTNode> Parser::parseEnumDecl() {
    advance();
    int line = previous().line;
    
    Token nameTok = consume(TokenType::TOKEN_IDENT, "Expected enum name");
    consume(TokenType::TOKEN_EQUAL, "Expected '=' after enum name");
    consume(TokenType::TOKEN_LBRACE, "Expected '{' to start enum values");

    std::vector<std::string> values;
    if (!check(TokenType::TOKEN_RBRACE)) {
        do {
            Token valTok = consume(TokenType::TOKEN_IDENT, "Expected enum value");
            values.push_back(valTok.lexeme);
        } while (!isAtEnd() && check(TokenType::TOKEN_COMMA) && (advance(), true));
    }
    
    consume(TokenType::TOKEN_RBRACE, "Expected '}' after enum values");
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after enum declaration");

    return std::make_unique<EnumDeclNode>(nameTok.lexeme, std::move(values), line);
}