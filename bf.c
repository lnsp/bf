#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define VERBOSE_ARG "-v"
#define VERBOSE_ARG_LONG "--verbose"
#define INPUT_ARG "-i"
#define INPUT_ARG_LONG "--input"
#define OUTPUT_ARG "-o"
#define OUTPUT_ARG_LONG "--output"

#define WORD_SIZE 0x80
#define BUFFER_FACTOR 0x2
#define BUFFER_START_SIZE 0xF
#define OUTPUT_MODE "%c"
#define INPUT_EOF 0x0

#define ERR_INPUT_FN "error: failed to open input file"
#define ERR_OUTPUT_FN "error: failed to open output file"
#define ERR_PROGRAM_FN "error: failed to open program file"
#define ERR_MEMORY_RESIZE "error: failed to resize memory"
#define ERR_MEMORY_OVERFLOW "error: memory overflow"
#define ERR_BAD_BRACKETS "error: no matching bracket found"

#define V_MEMORY_RESIZE "memory: resized program to %zu\n"
#define V_MEMORY_SIZE "memory: program size is %zu\n"
#define V_INPUT_FN "load: input source is %s\n"
#define V_OUTPUT_FN "load: output target is %s\n"
#define V_PROGRAM_FN "load: program is %s\n"

#define V_OP_INCR_PTR "eval: increase pointer by one\n"
#define V_OP_DECR_PTR "eval: decrease pointer by one\n"
#define V_OP_INCR_DATA "eval: increase storage by one to %d\n"
#define V_OP_DECR_DATA "eval: decrease storage by one to %d\n"
#define V_OP_INPUT "eval: read input %c\n"
#define V_OP_OUTPUT "eval: write output %c\n"
#define V_OP_LOOP_JMP "eval: jump to %zu\n"
#define V_OP_LOOP_START "eval: start loop\n"
#define V_OP_LOOP_END "eval: end loop\n"

#define OP_EXIT 0x0
#define OP_INCR_PTR 0x1
#define OP_DECR_PTR 0x2
#define OP_INCR_DATA 0x3
#define OP_DECR_DATA 0x4
#define OP_OUTPUT 0x5
#define OP_INPUT 0x6
#define OP_LOOP_START 0x7
#define OP_LOOP_END 0x8

const char OPERATION[WORD_SIZE] = {
    ['>'] = OP_INCR_PTR, // Move to next storage
    ['<'] = OP_DECR_PTR, // Move to previous storage
    ['+'] = OP_INCR_DATA, // Increment data in storage
    ['-'] = OP_DECR_DATA, // Decrenebt data in storage
    ['.'] = OP_OUTPUT, // Output the storage value
    [','] = OP_INPUT, // Read from input into storage
    ['['] = OP_LOOP_START, // If storage is zero continue, else jump to end
    [']'] = OP_LOOP_END, // If storage is nonzero move to match, else continue
};

FILE* input_file = NULL; // The input file pointer
FILE* output_file = NULL; // The output file pointer
FILE* program_file = NULL; // The source code file pointer

char* input_fn = ""; // The input file name
char* output_fn = ""; // The output file name
char* program_fn = ""; // The source file name
bool verbose_mode = false; // Verbose mode (more logging info)

/**
 * Stack structure used for brackets matching.
 */
typedef struct stack_item {
    struct stack_item* next;
    size_t value;
} stack_item;

/**
 * Pushes a new item onto the stack.
 */
stack_item* push(stack_item* base, size_t value) {
    stack_item* n = malloc(sizeof(stack_item));
    if (n == NULL) {
        puts(ERR_MEMORY_OVERFLOW);
        exit(EXIT_FAILURE);
    }
    n->value = value;
    n->next = base;
    return n;
}

/**
 Returns the value on top of the stack.
 */
size_t peek(stack_item* base) {
    return base->value;
}

/**
 * Pops an item from the stack and returns the new base.
 */
stack_item* pop(stack_item* base) {
    return base->next;
}

/**
 * Double-linked list structure used for infinite-large storage.
 */
typedef struct storage {
    struct storage* next;
    struct storage* prev;
    int value;
} storage;

/**
 * Allocates memory for a new storage item.
 */
storage* new_storage(int value, storage* prev, storage* next) {
    storage* n = malloc(sizeof(storage));
    if (n == NULL) {
        puts(ERR_MEMORY_OVERFLOW);
        exit(EXIT_FAILURE);
    }

    n->value = value;
    n->prev = prev;
    n->next = next;
    return n;
}

/**
 * Moves the pointer to the previous storage.
 */
storage* previous(storage* item) {
    if (item->prev == NULL) {
        storage* p = new_storage(0, NULL, item);
        item->prev = p;
    }
    return item->prev;
}

/**
 * Moves the pointer to the next storage.
 */
storage* next(storage* item) {
    if (item->next == NULL) {
        storage* n = new_storage(0, item, NULL);
        item->next = n;
    }
    return item->next;
}

/**
 * Parses the command line arguments and
 */
void parse_args(int argc, char* argv[]) {
    bool is_input = false,
         is_output = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], INPUT_ARG) == 0) {
            is_input = true;
        } else if (strcmp(argv[i], OUTPUT_ARG) == 0) {
            is_output = true;
        } else if (strcmp(argv[i], VERBOSE_ARG) == 0) {
            verbose_mode = true;
        } else {
            if (is_input) {
                input_fn = argv[i];
                is_input = false;
            } else if (is_output) {
                output_fn = argv[i];
                is_output = false;
            } else {
                program_fn = argv[i];
            }
        }
    }
}

/**
 * loads the program.
 */
char* load() {
    // Check if input file exists and load it
    if (strcmp(input_fn, "") != 0) {
        if (verbose_mode) {
            printf(V_INPUT_FN, input_fn);
        }
        input_file = fopen(input_fn, "r");
        if (input_file == NULL) {
            puts(ERR_INPUT_FN);
            exit(EXIT_FAILURE);
        }
    }

    // Check if the output file exists and load it
    if (strcmp(output_fn, "") != 0) {
        if (verbose_mode) {
            printf(V_OUTPUT_FN, output_fn);
        }
        output_file = fopen(output_fn, "w");
        if (output_file == NULL) {
            puts(ERR_OUTPUT_FN);
            exit(EXIT_FAILURE);
        }
    }

    // Check if the program file exists and load it
    if (verbose_mode) {
        printf(V_PROGRAM_FN, program_fn);
    }
    program_file = fopen(program_fn, "r");
    if (program_file == NULL) {
        puts(ERR_PROGRAM_FN);
        exit(EXIT_FAILURE);
    }

    // Load program file into char array, dynamically resized
    size_t real_size = 0, prog_size = BUFFER_START_SIZE;
    char* program = malloc(sizeof(char) * prog_size);
    int c = 0;

    while ((c = fgetc(program_file)) != EOF) {
        if (real_size >= prog_size - 1) {
            prog_size *= BUFFER_FACTOR; // Enlarge buffer
            program = realloc(program, prog_size);
            if (program == NULL) {
                puts(ERR_MEMORY_RESIZE);
                exit(EXIT_FAILURE);
            }
            if (verbose_mode) {
                printf(V_MEMORY_RESIZE, prog_size);
            }
        }
        char code = OPERATION[c];
        if (code != OP_EXIT && c < WORD_SIZE) {
            if (verbose_mode)  {
                printf("%c", c);
            }
            program[real_size++] = code;
        }
    }
    if (verbose_mode) puts("");
    fclose(program_file);

    if (verbose_mode) {
        printf(V_MEMORY_SIZE, prog_size);
    }

    program[real_size] = OP_EXIT;
    return program;
}

/**
 * Runs the program.
 */
void eval(char* program) {
    // Initialize machine storage
    storage* store = new_storage(0, NULL, NULL);
    stack_item* bracket = NULL;
    size_t op = 0;

    while (program[op] != OP_EXIT) {
        //printf("Executing %u at %zu\n", program[op], op);
        switch (program[op]) {
            case OP_INCR_PTR:
                store = next(store);
                if (verbose_mode) printf(V_OP_INCR_PTR);
                break;
            case OP_DECR_PTR:
                store = previous(store);
                if (verbose_mode) printf(V_OP_DECR_PTR);
                break;
            case OP_INCR_DATA:
                store->value++;
                if (verbose_mode) printf(V_OP_INCR_DATA, store->value);
                break;
            case OP_DECR_DATA:
                store->value--;
                if (verbose_mode) printf(V_OP_DECR_DATA, store->value);
                break;
            case OP_OUTPUT:
                printf(OUTPUT_MODE, store->value);
                if (verbose_mode) printf(V_OP_INPUT, store->value);
                break;
            case OP_INPUT:
                if (input_file != NULL) {
                    store->value = fgetc(input_file);
                } else {
                    store->value = getchar();
                }
                if (store->value == EOF) store->value = INPUT_EOF;
                if (verbose_mode) printf(V_OP_OUTPUT, store->value);
                break;
            case OP_LOOP_START:
                if (store->value == 0) {
                    // Search for matching bracket
                    int level = 1;
                    while (level > 0) {
                        op++;
                        if (program[op] == OP_LOOP_START) {
                            level++;
                        } else if (program[op] == OP_LOOP_END) {
                            level--;
                        } else if (program[op] == OP_EXIT) {
                            puts(ERR_BAD_BRACKETS);
                            exit(EXIT_FAILURE);
                        }
                    }
                    if (verbose_mode) printf(V_OP_LOOP_JMP, op);
                } else {
                    // Push bracket onto stack, jump into loop
                    bracket = push(bracket, op);
                    if (verbose_mode) printf(V_OP_LOOP_START);
                }
                break;
            case OP_LOOP_END:
                if (store->value != 0) {
                    // Jump back to bracket
                    size_t index = peek(bracket);
                    if (index == -1) {
                        puts(ERR_BAD_BRACKETS);
                        exit(EXIT_FAILURE);
                    }
                    op = index;
                    if (verbose_mode) printf(V_OP_LOOP_JMP, op);
                } else {
                    bracket = pop(bracket);
                    if (verbose_mode) printf(V_OP_LOOP_END);
                }
                break;
        }
        op++;
    }
}

void close(char* program) {
    if (input_file != NULL) fclose(input_file);
    if (output_file != NULL) fclose(output_file);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    parse_args(argc, argv);
    char* program = load();
    eval(program);
    close(program);

    return EXIT_SUCCESS;
}
