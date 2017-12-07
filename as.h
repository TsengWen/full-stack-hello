#ifndef AS_H
#define AS_H

#include "vm.h"

void assemble_from_fd(vm_env *env, int fd);

int write_to_elf(vm_env *env, int fd);

int load_from_elf(vm_env *env, int fd);

void insert_symtab(vm_env *env, char *label, int address);
int search_symtab(vm_env *env, char *label);
void set_addresses(vm_env *env);

#endif /* AS_H */
