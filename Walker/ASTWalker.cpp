//
// Created by bmuscede on 05/11/16.
//

//TODO:
// - Create attribute structs in Node and Edge classes (Improve with functions in struct).
// - Get unions and structs working
// - Verify correctness of union, struct, and enums.

#include <fstream>
#include <iostream>
#include <boost/filesystem.hpp>
#include "ASTWalker.h"

using namespace std;
using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;
using namespace llvm;

ASTWalker::ASTWalker(){
    //Sets the current file name to blank.
    curFileName = "";

    //Creates the graph system.
    graph = new TAGraph();
}

ASTWalker::~ASTWalker() {
    delete graph;
}

void ASTWalker::buildGraph(string fileName) {
    //First, runs the graph builder process.
    string tupleAttribute = graph->generateTAFormat();

    //Next, writes to disk.
    ofstream taFile;
    taFile.open(fileName.c_str());

    //Check if the file is opened.
    if (!taFile.is_open()){
        cout << "The TA file could not be written to " << fileName << "!" << endl;
        return;
    }

    taFile << tupleAttribute;
    taFile.close();

    cout << "TA file successfully written to " << fileName << "!" << endl;
}

void ASTWalker::resolveExternalReferences() {
    graph->resolveExternalReferences(false);
}

void ASTWalker::resolveFiles(){
    vector<ClangNode*> fileNodes = vector<ClangNode*>();
    vector<ClangEdge*> fileEdges = vector<ClangEdge*>();

    //Gets all the associated clang nodes.
    fileParser.processPaths(fileNodes, fileEdges);

    //Adds them to the graph.
    for (ClangNode* file : fileNodes){
        if (!(exclusions.cSubSystem && file->getType() == ClangNode::NodeType::SUBSYSTEM)){
            graph->addNode(file);
        }
    }

    //Adds the edges to the graph.
    for (ClangEdge* edge : fileEdges){
        if (!(exclusions.cSubSystem && edge->getType() == ClangEdge::EdgeType::CONTAINS)){
            graph->addEdge(edge);
        }
    }

    //Next, for each item in the graph, add it to a file.
    graph->addNodesToFile();
}

void ASTWalker::setGraph(TAGraph* graph){
    //Deletes the current graph.
    delete this->graph;

    //Sets the new graph.
    this->graph = graph;
}

string ASTWalker::generateID(string fileName, string signature, ClangNode::NodeType type) {
    //Check the signature for invalid characters.
    replace(signature.begin(), signature.end(), '=', 'e');

    if (type == ClangNode::CLASS){
        return fileName + "[" + CLASS_PREPEND + signature + "]";
    }

    return fileName + "[" + signature + "]";
}

string ASTWalker::generateID(const MatchFinder::MatchResult result, const DeclaratorDecl *decl, ClangNode::NodeType type){
    //Gets the file name from the source manager.
    string fileName = generateFileName(result, decl->getInnerLocStart());
    if (fileName.compare("") == 0) return string();

    //Gets the qualified name.
    string name = decl->getNameAsString();

    return generateID(fileName, name, type);
}

string ASTWalker::generateFileName(const MatchFinder::MatchResult result, SourceLocation loc){
    //Gets the file name.
    string newPath = generateFileNameQuietly(result, loc);
    if (newPath.compare("") == 0) return string();

    //Print file name.
    printFileName(newPath);

    return newPath;
}

string ASTWalker::generateFileNameQuietly(const MatchFinder::MatchResult result, SourceLocation loc){
    //Gets the file name.
    SourceManager& SrcMgr = result.Context->getSourceManager();
    const FileEntry* Entry = SrcMgr.getFileEntryForID(SrcMgr.getFileID(loc));

    if (Entry == NULL) return string();

    string fileName(Entry->getName());

    //Use boost to get the absolute path.
    boost::filesystem::path fN = boost::filesystem::path(fileName);
    string newPath = fN.normalize().string();

    //Adds the file path.
    fileParser.addPath(newPath);

    return newPath;
}

string ASTWalker::generateLabel(const Decl* decl, ClangNode::NodeType type){
    string label;

    //Get qualified name.
    if (type == ClangNode::FUNCTION){
        label = decl->getAsFunction()->getQualifiedNameAsString();
    } else if (type == ClangNode::VARIABLE){
        const VarDecl* var = static_cast<const VarDecl*>(decl);
        label = var->getQualifiedNameAsString();

        //We need to get the parent function.
        const DeclContext* parentContext = var->getParentFunctionOrMethod();

        //If we have NULL, get the parent function.
        if (parentContext != NULL){
            string parentQualName = static_cast<const FunctionDecl*>(parentContext)->getQualifiedNameAsString();
            label = parentQualName + "::" + label;
        }
    } else if (type == ClangNode::CLASS){
        const CXXRecordDecl* record = static_cast<const CXXRecordDecl*>(decl);
        label = record->getQualifiedNameAsString();
    } else if (type == ClangNode::ENUM){
        const EnumDecl* enumDec = static_cast<const EnumDecl*>(decl);
        label = enumDec->getQualifiedNameAsString();
    }

    //Check for valid anonymous items in the label.
    if (label.find(ANON_STRUCT) != string::npos) {
        label = replaceLabel(label, ANON_STRUCT, ANON_STRUCT_SYM);
    }
    if (label.find(UNION_STRUCT) != string::npos) {
        label = replaceLabel(label, UNION_STRUCT, UNION_STRUCT_SYM);
    }
    if (label.find(ANON) != string::npos){
        label = replaceLabel(label, ANON, ANON_SYM);
    }

    //Check for =.
    replace(label.begin(), label.end(), '=', 'e');

    return label;
}

string ASTWalker::getClassNameFromQualifier(string qualifiedName){
    string cpyQual = qualifiedName;
    string qualifier = "::";
    vector<string> quals = vector<string>();

    size_t pos = 0;
    while ((pos = cpyQual.find(qualifier)) != std::string::npos) {
        quals.push_back(cpyQual.substr(0, pos));
        cpyQual.erase(0, pos + qualifier.length());
    }
    quals.push_back(cpyQual);

    //Check if we have a class.
    if (quals.size() == 1) return string();

    return quals.at(quals.size() - 2);
}

bool ASTWalker::isInSystemHeader(const MatchFinder::MatchResult &result, const clang::Decl *decl){
    if (decl == NULL) return false;
    bool isIn;

    //Some system headers cause Clang to segfault.
    try {
        //Gets where this item is located.
        auto &SourceManager = result.Context->getSourceManager();
        auto ExpansionLoc = SourceManager.getExpansionLoc(decl->getLocStart());

        //Checks if we have an invalid location.
        if (ExpansionLoc.isInvalid()) {
            return false;
        }

        //Now, checks if we don't have a system header.
        isIn = SourceManager.isInSystemHeader(ExpansionLoc);
    } catch ( ... ){
        return false;
    }

    return isIn;
}

void ASTWalker::printFileName(std::string curFile){
    if (curFile.compare(curFileName) != 0){
        cout << "\tCurrently processing: " << curFile << endl;
        curFileName = curFile;
    }
}

std::string ASTWalker::replaceLabel(std::string label, std::string init, std::string aft){
    size_t index = 0;
    while (true) {
        //Locate the substring to replace.
        index = label.find(init, index);
        if (index == string::npos) break;

        //Make the replacement.
        label.replace(index, init.size(), aft);

        //Advance index forward so the next iteration doesn't pick it up as well.
        index += aft.size();
    }

    return label;
}