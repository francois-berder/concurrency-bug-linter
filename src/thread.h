#ifndef THEAD_H
#define THEAD_H

#include "lib.h"

struct thread {
    char *file;
    int line;
    char *name;
    struct symbol *sym;
};

DECLARE_PTR_LIST(thread_list, struct thread);

#endif

