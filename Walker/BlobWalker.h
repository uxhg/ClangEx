//
// Created by bmuscede on 06/12/16.
//

#ifndef CLANGEX_MINIMALWALKER_H
#define CLANGEX_MINIMALWALKER_H

#include "ASTWalker.h"

class MinimalWalker : public ASTWalker {
public:
    MinimalWalker(ClangArgParse::ClangExclude exclusions = ClangArgParse::ClangExclude(), TAGraph* graph = new TAGraph());
    virtual ~MinimalWalker();

    virtual void run(const MatchFinder::MatchResult &result);
    virtual void generateASTMatches(MatchFinder *finder);

private:
    enum {FUNC_DEC = 0, VAR_DEC, FIELD_DEC, FUNC_CALLER, FUNC_CALLEE, VAR_CALLER, VAR_CALLEE, VAR_EXPR,
        FIELD_CALLEE, FIELD_EXPR, CLASS_DEC, ENUM_DEC};
    const char* types[12] = {"func_dec", "var_dec", "field_dec", "caller", "callee", "v_caller", "v_callee", "v_expr",
                            "field_callee", "field_expr", "class_dec", "enum_dec"};

    void addFunctionDec(const MatchFinder::MatchResult results, const clang::DeclaratorDecl *dec);
    void addVariableDec(const MatchFinder::MatchResult results, const clang::VarDecl *dec);
    void addVariableDec(const MatchFinder::MatchResult results, const clang::FieldDecl *dec);
    void addFunctionCall(const MatchFinder::MatchResult results, const clang::DeclaratorDecl *caller, const clang::FunctionDecl *callee);
    void addVariableCall(const MatchFinder::MatchResult result, const clang::VarDecl *callee, const clang::DeclaratorDecl *caller,
                        const clang::Expr* expr);
    void addVariableCall(const MatchFinder::MatchResult result, const clang::FieldDecl *callee, const clang::DeclaratorDecl *caller,
                         const clang::Expr* expr);
    void addVariableCall(const MatchFinder::MatchResult result, std::string calleeName,
                         std::string callerLabel, std::string calleeLabel, const clang::Expr* expr);
    void addClassDec(const MatchFinder::MatchResult result, const clang::CXXRecordDecl *classRec);
    void addClassInheritanceRef(const clang::CXXRecordDecl *childClass, const clang::CXXRecordDecl *parentClass);
    void addEnumDec(const MatchFinder::MatchResult result, const clang::EnumDecl *dec);
};


#endif //CLANGEX_MINIMALWALKER_H
