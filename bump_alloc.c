#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

static void* bump_ptr = NULL;
static uintptr_t init = 0;

uintptr_t align_number(uintptr_t number, size_t alignment) {
    while (number % alignment != 0) number++;
    return number;
}

void* bump_alloc(size_t size) {
    if (bump_ptr == NULL) {
        bump_ptr = sbrk(0);
        init = (uintptr_t)bump_ptr;
    }

    size_t alignment = sizeof(int);

    uintptr_t aligned_addr = (uintptr_t)bump_ptr;

    aligned_addr = align_number(aligned_addr, alignment);
    size = align_number(size, alignment);

    size_t actual_size = (aligned_addr - (uintptr_t)bump_ptr) + size;

    if (sbrk(actual_size) == (void*)-1) {
        return NULL;
    }

    bump_ptr = (void*)(aligned_addr + size);

    return (void*)aligned_addr;
}

void bump_reset() {
    if (bump_ptr == NULL) return;

    size_t allocated_size = (uintptr_t)bump_ptr - init;

    if (sbrk(-allocated_size) == (void*)-1) return;

    bump_ptr = (void*)init;
}

int main() {
    for (int i = 0; i < 10; i++) {
        int* mem = (int*)bump_alloc(sizeof(int));
        *mem = 5;
        printf("Se asignaron %d a la direccion %p\n", sizeof(int), mem);
        printf("%d\n", *mem);
    }

    bump_reset();
    printf("Se supone que la memoria ya fue liberada \n");
    int* mem = (int*)bump_alloc(sizeof(int));
    *mem = 5;
    printf("Se asignaron %d a la direccion %p\n", sizeof(int), mem);

    return 0;
}