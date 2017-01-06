//
// Created by bmuscede on 22/12/16.
//

#include <fstream>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include "TAProcessor.h"

using namespace std;

/*
 * Trim Operations
 * Taken From: http://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
 */
static inline string &ltrim(string &s) {
    s.erase(s.begin(), find_if(s.begin(), s.end(),
                                    not1(ptr_fun<int, int>(isspace))));
    return s;
}
static inline string &rtrim(string &s) {
    s.erase(find_if(s.rbegin(), s.rend(),
                         not1(ptr_fun<int, int>(isspace))).base(), s.end());
    return s;
}
static inline string &trim(string &s) {
    return ltrim(rtrim(s));
}

TAProcessor::TAProcessor(string entityRelName){
    this->entityString = entityRelName;
}

TAProcessor::~TAProcessor(){ }

bool TAProcessor::readTAFile(string fileName){
    //Starts by creating the file stream.
    ifstream modelStream(fileName);

    //Check if the file opens.
    if (!modelStream.is_open()){
        cerr << "The TA file " << fileName << " does not exist!" << endl;
        cerr << "Exiting program..." << endl;
        return false;
    }

    //Next starts the main loop.
    bool success = readGeneric(modelStream, fileName);

    modelStream.close();
    return success;
}

bool TAProcessor::writeTAFile(string fileName){
    //Open up a file pointer.
    ofstream taFile;
    taFile.open(fileName.c_str());

    //Check if it opened.
    if (!taFile.is_open()){
        cerr << "The output TA file could not be written to the file called " << fileName << "!" << endl;
        return false;
    }

    //Generates the TA file.
    taFile << generateTAString();
    taFile.close();

    cout << "TA file successfully written to " << fileName << "!" << endl;
    return true;
}

bool TAProcessor::readTAGraph(TAGraph* graph){
    if (graph == NULL){
        cerr << "Invalid TA graph object supplied." << endl;
        cerr << "Please supply an initialized TA graph object!" << endl;
        return false;
    }

    //We simply read through the nodes and edges.
    processNodes(graph->getNodes());
    processEdges(graph->getEdges());

    return true;
}

TAGraph* TAProcessor::writeTAGraph(){
    //Create a new graph.
    TAGraph* graph = new TAGraph();

    //We now iterate through the facts first.
    bool succ = writeRelations(graph);
    if (!succ) return NULL;
    succ = writeAttributes(graph);
    if (!succ) return NULL;

    return graph;
}

bool TAProcessor::readGeneric(ifstream& modelStream, string fileName){
    bool running = true;
    bool tupleEncountered = false;

    //Starts by iterating until complete.
    string curLine;
    int line = 1;
    while(running){
        //We've hit the end.
        if (!getline(modelStream, curLine)){
            running = false;
            continue;
        }

        //We now check the line.
        if (!curLine.compare(0, SCHEME_FLAG.size(), SCHEME_FLAG)){
            //Fast forward.
            bool success = readScheme(modelStream, &line);
            if (!success) return false;

        } else if (!curLine.compare(0, RELATION_FLAG.size(), RELATION_FLAG)){
            tupleEncountered = true;

            //Reads the relations.
            bool success = readRelations(modelStream, &line);
            if (!success) return false;
        } else if (!curLine.compare(0, ATTRIBUTE_FLAG.size(), ATTRIBUTE_FLAG)){
            if (tupleEncountered == false){
                cerr << "Error on line " << line << "." << endl;
                cerr << ATTRIBUTE_FLAG << " encountered before " << RELATION_FLAG << "!" << endl;
                return false;
            }

            //Reads the attributes.
            bool success = readAttributes(modelStream, &line);
            if (!success) return false;
        }

        line++;
    }

    //Checks whether we've encountered a "fact tuple" section.
    if (tupleEncountered){
        return true;
    }

    return false;
}

bool TAProcessor::readScheme(ifstream& modelStream, int* lineNum){
    string line;

    //Start iterating through
    auto pos = modelStream.tellg();
    while(getline(modelStream, line)){
        //Check the line.
        if (!line.compare(0, SCHEME_FLAG.size(), SCHEME_FLAG)){
            //Invalid input.
            cerr << "Invalid input on line " << *lineNum << "." << endl;
            cerr << "Unexpected flag." << endl;

            return false;
        } else if (!line.compare(0, RELATION_FLAG.size(), RELATION_FLAG) ||
                !line.compare(0, ATTRIBUTE_FLAG.size(), ATTRIBUTE_FLAG)) {
            //Breaks out of the loop.
            break;
        }

        //Get the current line.
        pos = modelStream.tellg();
        (*lineNum)++;
    }

    //Seeks backward.
    modelStream.seekg(pos);
    return true;
}

bool TAProcessor::readRelations(ifstream& modelStream, int* lineNum){
    string line;
    bool blockComment = false;
    (*lineNum)--;

    //Start iterating through
    auto pos = modelStream.tellg();
    while(getline(modelStream, line)){
        (*lineNum)++;

        //Tokenize.
        vector<string> entry = prepareLine(line, blockComment);
        if (line.compare("") == 0 || line.find_first_not_of(' ') != std::string::npos) continue;
        if (blockComment && entry.size() == 0) continue;

        //Check whether the entry is valid.
        if (entry.size() < 3) {
            cerr << "Invalid input on line " << *lineNum << "." << endl;
            cerr << "Line should contain a single tuple in RSF format." << endl;
            return false;
        }

        //Next, gets the relation name.
        auto relName = entry.at(0);
        auto toName = entry.at(1);
        auto fromName = entry.at(2);

        //Finds if a pair exists.
        int pos = findRelEntry(relName);
        if (pos == -1) {
            createRelEntry(relName);
            pos = (int) relations.size() - 1;
        }

        //Creates a to from pair.
        pair<string, string> edge = pair<string, string>();
        edge.first = toName;
        edge.second = fromName;

        //Inserts it.
        relations.at(pos).second.insert(edge);

    }

    return true;
}

bool TAProcessor::readAttributes(ifstream& modelStream, int* lineNum){
    string line;
    bool blockComment = false;
    (*lineNum)--;

    //Start iterating through
    auto pos = modelStream.tellg();
    while(getline(modelStream, line)) {
        (*lineNum)++;

        //Prepare the line.
        vector<string> entry = prepareLine(line, blockComment);
        if (line.compare("") == 0 || line.find_first_not_of(' ') != std::string::npos) continue;
        if (blockComment && line.size() == 0) continue;

        //Checks for what type of system we're dealing with.
        bool succ = true;
        if (entry.at(0).compare("(") == 0 || entry.at(0).find("(") == 0) {
            //Relation attribute.
            if (entry.at(0).compare("(") == 0){
                entry.erase(entry.begin());
            } else {
                entry.at(0).erase(0, 1);
            }

            //Check for valid entry.
            if (entry.size() < 2 || entry.at(1).compare(")") == 0){
                cerr << "Invalid input on line " << *lineNum << "." << endl;
                cerr << "Attribute line is too short to be valid!" << endl;
                return false;
            }

            //Gets the IDs.
            string srcID = entry.at(0);
            if (entry.at(1).back() == ')'){
                entry.at(1).erase(entry.at(1).size() - 1, 1);
            } else {
                entry.erase(entry.begin() + 2);
            }
            string dstID = entry.at(1);

            //Generates the attribute list.
            auto attrs = generateAttributes(*lineNum, succ, entry);
            if (!succ) return false;

            //Next, we insert
            int pos = findAttrEntry(srcID, dstID);
            if (pos == -1) {
                createAttrEntry(srcID, dstID);
                pos = (int) attributes.size() - 1;
            }
            this->attributes.at(pos).second = attrs;
        } else {
            //Regular attribute.
            //Gets the name and trims down the vector.
            string attrName = entry.at(0);
            entry.erase(entry.begin());

            //Generates the attribute list.
            auto attrs = generateAttributes(*lineNum, succ, entry);
            if (!succ) return false;

            //Next, we insert
            int pos = findAttrEntry(attrName);
            if (pos == -1) {
                createAttrEntry(attrName);
                pos = (int) attributes.size() - 1;
            }
            this->attributes.at(pos).second = attrs;
        }
    }

    return true;
}

bool TAProcessor::writeRelations(TAGraph* graph){
    //First, finds the instance relation.
    int pos = findRelEntry(entityString);
    if (pos == -1){
        cerr << "TA file does not have a relation called " << entityString << "!" << endl;
        cerr << "Cannot continue..." << endl;
        return false;
    }

    //Gets the entity relation.
    auto entity = relations.at(pos).second;
    for (auto entry : entity){
        //Gets the name.
        string ID = entry.first;

        //Gets the ClangNode enum.
        ClangNode::NodeType type = ClangNode::getTypeNode(entry.second);

        //Creates a new node.
        ClangNode* node = new ClangNode(ID, ID, type);
        graph->addNode(node);
    }

    //Next, processes the other relationships.
    int i = 0;
    for (auto rels : relations){
        if (i == pos) continue;

        string relName = rels.first;
        ClangEdge::EdgeType type = ClangEdge::getTypeEdge(relName);

        std::set<pair<string, string>>::iterator it;
        for (it = rels.second.begin(); it != rels.second.end(); it++) {
            auto nodes = *it;

            //Gets the nodes.
            ClangNode* src = graph->findNodeByID(nodes.first);
            ClangNode* dst = graph->findNodeByID(nodes.second);

            if (src == NULL || dst == NULL){
                graph->addUnresolvedRef(nodes.first, nodes.second, type);
                continue;
            }

            //Creates a new edge.
            ClangEdge* edge = new ClangEdge(src, dst, type);
        }
    }

    return true;
}

bool TAProcessor::writeAttributes(TAGraph* graph){
    //We simply go through and process them.
    for (auto attr : attributes){
        string itemID = attr.first;

        //Next, we go through all the KVs.
        for (auto kv : attr.second){
            string key = kv.first;
            vector<string> values = kv.second;

            //Now, updates the attributes.
            for (auto value : values) {
                bool succ = graph->addAttribute(itemID, key, value);
                if (!succ) {
                    cerr << "TA file does not have a node called " << itemID << "!" << endl;
                    cerr << "This item needs to be specified before giving it attributes." << endl;
                    return false;
                }
            }
        }
    }

    //Next, we deal with relation attributes.
    for (auto attr : relAttributes){
        string srcID = attr.first.first;
        string dstID = attr.first.second;

        //Next, we go through all the KVs.
        for (auto kv : attr.second){
            string key = kv.first;
            vector<string> values = kv.second;

            //Now, updates the attributes.
            for (auto value : values) {
                bool succ = graph->addAttribute(srcID, dstID, key, value);
                if (!succ) {
                    cerr << "TA file does not have an edge called (" << srcID << ", " << dstID << ")!" << endl;
                    cerr << "This item needs to be specified before giving it attributes." << endl;
                    return false;
                }
            }
        }
    }

    return true;
}

string TAProcessor::generateTAString(){
    string taString = "";

    //Gets the time.
    time_t now = time(0);
    string curTime = string(ctime(&now));

    //Start by generating the header.
    taString += SCHEMA_HEADER + "\n";
    taString += "//Generated on: " + curTime + "\n";

    //Next, gets the relations.
    taString += generateRelationString();
    taString += generateAttributeString();

    return taString;
}

string TAProcessor::generateRelationString(){
    string relString = "";
    relString += RELATION_FLAG + "\n";

    //Iterate through the relations.
    for (auto curr : relations){
        string relName = curr.first;

        //Iterate through the entries.
        for (auto currRel : curr.second){
            relString += relName + " " + currRel.first + " " + currRel.second + "\n";
        }
    }

    relString + "\n";
    return relString;
}

string TAProcessor::generateAttributeString(){
    string attrString = "";
    attrString += ATTRIBUTE_FLAG + "\n";

    //Iterate through the entity attributes first.
    for (auto curr : attributes){
        string attributeID = curr.first;
        attrString += attributeID + generateAttributeStringFromKVs(curr.second) + "\n";
    }

    //Next, deals with the relation attribute list.
    for (auto curr : relAttributes){
        attrString += "(" + curr.first.first + " " +
                curr.first.second + ")" + generateAttributeStringFromKVs(curr.second) + "\n";
    }

    return attrString;
}

string TAProcessor::generateAttributeStringFromKVs(vector<pair<string, vector<string>>> attr){
    string attrString = " { ";

    //Iterate through the pairs.
    for (auto currAttr : attr){
        //Check what type of string we need to generate.
        if (currAttr.second.size() == 1){
            attrString += currAttr.first + " = " + currAttr.second.at(0) + " ";
        } else {
            attrString += currAttr.first + " = (";
            for (auto value : currAttr.second)
                attrString += " " + value;

            attrString += " ) ";
        }
    }
    attrString += " }";

    return attrString;
}

vector<pair<string, vector<string>>> TAProcessor::generateAttributes(int lineNum, bool& succ,
                                                                     std::vector<std::string> line){
    if (line.size() < 3){
        succ = false;
        return vector<pair<string, vector<string>>>();
    }

    //Start by expecting the { symbol.
    if (line.at(0).compare("{") == 0){
        line.erase(line.begin());
    } else if (line.at(0).find("{") == 0){
        line.at(0).erase(0, 1);
    } else {
        succ = false;
        return vector<pair<string, vector<string>>>();
    }

    //Now, we iterate until we hit the end.
    int i = 0;
    bool end = false;
    string current = line.at(i);
    vector<pair<string, vector<string>>> attrList = vector<pair<string, vector<string>>>();
    do {
        //Adds in the first part of the entry.
        pair<string, vector<string>> currentEntry = pair<string, vector<string>>();
        currentEntry.first = current;

        //Checks for validity.
        if (i + 2 >= current.size() || line.at(++i).compare("=") != 0){
            succ = false;
            return vector<pair<string, vector<string>>>();
        }

        //Gets the next KV pair.
        string next = line.at(++i);
        if (next.compare("(") == 0 || next.find("(") == 0) {
            //First, remove the ( symbol.
            if (next.find("(") == 0) {
                next.erase(0, 1);
            } else {
                if (i + 1 == line.size()){
                    succ = false;
                    return vector<pair<string, vector<string>>>();
                }
                next = line.at(++i);
            }

            //We now iterate through the attribute list.
            bool endList = false;
            do {
                //Check if we hit the end.
                if (next.find(")") == next.size() - 1){
                    next.erase(next.size() - 1, 1);
                    endList = true;
                } else if (next.compare(")") == 0) {
                    break;
                }

                //Next, process the item.
                currentEntry.second.push_back(next);

                //Finally check if we've hit the conditions.
                if (endList){
                    break;
                } else if (i + 1 == line.size()){
                    succ = false;
                    return vector<pair<string, vector<string>>>();
                } else {
                    next = line.at(++i);
                }
            } while (true);
        } else {
            //Check if we have a "} symbol at the end.
            if (next.find("}") == next.size() - 1){
                end = true;
                next.erase(next.size() - 1, 1);
            }

            //Add it to the current entry.
            currentEntry.second.push_back(next);
        }

        //Increments the current string.
        if (i + 1 == line.size() && end == false){
            succ = false;
            return vector<pair<string, vector<string>>>();
        } else if (!end){
            current = line.at(++i);
        }

        //Adds the entry in.
        attrList.push_back(currentEntry);
    } while (current.compare("}") != 0 && end == false);

    return attrList;
}

vector<string> TAProcessor::prepareLine(string line, bool& blockComment){
    //Perform comment processing.
    line = removeStandardComment(line);
    line = removeBlockComment(line, blockComment);

    //Split into a vector.
    vector<string> stringList;
    boost::split(stringList, line, boost::is_any_of(" "));

    //Trim the strings.
    vector<string> modified;
    for (string curr : stringList){
        trim(curr);
        modified.push_back(curr);
    }

    return modified;
}

string TAProcessor::removeStandardComment(string line){
    //Iterate through the string two characters at a time.
    for (int i = 0; i + 1 < line.size(); i++){
        //Get the next two characters.
        char first = line.at(i);
        char second = line.at(i + 1);

        //Erase the characters.
        if (first == COMMENT_CHAR && second == COMMENT_CHAR){
            //We want to purge the line of all subsequent characters.
            line.erase(i, string::npos);
            break;
        }
    }

    return line;
}

string TAProcessor::removeBlockComment(string line, bool& blockComment){
    string newLine = "";

    //Iterate through the string two characters at a time.
    for (int i = 0; i + 1 < line.size(); i++){
        //Get the next two characters.
        char first = line.at(i);
        char second = line.at(i + 1);

        //Check if we have a block comment ending to look for.
        if (blockComment == true && (first == COMMENT_BLOCK_CHAR && second == COMMENT_CHAR)){
            //Set block comment to false.
            blockComment = false;

            //Now, we move the pointer ahead by 1.
            i++;
        } else if (blockComment == false && (first == COMMENT_CHAR && second == COMMENT_BLOCK_CHAR)){
            //Set block comment to true.
            blockComment = true;

            //Now, we move the pointer ahead by 1.
            i++;
        } else if (blockComment == false) {
            //Adds the character.
            newLine += first;
        }
    }

    return newLine;
}

int TAProcessor::findRelEntry(string name){
    int i = 0;

    //Goes through the relation vector.
    for (auto rel : relations){
        if (rel.first.compare(name) == 0) return i;

        i++;
    }

    return -1;
}

void TAProcessor::createRelEntry(string name){
    pair<string, set<pair<string, string>>> entry = pair<string, set<pair<string, string>>>();
    entry.first = name;

    relations.push_back(entry);
}

int TAProcessor::findAttrEntry(string attrName){
    int i = 0;

    //Goes through the attribute vector.
    for (auto attr : attributes){
        if (attr.first.compare(attrName) == 0) return i;

        i++;
    }

    return -1;
}

int TAProcessor::findAttrEntry(string src, string dst){
    int i = 0;

    //Goes through the attribute vector
    for (auto attr : relAttributes){
        if (attr.first.first.compare(src) == 0 &&
                attr.first.second.compare(dst) ==0) return i;

        i++;
    }

    return -1;
}

void TAProcessor::createAttrEntry(string attrName){
    //Create the pair object.
    pair<string, vector<pair<string, vector<string>>>> entry = pair<string, vector<pair<string, vector<string>>>>();
    entry.first = attrName;

    attributes.push_back(entry);
}

void TAProcessor::createAttrEntry(string src, string dst){
    //Create the pair object.
    pair<pair<string, string>, vector<pair<string, vector<string>>>> entry =
        pair<pair<string, string>, vector<pair<string, vector<string>>>>();
    entry.first.first = src;
    entry.first.second = dst;

    relAttributes.push_back(entry);
}

void TAProcessor::processNodes(vector<ClangNode*> nodes){
    //Sees if we have an entry for the current relation.
    int pos = findRelEntry(entityString);
    if (pos == -1) {
        createRelEntry(entityString);
        pos = (int) relations.size() - 1;
    }

    //Iterate through the nodes.
    for (ClangNode* curNode : nodes){
        //Adds in the node information.
        pair<string, string> relPair = pair<string, string>();
        relPair.first = curNode->getID();
        relPair.second = ClangNode::getTypeString(curNode->getType());
        relations.at(pos).second.insert(relPair);

        //Adds in the attributes.
        auto curAttr = curNode->getAttributes();
        if (curAttr.size() < 1) continue;

        //Adds in the attribute entry.
        int attrPos = findAttrEntry(curNode->getID());
        if (attrPos == -1) {
            createAttrEntry(curNode->getID());
            attrPos = (int) attributes.size() - 1;
        }

        //Iterates through them.
        typedef map<string, vector<string>>::iterator itType;
        for(itType it = curAttr.begin(); it != curAttr.end(); it++) {
            pair<string, vector<string>> kVs = pair<string, vector<string>>();
            kVs.first = it->first;

            //Iterates through all the values.
            for (string curVal : it->second){
                //Creates the entry.
                kVs.second.push_back(curVal);
            }

            //Adds the pair to the attribute list.
            attributes.at(attrPos).second.push_back(kVs);
        }
    }
}

void TAProcessor::processEdges(vector<ClangEdge*> edges){
    //Iterate over the edges.
    for (auto curEdge : edges){
        string typeName = ClangEdge::getTypeString(curEdge->getType());

        //Sees if we have an entry for the current type.
        int pos = findRelEntry(typeName);
        if (pos == -1) {
            createRelEntry(typeName);
            pos = (int) relations.size() - 1;
        }

        //Now, we simply add it.
        string srcID = curEdge->getSrc()->getID();
        string dstID = curEdge->getDst()->getID();

        pair<string, string> relPair = pair<string, string>();
        relPair.first = srcID;
        relPair.second = dstID;

        //Add it to the relation list.
        relations.at(pos).second.insert(relPair);


        //Now we deal with any edge attributes;
        auto edgeAttr = curEdge->getAttributes();
        if (edgeAttr.size() < 1) continue;

        //Adds in an attribute entry.
        int attrPos = findAttrEntry(srcID, dstID);
        if (attrPos == -1) {
            createAttrEntry(srcID, dstID);
            attrPos = (int) relAttributes.size() - 1;
        }

        //Iterates through the attributes.
        typedef map<string, vector<string>>::iterator itType;
        for(itType it = edgeAttr.begin(); it != edgeAttr.end(); it++) {
            pair<string, vector<string>> kVs = pair<string, vector<string>>();
            kVs.first = it->first;

            //Iterates through all the values.
            for (string curVal : it->second){
                //Creates the entry.
                kVs.second.push_back(curVal);
            }

            //Adds the pair to the attribute list.
            relAttributes.at(attrPos).second.push_back(kVs);
        }
    }
}
