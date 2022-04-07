#ifndef VARIABLE_H
#define VARIABLE_H

#include "lib.h"
#include "symbol.h"
#include "thread.h"

struct variable {
    struct symbol *sym;
    struct thread_list *threads;
};

DECLARE_PTR_LIST(variable_list, struct variable);

#endif

