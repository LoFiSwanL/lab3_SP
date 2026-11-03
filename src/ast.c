#include "ast.h"
#include "assets.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

/* Every node is owned by this registry, including a partially parsed tree. */
typedef struct Allocation { Node *node; struct Allocation *next; } Allocation;
static Allocation *allocated;
static void *allocate(size_t size) {
    void *result = calloc(1, size);
    if (!result) fail(0, "недостатньо пам'яті");
    return result;
}
char *copy_text(const char *text) {
    char *copy = allocate(strlen(text) + 1);
    strcpy(copy, text);
    return copy;
}
Node *node(const char *kind, const char *value, int line, Node *a, Node *b, Node *c) {
    Node *n = allocate(sizeof *n);
    Allocation *entry = allocate(sizeof *entry);
    entry->node = n; entry->next = allocated; allocated = entry;
    n->kind = copy_text(kind); n->value = copy_text(value); n->line = line;
    n->a = a; n->b = b; n->c = c;
    return n;
}
Node *append(Node *list, Node *item) {
    if (!list) return item;
    Node *last = list;
    while (last->next) last = last->next;
    last->next = item;
    return list;
}
void free_nodes(void) {
    while (allocated) {
        Allocation *next = allocated->next;
        free(allocated->node->kind); free(allocated->node->value);
        free(allocated->node); free(allocated); allocated = next;
    }
}
void fail(int line, const char *message) {
    fprintf(stderr, "Рядок %d: %s\n", line, message);
    exit(EXIT_FAILURE);
}
static int is(Node *n, const char *kind) { return !strcmp(n->kind, kind); }
static int32_t integer(int64_t value, int line) {
    if (value < INT32_MIN || value > INT32_MAX) fail(line, "переповнення int");
    return (int32_t)value;
}
static int32_t literal(Node *n) {
    errno = 0;
    char *end;
    long long value = strtoll(n->value, &end, 0);
    if (errno || *end || value > INT32_MAX) fail(n->line, "літерал поза діапазоном int");
    return (int32_t)value;
}

typedef struct Variable {
    const char *name;
    int32_t value;
    int initialized;
    struct Variable *next;
} Variable;
typedef struct Scope { Variable *variables; struct Scope *parent; } Scope;
typedef struct { Scope *scope; int returned; int32_t result; unsigned steps; } Context;

static void variable_error(int line, const char *reason, const char *name) {
    fprintf(stderr, "Рядок %d: %s %s\n", line, reason, name);
    exit(EXIT_FAILURE);
}
static Variable *lookup(Context *ctx, const char *name, int line) {
    for (Scope *s = ctx->scope; s; s = s->parent)
        for (Variable *v = s->variables; v; v = v->next)
            if (!strcmp(v->name, name)) return v;
    variable_error(line, "неоголошена змінна", name);
    return NULL;
}
static int32_t evaluate(Context *ctx, Node *n) {
    if (is(n, "Number")) return literal(n);
    if (is(n, "Variable")) {
        Variable *v = lookup(ctx, n->value, n->line);
        if (!v->initialized) variable_error(n->line, "неініціалізована змінна", n->value);
        return v->value;
    }
    int64_t a = evaluate(ctx, n->a);
    if (is(n, "Unary")) {
        if (!strcmp(n->value, "!")) return !a;
        return integer(!strcmp(n->value, "-") ? -a : a, n->line);
    }
    /* C logical operators evaluate the right operand only when necessary. */
    if (!strcmp(n->value, "&&")) return a && evaluate(ctx, n->b);
    if (!strcmp(n->value, "||")) return a || evaluate(ctx, n->b);
    int64_t b = evaluate(ctx, n->b);
    if (!strcmp(n->value, "+")) return integer(a + b, n->line);
    if (!strcmp(n->value, "-")) return integer(a - b, n->line);
    if (!strcmp(n->value, "*")) return integer(a * b, n->line);
    if (!strcmp(n->value, "/") || !strcmp(n->value, "%")) {
        if (!b) fail(n->line, "ділення на нуль");
        integer(a / b, n->line);
        return (int32_t)(!strcmp(n->value, "/") ? a / b : a % b);
    }
    if (!strcmp(n->value, "<")) return a < b;
    if (!strcmp(n->value, ">")) return a > b;
    if (!strcmp(n->value, "<=")) return a <= b;
    if (!strcmp(n->value, ">=")) return a >= b;
    if (!strcmp(n->value, "==")) return a == b;
    if (!strcmp(n->value, "!=")) return a != b;
    fail(n->line, "невідомий оператор AST");
    return 0;
}
static void execute_statement(Context *ctx, Node *n) {
    if (++ctx->steps > 100000) fail(n->line, "перевищено ліміт 100000 кроків");
    if (is(n, "Block")) {
        Scope local = {NULL, ctx->scope};
        ctx->scope = &local;
        for (Node *s = n->a; s && !ctx->returned; s = s->next) execute_statement(ctx, s);
        while (local.variables) {
            Variable *next = local.variables->next;
            free(local.variables); local.variables = next;
        }
        ctx->scope = local.parent;
    } else if (is(n, "Declare")) {
        for (Variable *v = ctx->scope->variables; v; v = v->next)
            if (!strcmp(v->name, n->value)) variable_error(n->line, "повторне оголошення змінної", n->value);
        Variable *v = allocate(sizeof *v);
        v->name = n->value; v->next = ctx->scope->variables; ctx->scope->variables = v;
        if (n->a) { v->value = evaluate(ctx, n->a); v->initialized = 1; }
    } else if (is(n, "Assign")) {
        int32_t value = evaluate(ctx, n->a);
        Variable *v = lookup(ctx, n->value, n->line);
        v->value = value; v->initialized = 1;
    } else if (is(n, "If")) {
        if (evaluate(ctx, n->a)) execute_statement(ctx, n->b);
        else if (n->c) execute_statement(ctx, n->c);
    } else if (is(n, "While")) {
        while (!ctx->returned && evaluate(ctx, n->a)) execute_statement(ctx, n->b);
    } else if (is(n, "Return")) {
        ctx->result = evaluate(ctx, n->a); ctx->returned = 1;
    }
}
int32_t execute(Node *tree) {
    Context ctx = {0};
    execute_statement(&ctx, tree->a);
    return ctx.result;
}

/* AST -> Python. A tiny runtime preserves C integer and block semantics. */
static void indent(FILE *out, int depth) { for (int i = 0; i < depth; i++) fputs("    ", out); }
static void expression(FILE *out, Node *n) {
    if (is(n, "Number")) { fprintf(out, "%d", literal(n)); return; }
    if (is(n, "Variable")) { fprintf(out, "rt.get('%s', %d)", n->value, n->line); return; }
    if (is(n, "Unary")) {
        fprintf(out, "rt.unary('%s', ", n->value); expression(out, n->a);
        fprintf(out, ", %d)", n->line); return;
    }
    if (!strcmp(n->value, "&&") || !strcmp(n->value, "||")) {
        fputs("int(bool(", out); expression(out, n->a);
        fprintf(out, ") %s bool(", !strcmp(n->value, "&&") ? "and" : "or");
        expression(out, n->b); fputs("))", out); return;
    }
    fprintf(out, "rt.binary('%s', ", n->value); expression(out, n->a); fputs(", ", out);
    expression(out, n->b); fprintf(out, ", %d)", n->line);
}
static void statement(FILE *out, Node *n, int depth) {
    indent(out, depth); fprintf(out, "rt.tick(%d)\n", n->line);
    indent(out, depth);
    if (is(n, "Block")) {
        fputs("with rt.block():\n", out);
        if (!n->a) { indent(out, depth + 1); fputs("pass\n", out); }
        for (Node *s = n->a; s; s = s->next) statement(out, s, depth + 1);
    } else if (is(n, "Declare")) {
        fprintf(out, "rt.declare('%s', %d)\n", n->value, n->line);
        if (n->a) {
            indent(out, depth); fprintf(out, "rt.set('%s', ", n->value); expression(out, n->a);
            fprintf(out, ", %d)\n", n->line);
        }
    } else if (is(n, "Assign")) {
        fprintf(out, "rt.set('%s', ", n->value); expression(out, n->a);
        fprintf(out, ", %d)\n", n->line);
    } else if (is(n, "Return")) {
        fputs("return ", out); expression(out, n->a); fputc('\n', out);
    } else if (is(n, "If")) {
        fputs("if ", out); expression(out, n->a); fputs(":\n", out);
        statement(out, n->b, depth + 1);
        if (n->c) { indent(out, depth); fputs("else:\n", out); statement(out, n->c, depth + 1); }
    } else if (is(n, "While")) {
        fputs("while ", out); expression(out, n->a); fputs(":\n", out);
        statement(out, n->b, depth + 1);
    } else fputs("pass\n", out);
}
void generate_python(FILE *out, Node *tree) {
    fputs(python_runtime, out);
    fputs("\n\ndef main():\n", out);
    statement(out, tree->a, 1);
    fputs("    return 0\n\n\nif __name__ == '__main__':\n"
          "    try:\n        print(main())\n"
          "    except RuntimeError as error:\n"
          "        import sys\n        print(error, file=sys.stderr)\n        sys.exit(1)\n", out);
}

/* Embed the actual AST as JSON, followed by an offline SVG viewer. */
static void json_string(FILE *out, const char *text) {
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (*p == '"' || *p == '\\') fprintf(out, "\\%c", *p);
        else if (*p < 32 || *p == '<' || *p == '>') fprintf(out, "\\u%04x", *p);
        else fputc(*p, out);
    }
    fputc('"', out);
}
static void json_node(FILE *out, Node *n) {
    fputs("{\"kind\":", out); json_string(out, n->kind);
    fputs(",\"value\":", out); json_string(out, n->value);
    fprintf(out, ",\"line\":%d,\"children\":[", n->line);
    int comma = 0;
    Node *children[] = {n->a, n->b, n->c};
    for (int i = 0; i < 3; i++) {
        for (Node *child = children[i]; child; child = child->next) {
            if (comma++) fputc(',', out);
            json_node(out, child);
        }
    }
    fputs("]}", out);
}
void write_html(FILE *out, Node *tree) {
    fputs("<!doctype html><html lang=\"uk\"><meta charset=\"utf-8\">\n"
          "<script id=\"ast-data\" type=\"application/json\">", out);
    json_node(out, tree);
    fputs("</script>\n", out);
    fputs(html_viewer, out);
}
