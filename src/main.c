#include <stdio.h>
#include <stdbool.h>

#include "raylib.h"
#include "utils.h"
#include "draw.h"
#include "nodes.h"
#include "parser.h"

void print_indent(int indent) {
    for(int i = 0; i < indent; i++) {
        putchar(' ');
    }
}

void print_md_node(MDNode *node, int indent);

void print_md_node_children(MDNodeList children, int indent) {
    MDNode *node = children.head;
    while(node != NULL) {
        print_md_node(node, indent);
        node = node->next;
    }
}

void print_md_node(MDNode *node, int indent) {
    print_indent(indent);
    switch(node->type) {
        case MD_DOCUMENT_NODE:
            printf("DOCUMENT {\n");
            print_md_node_children(node->children, indent + 4);
            printf("}\n");
            break;
        case MD_HEADER_NODE:
            printf("HEADER(%u) {\n", node->header.level);
            print_md_node_children(node->children, indent + 4);
            print_indent(indent);
            printf("}\n");
            break;
        case MD_TEXT_NODE:
            printf("TEXT(%s)\n", node->text);
            break;
        case MD_P_NODE:
            printf("PARAGRAPH {");
            print_md_node_children(node->children, indent + 4);
            print_indent(indent);
            printf("}\n");
            break;
        default:
            TraceLog(LOG_ERROR, "Node (%d) not implemented yet", node->type);
    }
}

int main(int argc, const char **args) {
    if(argc != 2) {
        printf("Usage: ./main <file-path>\n");
        return 1;
    }

    const char *filePath = args[1];
    ParserData data = {
        .arena = arena_create(),
    };

    parse_file(filePath, &data);

    if(data.docNode == NULL) {
        return 1;
    }

    InitWindow(1280, 720, "C Markdown Renderer");
    SetTargetFPS(60);

    draw_init();

    Camera2D camera = {
        .zoom = 1,
    };

    while(!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode2D(camera);
        draw_document_node(data.docNode);
        EndMode2D();

        // if(IsKeyDown(KEY_DOWN) && ctx.pos.y > GetScreenHeight()) {
        //     camera.offset.y -= 10;
        //
        //     float bottom = GetScreenHeight() - ctx.pos.y - padding * 2;
        //
        //     if(camera.offset.y < bottom) {
        //         camera.offset.y = bottom;
        //     }
        // } else if(IsKeyDown(KEY_UP)) {
        //     camera.offset.y += 10;
        //
        //     if(camera.offset.y > 0) {
        //         camera.offset.y = 0;
        //     }
        // }

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
