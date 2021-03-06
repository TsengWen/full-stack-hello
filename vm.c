#include <stdlib.h>
#include <string.h>

#include "vm.h"
#include "vm_codegen.h"
#include "opcode.h"

#if !defined(__GNUC__)
#error "Only gcc is supported at present"
#endif

#define OPCODE env->insts[env->r.pc]
#define OPCODE_IMPL(inst) env->impl[inst.opcode]
#define HANDLER OPCODE.handler

#define OP(name) OP_##name

#define BEGIN_OPCODES                          \
    const static void *labels[] = {OP_LABELS}; \
    goto *labels[OPCODE.opcode]

#define DISPATCH                     \
    do {                             \
        ++env->r.pc;                 \
        goto *labels[OPCODE.opcode]; \
    } while (0)

#define GOTO(n)                      \
    do {                             \
        env->r.from = env->r.pc;     \
        env->r.to = n;               \
        env->r.pc = n;               \
        goto *labels[OPCODE.opcode]; \
    } while (0)

#define END_OPCODES

static inline void vm_push(vm_env *env, size_t n);

#define VM_CALL(n)               \
    do {                         \
        vm_push(env, env->r.pc); \
        GOTO(n);                 \
    } while (0)

#define VM_RET()                     \
    do {                             \
        size_t pc = vm_pop(env) + 1; \
        GOTO(pc);                    \
    } while (0)

#define VM_J_TYPE_INST(cond)                                     \
    do {                                                         \
        int gle = vm_get_op_value(env, &OPCODE.op1)->value.vint; \
        if (gle cond 0)                                          \
            GOTO(OPCODE.op2.value.id);                           \
        DISPATCH;                                                \
    } while (0)

#define VM_JLT() VM_J_TYPE_INST(<)
#define VM_JLE() VM_J_TYPE_INST(<=)
#define VM_JZ() VM_J_TYPE_INST(==)
#define VM_JGE() VM_J_TYPE_INST(>=)
#define VM_JGT() VM_J_TYPE_INST(>)
#define VM_JNZ() VM_J_TYPE_INST(!=)

#define VM_CALL_HANDLER()                                        \
    do {                                                         \
        if (OPCODE_IMPL(OPCODE).handler)                         \
            OPCODE_IMPL(OPCODE)                                  \
                .handler(vm_get_op_value(env, &OPCODE.op1),      \
                         vm_get_op_value(env, &OPCODE.op2),      \
                         vm_get_temp_value(env, OPCODE.result)); \
        DISPATCH;                                                \
    } while (0)

vm_env *vm_new()
{
    vm_env *env = malloc(sizeof(vm_env));
    memset(env, 0, sizeof(vm_env));
    env->symtab.count = 0;
    env->r.sp = TEMPS_MAX_SIZE;  // stack runs from the end of 'temps' region.
    return env;
}

void vm_free(vm_env *env)
{
    free(env);
}

vm_inst *vm_get_inst_by_address(vm_env *env, int addr)
{
    if (addr >= env->insts_count)
        return NULL;
    return &env->insts[addr];
}

size_t vm_add_const(vm_env *env, int type, void *value)
{
    env->cpool[env->cpool_count] = (vm_value){.type = type};

    switch (type) {
    case INT:
        env->cpool[env->cpool_count].value.vint = *(int *) value;
        break;
    case STR:
        env->cpool[env->cpool_count].value.vstr = (char *) value;
        break;
    }

    return env->cpool_count++;
}

size_t vm_add_inst(vm_env *env, vm_inst inst)
{
    env->insts[env->insts_count] = inst;

    return env->insts_count++;
}

void vm_set_temp_value(vm_env *env, int pos, int val)
{
    vm_value temp_v = {.type = INT};
    temp_v.value.vint = val;
    env->temps[pos] = temp_v;
}

static inline void vm_push(vm_env *env, size_t n)
{
    env->r.sp--;
    env->temps[env->r.sp].type = INT;
    env->temps[env->r.sp].value.vint = (int) n;
}

static inline size_t vm_pop(vm_env *env)
{
    size_t n = (size_t) env->temps[env->r.sp].value.vint;
    env->r.sp++;
    return n;
}

void vm_hook_opcode_handler(vm_env *env, int opcode, vm_handler handler)
{
    env->impl[opcode].opcode = opcode;
    env->impl[opcode].handler = handler;
}

static inline size_t vm_get_temp(vm_env *env)
{
    return env->temps_count++;
}

static inline vm_value *vm_get_temp_value(vm_env *env, int id)
{
    return &env->temps[id];
}

static inline vm_value *vm_get_op_value(vm_env *env, const vm_operand *op)
{
    switch (VM_T(op)) {
    case CONST:
        return &env->cpool[op->value.id];
    case TEMP:
        return &env->temps[op->value.id];
    default:
        break;
    }

    return NULL;
}

void vm_run(vm_env *env)
{
    BEGIN_OPCODES;

    OP(ADD) : VM_CALL_HANDLER();
    OP(SUB) : VM_CALL_HANDLER();
    OP(MUL) : VM_CALL_HANDLER();
    OP(DIV) : VM_CALL_HANDLER();
    OP(MOD) : VM_CALL_HANDLER();
    OP(AND) : VM_CALL_HANDLER();
    OP(OR) : VM_CALL_HANDLER();
    OP(NOT) : VM_CALL_HANDLER();
    OP(XOR) : VM_CALL_HANDLER();
    OP(LSL) : VM_CALL_HANDLER();
    OP(LSR) : VM_CALL_HANDLER();
    OP(ASR) : VM_CALL_HANDLER();
    OP(PRINT) : VM_CALL_HANDLER();
    OP(JLT) : VM_JLT();
    OP(JLE) : VM_JLE();
    OP(JZ) : VM_JZ();
    OP(JGE) : VM_JGE();
    OP(JGT) : VM_JGT();
    OP(JNZ) : VM_JNZ();
    OP(JMP) : GOTO(OPCODE.op1.value.id);
    OP(CALL) : VM_CALL(OPCODE.op1.value.id);
    OP(RET) : VM_RET();

    OP(HALT) : goto terminate;

    END_OPCODES;
terminate:
    return;
}

static int vm_insts_seg_inflate(vm_env *env, char *mem, size_t sz)
{
    memset(&env->insts[0], 0, sizeof(env->insts));
    memcpy(&env->insts[0], mem, sz);
    return env->insts_count = sz / sizeof(vm_inst);
}

static int vm_insts_seg_deflate(vm_env *env, char **mem, size_t *sz)
{
    *sz = env->insts_count * sizeof(vm_inst);
    *mem = malloc(*sz);

    memcpy(*mem, (char *) &env->insts[0], *sz);

    return *sz;
}

static int vm_cpool_seg_inflate(vm_env *env, char *mem, size_t sz)
{
    vm_value *src, *dst;
    char *ptr;
    char *end;
    int i = 0;

    memset(&env->cpool[0], 0, sizeof(env->cpool));

    src = (vm_value *) mem;
    dst = &env->cpool[0];
    end = mem + sz;

    while ((char *) src < end) {
        memcpy(dst, src, sizeof(*dst));

        if (src->type == STR) {
            ptr = &mem[(long) src->value.vstr];

            dst->value.vstr = strdup(ptr);

            if (ptr < end)
                end = ptr;
        }

        dst++;
        src++;
        i++;
    }

    return env->cpool_count = i++;
}

static int vm_cpool_seg_deflate(vm_env *env, char **mem, size_t *sz)
{
    vm_value *src, *dst;
    char *ptr;

    *sz = env->cpool_count * sizeof(vm_value);

    for (int i = 0; i < env->cpool_count; i++)
        if (env->cpool[i].type == STR)
            *sz += strlen(env->cpool[i].value.vstr) + 1;

    *mem = malloc(*sz);
    memcpy(*mem, &env->cpool[0], env->cpool_count * sizeof(vm_value));

    dst = (vm_value *) *mem;
    src = (vm_value *) &env->cpool[0];
    ptr = &((*mem)[env->cpool_count * sizeof(vm_value)]);

    for (int i = 0; i < env->cpool_count; i++) {
        if (src[i].type == STR) {
            dst[i].value.vint = ptr - *mem;  // use vint to avoid casting
            strcpy(ptr, src[i].value.vstr);
            ptr += strlen(src[i].value.vstr) + 1;
        }
    }

    return *sz;
}

static int vm_temps_seg_inflate(vm_env *env, char *mem, size_t sz)
{
    memset(&env->temps[0], 0, sizeof(env->temps));
    memcpy(&env->temps[0], mem, sz);
    return env->temps_count = sz / sizeof(vm_value);
}

static int vm_temps_seg_deflate(vm_env *env, char **mem, size_t *sz)
{
    *sz = env->temps_count * sizeof(vm_value);
    *mem = malloc(*sz);

    memcpy(*mem, (char *) &env->temps[0], *sz);

    return *sz;
}

vm_seg_info *vm_new_seg_info(char *mem, size_t sz)
{
    vm_seg_info *p = calloc(sizeof(vm_seg_info), 1);

    p->mem = mem;
    p->sz = sz;

    return p;
}

vm_seg_info *vm_get_next_seg_info(vm_seg_info *info)
{
    return (!info) ? NULL : info->next;
}

void vm_append_seg_info(vm_seg_info *head, vm_seg_info *info)
{
    while (head->next)
        head = head->next;
    head->next = info;
}

size_t vm_get_seg_size(vm_seg_info *info)
{
    return info->sz;
}

void *vm_get_seg_mem(vm_seg_info *info)
{
    return info->mem;
}

unsigned short vm_get_seg_info(vm_env *env, vm_seg_info **seg_info)
{
    vm_seg_info *p = *seg_info = vm_new_seg_info(NULL, 0);

    vm_insts_seg_deflate(env, &p->mem, &p->sz);
    p->next = calloc(sizeof(*p), 1);
    p = p->next;

    vm_cpool_seg_deflate(env, &p->mem, &p->sz);
    p->next = calloc(sizeof(*p), 1);
    p = p->next;

    vm_temps_seg_deflate(env, &p->mem, &p->sz);
    p->next = NULL;

    return 3;
}

void vm_free_seg_info(vm_seg_info *seg_info)
{
    vm_seg_info *p;

    while (seg_info) {
        p = seg_info->next;
        free(seg_info->mem);
        free(seg_info);
        seg_info = p;
    }
}

int vm_set_seg_info(vm_env *env, const vm_seg_info *seg_info)
{
    int i = 0;

    while (seg_info) {
        switch (i++) {
        case 0:
            vm_insts_seg_inflate(env, seg_info->mem, seg_info->sz);
            break;
        case 1:
            vm_cpool_seg_inflate(env, seg_info->mem, seg_info->sz);
            break;
        case 2:
            vm_temps_seg_inflate(env, seg_info->mem, seg_info->sz);
            break;
        default:
            break;
        }
        seg_info = seg_info->next;
    }

    return i;
}

void vm_seg_info_free_list(vm_seg_info *p)
{
    while (p) {
        void *q = p->next;
        free(p);
        p = q;
    }
}
