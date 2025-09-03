#ifndef PARSER_H
#define PARSER_H

#include "nodes.h"
#include "utils.h"

typedef struct {
    Arena *arena;
    MDNode *docNode;
    Stack parentStack;
} ParserData;

void parse_file(const char *filePath, ParserData *parserData);

#endif // PARSER_H
