#include "ast.h"
#include <stdlib.h>
#include <string.h>

extern FILE *yyin;
int yyparse(void);
int yylex_destroy(void);

static FILE *output(const char *path) {
    FILE *file = fopen(path, "w");
    if (!file) { perror(path); exit(EXIT_FAILURE); }
    return file;
}

int main(int argc, char **argv) {
    const char *python_path = NULL, *html_path = NULL;
    int run = 0;
    if (argc < 2) {
        fprintf(stderr, "Використання: %s file.c [--run] [--python file.py] [--html file.html]\n", argv[0]);
        return EXIT_FAILURE;
    }
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--run")) run = 1;
        else if (!strcmp(argv[i], "--python") && i + 1 < argc) python_path = argv[++i];
        else if (!strcmp(argv[i], "--html") && i + 1 < argc) html_path = argv[++i];
        else { fprintf(stderr, "Невідомий або неповний параметр: %s\n", argv[i]); return EXIT_FAILURE; }
    }
    if ((python_path && !strcmp(argv[1], python_path)) ||
        (html_path && !strcmp(argv[1], html_path)) ||
        (python_path && html_path && !strcmp(python_path, html_path))) {
        fprintf(stderr, "Вхідний файл і результати повинні мати різні шляхи\n");
        return EXIT_FAILURE;
    }
    atexit(free_nodes);
    yyin = fopen(argv[1], "r");
    if (!yyin) { perror(argv[1]); return EXIT_FAILURE; }
    int status = yyparse();
    fclose(yyin);
    yylex_destroy();
    if (status) return EXIT_FAILURE;
    puts("Синтаксис: OK");
    if (run) printf("Результат main: %d\n", execute(root));
    if (python_path) {
        FILE *file = output(python_path);
        generate_python(file, root);
        if (fclose(file)) { perror(python_path); return EXIT_FAILURE; }
        printf("Python: %s\n", python_path);
    }
    if (html_path) {
        FILE *file = output(html_path);
        write_html(file, root);
        if (fclose(file)) { perror(html_path); return EXIT_FAILURE; }
        printf("AST: %s\n", html_path);
    }
    return EXIT_SUCCESS;
}
