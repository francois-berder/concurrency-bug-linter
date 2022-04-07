#ifndef FUNCTION_H
#define FUNCTION_H

#include "lib.h"

struct function;

DECLARE_PTR_LIST(function_list, struct function);

struct function {
    struct symbol *sym;
    struct function_list *children;
};

struct function* lookup_func(struct function_list *call_graph, struct symbol* sym);

void print_tree(struct function *func);
void print_call_graph(struct function_list *call_graph);


#endif

