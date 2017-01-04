//
// Created by bmuscede on 22/12/16.
//

#ifndef CLANGEX_TAPROCESSOR_H
#define CLANGEX_TAPROCESSOR_H

#include <string>
#include "../Graph/TAGraph.h"

class TAProcessor {
public:
    TAProcessor(std::string entityRelName);
    ~TAProcessor();

    bool readTAFile(std::string fileName);
    bool writeTAFile(std::string fileName);

    bool readTAGraph(TAGraph graph);
    TAGraph* writeTAGraph();

private:
    const std::string COMMENT_PREFIX = "//";
    const std::string COMMENT_BLOCK_START = "/*";
    const std::string COMMENT_BLOCK_END = "*/";

    const std::string RELATION_FLAG = "FACT TUPLE";
    const std::string ATTRIBUTE_FLAG = "FACT ATTRIBUTE";
    const std::string SCHEME_FLAG = "SCHEME TUPLE";

    std::string entityString;

    std::vector<std::pair<std::string, std::set<std::pair<std::string, std::string>>>> relations;
    std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> attributes;

    bool readGeneric(std::ifstream& modelStream, std::string fileName);
    bool readScheme(std::ifstream& modelStream, int* lineNum);
    bool readRelations(std::ifstream& modelStream, int* lineNum);
    bool readAttributes(std::ifstream& modelStream, int* lineNum);

    void writeRelations(TAGraph* graph, std::pair<std::string, std::set<std::pair<std::string, std::string>>> relation);
    void writeAttributes(TAGraph* graph, std::pair<std::string, std::vector<std::pair<std::string, std::string>>> attr);

    std::vector<std::string> prepareLine(std::string line);
    int checkForComment(std::vector<std::string> line, bool &blockComment);
    int checkForBlockEnd(std::vector<std::string> line, bool &blockComment);
    int findRelEntry(std::string name);
    std::vector<std::string> prepareAttributeLine(std::string line);
    std::vector<std::string> prepareAttributes(std::string attrList, bool &success);
    int findAttrEntry(std::string attrName);
    void getAttribute(std::string curAttr, std::string &key, std::string &value);
};


#endif //CLANGEX_TAPROCESSOR_H