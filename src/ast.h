#ifndef AST_H
#define AST_H
#include <stdio.h>
#include <stdint.h>

typedef struct Node {
    char *kind, *value;
    int line;
    struct Node *a, *b, *c, *next;
} Node;
extern Node *root;
Node *node(const char *kind, const char *value, int line, Node *a, Node *b, Node *c);
Node *append(Node *list, Node *item);
char *copy_text(const char *text);
void free_nodes(void);
void fail(int line, const char *message);
int32_t execute(Node *tree);
void generate_python(FILE *out, Node *tree);
void write_html(FILE *out, Node *tree);
#endif
