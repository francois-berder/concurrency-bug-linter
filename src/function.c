#include "function.h"

#include "lib.h"
#include "symbol.h"
#include "linearize.h"

#include <stdio.h>


static void print_tree_helper(struct function *f, int indent)
{
    struct function *child;
    int i;

    for (i = 0; i < indent; ++i)
        printf("  ");
    printf("%.*s\n", f->sym->ep->name->ident->len, f->sym->ep->name->ident->name);

    FOR_EACH_PTR(f->children, child) {
        print_tree_helper(child, indent + 1);
    } END_FOR_EACH_PTR(child);
}

struct function* lookup_func(struct function_list *call_graph, struct symbol* sym)
{
    struct function *f;

    FOR_EACH_PTR(call_graph, f) {
        if (f->sym == sym)
            return f;

        struct function *found = lookup_func(f->children, sym);
        if (found)
            return found;
    } END_FOR_EACH_PTR(f);

    return NULL;
}

void print_tree(struct function *func)
{
    print_tree_helper(func, 0);
}

void print_call_graph(struct function_list *call_graph)
{
    struct function *f;

    FOR_EACH_PTR(call_graph, f) {
        print_tree(f);
    } END_FOR_EACH_PTR(f);
}

