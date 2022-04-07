#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

#include "lib.h"
#include "allocate.h"
#include "token.h"
#include "target.h"
#include "parse.h"
#include "symbol.h"
#include "expression.h"
#include "linearize.h"

#include "cJSON.h"
#include "function.h"
#include "variable.h"
#include "thread.h"

static void find_calls(struct function_list *call_graph, struct function *f)
{
    struct basic_block *bb;
    struct instruction *insn;

    FOR_EACH_PTR(f->sym->ep->bbs, bb) {
        if (!bb)
            continue;
        if (!bb->parents && !bb->children && !bb->insns)
            continue;

        FOR_EACH_PTR(bb->insns, insn) {
            if (!insn->bb)
                continue;
            if (insn->opcode == OP_CALL) {

                /* Find the symbol for the callee's definition */
                struct symbol * sym;
                if (insn->func->type == PSEUDO_SYM) {
                    for (sym = insn->func->sym->ident->symbols;
                         sym; sym = sym->next_id) {
                        if (sym->namespace & NS_SYMBOL && sym->ep)
                            break;
                    }
                    if (sym) {
                        struct function *child = lookup_func(call_graph, sym);
                        if (child)
                            add_ptr_list(&f->children, child);
                    }
                }
            }
        } END_FOR_EACH_PTR(insn);
    } END_FOR_EACH_PTR(bb);
}

static int find_var_usage(struct function *f, struct symbol *var)
{
    struct basic_block *bb;
    struct instruction *insn;

    FOR_EACH_PTR(f->sym->ep->bbs, bb) {
        if (!bb)
            continue;
        if (!bb->parents && !bb->children && !bb->insns)
            continue;

        FOR_EACH_PTR(bb->insns, insn) {
            if (!insn->bb)
                continue;

            if (insn->opcode == OP_LOAD || insn->opcode == OP_STORE) {
                if (insn->addr->type == PSEUDO_SYM) {
                    if (insn->addr->sym == var) {
                        return 1;
                    }
                }
            }
        } END_FOR_EACH_PTR(insn);
    } END_FOR_EACH_PTR(bb);

    return 0;
}

static int find_var_usage_in_thread(struct function *f, struct symbol *var)
{
    struct function *child;

    if (find_var_usage(f, var))
        return 1;

    FOR_EACH_PTR(f->children, child) {
        if (find_var_usage(child, var))
            return 1;
    } END_FOR_EACH_PTR(child);

    return 0;
}

static char** parse_compile_db(char *filename)
{
    FILE *f = NULL;
    long int length;
    char *string = NULL;
    cJSON *json = NULL;
    int file_count;
    int i;
    char **results;
    char *file = NULL;
    char *arg = NULL;
    struct string_list *filelist = NULL;
    struct string_list *arglist = NULL;

    f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Failed to open compile db file %s\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    length = ftell(f);
    if (length < 0) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);
    string = malloc(length);
    if (!string) {
        fclose(f);
        return NULL;
    }
    if (fread(string, length, 1, f) != 1) {
        free(string);
        fclose(f);
        return NULL;
    }
    fclose(f);

    json = cJSON_Parse(string);
    if (!json) {
        free(string);
        return NULL;
    }

    if (!cJSON_IsArray(json)) {
        free(string);
        cJSON_Delete(json);
        return NULL;
    }

    file_count = cJSON_GetArraySize(json);
    for (i = 0; i < file_count; ++i) {
        cJSON *item, *args, *directory, *file;
        char *dir, *f;
        int arg_count;
        int j;
        char *filepath;

        item = cJSON_GetArrayItem(json, i);
        args = cJSON_GetObjectItemCaseSensitive(item, "arguments");
        directory = cJSON_GetObjectItemCaseSensitive(item, "directory");
        file = cJSON_GetObjectItemCaseSensitive(item, "file");
        
        if (!args || !directory || !file)
            continue;

        if (!cJSON_IsArray(args) || !cJSON_IsString(directory) || !cJSON_IsString(file))
            continue;

        arg_count = cJSON_GetArraySize(args);
        for (j = 0; j < arg_count; ++j) {
            cJSON *arg = cJSON_GetArrayItem(args, j);
            char *str;
            int arg_length;

            if (!cJSON_IsString(arg))
                continue;

            str = cJSON_GetStringValue(arg);
            arg_length = strlen(str);
            if (arg_length <= 2)
                continue;

            /* Keep only macro and include */
            if (str[0] == '-' && (str[1] == 'D' || str[1] == 'I')) {
                char *tmp = malloc(arg_length + 1);
                strcpy(tmp, str);
                add_ptr_list(&arglist, tmp);
            }
        }

        dir = cJSON_GetStringValue(directory);
        f = cJSON_GetStringValue(file);

        filepath = malloc(strlen(dir) + strlen(f) + 2);
        strcpy(filepath, dir);
        strcat(filepath, "/");
        strcat(filepath, f);

        add_ptr_list(&filelist, filepath);
    }

    cJSON_Delete(json);
    free(string);

    results = malloc(sizeof(char*) * (ptr_list_size((struct ptr_list*)arglist) + ptr_list_size((struct ptr_list*)filelist) + 2));
    i = 0;
    results[i++] = "sparse";

    FOR_EACH_PTR(arglist, arg) {
        results[i++] = arg;
    } END_FOR_EACH_PTR(arg);

    FOR_EACH_PTR(filelist, file) {
        results[i++] = file;
    } END_FOR_EACH_PTR(file);

    results[i] = NULL;

    return results;
}

static struct thread_list* parse_thread_config_file(char *filename)
{
    FILE *f = NULL;
    long int length;
    char *string = NULL;
    cJSON *json = NULL;
    struct thread_list *threads = NULL;
    int i, thread_count;

    f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Failed to open thread config file %s\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    length = ftell(f);
    if (length < 0) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);
    string = malloc(length);
    if (!string) {
        fclose(f);
        return NULL;
    }
    if (fread(string, length, 1, f) != 1) {
        free(string);
        fclose(f);
        return NULL;
    }
    fclose(f);

    json = cJSON_Parse(string);
    if (!json) {
        free(string);
        return NULL;
    }
    
    if (!cJSON_IsArray(json)) {
        free(string);
        cJSON_Delete(json);
        return NULL;
    }

    thread_count = cJSON_GetArraySize(json);
    for (i = 0; i < thread_count; ++i) {
        cJSON *item, *name, *file, *line;
        struct thread *t;

        item = cJSON_GetArrayItem(json, i);
        name = cJSON_GetObjectItemCaseSensitive(item, "name");
        file = cJSON_GetObjectItemCaseSensitive(item, "file");
        line = cJSON_GetObjectItemCaseSensitive(item, "line");

        if (!name && !file && !line) {
            continue;
        }

        t = malloc(sizeof(struct thread));
        if (!t)
            continue;

        memset(t, 0, sizeof(struct thread));

        if (name && cJSON_IsString(name))
            t->name = strdup(cJSON_GetStringValue(name));
        if (file && cJSON_IsString(file))
            t->file = strdup(cJSON_GetStringValue(file));
        if (line && cJSON_IsNumber(line))
            t->line = cJSON_GetNumberValue(line);

        add_ptr_list(&threads, t);
    }

    cJSON_Delete(json);
    free(string);

    return threads;
}

int main(int argc, char **argv)
{
    struct string_list *filelist = NULL;
    char *file;
    struct symbol *sym;
    struct symbol_list *fsyms, *all_syms=NULL;
    struct function_list *call_graph = NULL;
    struct function *f;
    struct variable_list *variables = NULL;
    struct variable *var;
    struct thread_list *threads = NULL;
    struct thread *t;
    char **args;
    int arg_len;

    /* 1. Parse arguments */
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <compile-db> <thread-config>\n", argv[0]);
        return -1;
    }

    args = parse_compile_db(argv[1]);
    threads = parse_thread_config_file(argv[2]);
    arg_len = 0;
    while (args[arg_len])
        arg_len++;

    /* 2. Collect all functions and variables */
    fsyms = sparse_initialize(arg_len, args, &filelist);
    concat_symbol_list(fsyms, &all_syms);
    FOR_EACH_PTR(filelist, file) {
        fsyms = sparse(file);

        concat_symbol_list(fsyms, &all_syms);
        FOR_EACH_PTR(fsyms, sym) {
            expand_symbol(sym);
            linearize_symbol(sym);
        } END_FOR_EACH_PTR(sym);

        FOR_EACH_PTR(fsyms, sym) {
            if (is_integral_type(sym)) {
                /* We ignore variables on the stack */
                if ((sym->ctype.modifiers & MOD_STATIC)
                || (sym->ctype.modifiers & MOD_TOPLEVEL)) {
                    struct variable *var = malloc(sizeof(struct variable));
                    memset(var, 0, sizeof(struct variable));
                    var->sym = sym;
                    add_ptr_list(&variables, var);
                }
            }
            if (sym->ep) {
                struct function *f = malloc(sizeof(struct function));
                f->sym = sym;
                f->children = NULL;
                add_ptr_list(&call_graph, f);

                /* Associate thread with function */
                FOR_EACH_PTR(threads, t) {
                    if (!t->sym && sym->pos.line == t->line && strstr(stream_name(sym->pos.stream), t->file) && !strcmp(sym->ident->name, t->name)) {
                        t->sym = sym;
                        break;
                    }
                } END_FOR_EACH_PTR(t);
                

            }
        } END_FOR_EACH_PTR(sym);
    } END_FOR_EACH_PTR(file);

    FOR_EACH_PTR(threads, t) {
        if (!t->sym) {
            printf("WARNING! Could not locate thread %s. Please check the thread config file.\n", t->name);
        }
    } END_FOR_EACH_PTR(t);

    /* 3. Handle function calls */
    FOR_EACH_PTR(all_syms, sym) {
        if (sym->ep) {
            struct function *f = lookup_func(call_graph, sym);
            if (f)
                find_calls(call_graph, f);
        }
    } END_FOR_EACH_PTR(sym);

    /* 4. Find shared variables */
    FOR_EACH_PTR(threads, t) {
        if (!t->sym)
            continue;

        struct function *f = lookup_func(call_graph, t->sym);
        if (!f)
            continue;

        FOR_EACH_PTR(variables, var) {
            if (find_var_usage_in_thread(f, var->sym)) {
                add_ptr_list(&var->threads, t);
            }
        } END_FOR_EACH_PTR(var);
    } END_FOR_EACH_PTR(t);

    /* 5. Print shared variables */
    FOR_EACH_PTR(variables, var) {
        if (ptr_list_size((struct ptr_list *)var->threads) <= 1)
            continue;

        printf("WARNING! Variable %s declared at %s:%d is used by multiple threads:\n", var->sym->ident->name, stream_name(var->sym->pos.stream), var->sym->pos.line);
        FOR_EACH_PTR(var->threads, t) {
            printf("    %s\n", t->name);
        } END_FOR_EACH_PTR(t);
    } END_FOR_EACH_PTR(var);

    /* 6. Cleanup */
    FOR_EACH_PTR(threads, t) {
        free(t->name);
        free(t->file);
        DELETE_CURRENT_PTR(t);
        free(t);
    } END_FOR_EACH_PTR(t);

    FOR_EACH_PTR(variables, var) {
        DELETE_CURRENT_PTR(var);
        free(var);
    } END_FOR_EACH_PTR(var);

    FOR_EACH_PTR(call_graph, f) {
        DELETE_CURRENT_PTR(f);
        free(f);
    } END_FOR_EACH_PTR(f);

    {
        int i;
        for (i = 0; i < arg_len; ++i)
            free(args[i]);
    }
    free(args);

    return 0;
}

