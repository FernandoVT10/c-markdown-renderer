#ifndef NODES_H
#define NODES_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    MD_DOCUMENT_NODE = 0,
    MD_HEADER_NODE,
    MD_TEXT_NODE,
    MD_P_NODE,
    MD_LIST_NODE,
    MD_LIST_ITEM_NODE,
    MD_BOLD_NODE,
} MDNodeType;

typedef struct MDNode MDNode;

typedef struct {
    MDNode *head, *tail;
    size_t count;
} MDNodeList;

typedef struct {
    unsigned int level;
} MDHeaderNode;

typedef struct {
    bool ordered;
    unsigned int startIndex; // from where a ordered list starts
} MDListNode;

struct MDNode {
    MDNodeType type;
    MDNodeList children;
    MDNode *next;

    union {
        MDHeaderNode header;
        char *text; // null-terminated string
        MDListNode list;
    };
};

#endif // NODES_H
