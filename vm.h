#ifndef VM_H
#define VM_H

/* Constant pool max size */
#define CPOOL_MAX_SIZE 100

/* Instruction max size */
#define INSTS_MAX_SIZE 200

/* Symbol table max size */
#define SYMBOL_TABLE_MAX_SIZE 200

/* Temporary storage max size */
#define TEMPS_MAX_SIZE 150

/* OPCODE impl max size */
#define OPCODE_IMPL_MAX_SIZE 256

typedef struct {
    enum { INT, STR } type;
    union {
        int vint;
        char *vstr;
    } value;
} vm_value;

#define VM_HANDLER_ARGS vm_value *op1, vm_value *op2, vm_value *result
typedef void (*vm_handler)(VM_HANDLER_ARGS);

typedef struct {
    enum { CONST, TEMP, LABEL } type;
    union {
        int id;
        char *label;
    } value;
} vm_operand;

typedef struct {
    int opcode;
    vm_operand op1;
    vm_operand op2;
    int result;
} vm_inst;

typedef struct {
    int opcode;
    vm_handler handler;
} vm_opcode_impl;

#define VM_T(_op) _op->type
#define VM_INT(_op) _op->value.vint
#define VM_UINT(_op) ((unsigned int) _op->value.vint)


typedef struct {
    char *labels[SYMBOL_TABLE_MAX_SIZE];
    int addresses[SYMBOL_TABLE_MAX_SIZE];
    int count;
} vm_symbol_table;

typedef struct {
    size_t pc;    // program counter.
    size_t sp;    // stack runs from the end of 'temps' region.
    size_t from;  // the immediate PC before last branch/return.
    size_t to;    // the immediate PC after last branch/return.
} vm_regs;

typedef struct {
    vm_inst insts[INSTS_MAX_SIZE];             /* Program instructions */
    vm_value cpool[CPOOL_MAX_SIZE];            /* Constant pool */
    vm_value temps[TEMPS_MAX_SIZE];            /* Temporary storage */
    vm_opcode_impl impl[OPCODE_IMPL_MAX_SIZE]; /* OPCODE impl */
    vm_symbol_table symtab;
    vm_regs r;
    int insts_count;
    int cpool_count;
    int temps_count;
} vm_env;

typedef struct __vm_seg_info vm_seg_info;
struct __vm_seg_info {
    char *mem;  // the pointer to actual memory
    size_t sz;
    vm_seg_info *next;
};

vm_env *vm_new();
void vm_free(vm_env *);

vm_inst *vm_get_inst_by_address(vm_env *env, int addr);

size_t vm_add_const(vm_env *, int, void *);
size_t vm_add_inst(vm_env *, vm_inst);

void vm_set_temp_value(vm_env *env, int pos, int val);

void vm_hook_opcode_handler(vm_env *, int, vm_handler);

void vm_run(vm_env *env);

#endif /* VM_H */
