#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

size_t alignment = sizeof(int);

typedef struct block_t {
    size_t size;
    int used;
    void* addr;
    struct block_t* next;
    struct block_t* prev;
} block_t;

static block_t* heap_start = NULL;

uintptr_t align_number(uintptr_t number, size_t alignment) {
    while (number % alignment != 0) number++;
    return number;
}

block_t* get_alloc_block(block_t* curr, uintptr_t size) {
    while (curr != NULL) {
        if (curr->used == 0 && (uintptr_t)curr->size >= size) return curr;
        curr = curr->next;
    }

    return NULL;
}

void list_printer(block_t* head) {
    block_t* curr = head;
    int counter = 1;

    printf("List: \n");
    while (curr) {
        printf("#%d {\nSize: %d\nUsed: %d \n}\n\n", counter++, curr->size, curr->used);
        curr = curr->next;
    }
}

void* cool_alloc(size_t size) {
    size = align_number(size, alignment);

    if (heap_start == NULL) {
        heap_start = (block_t*)sbrk(sizeof(block_t));
        if (heap_start == (void*)-1) return NULL;

        heap_start->size = size;
        heap_start->used = 1;
        heap_start->addr = sbrk(size);

        if (heap_start->addr == (void*)-1) return NULL;
        heap_start->next = heap_start->prev = NULL;

        return heap_start->addr;
    }

    block_t* avail_block = get_alloc_block(heap_start, (uintptr_t)size);
    if (avail_block == NULL) {
        avail_block = (block_t*)sbrk(sizeof(block_t));
        if (avail_block == (void*)-1) return NULL;

        block_t* temp_addr = heap_start;
        heap_start = avail_block;
        avail_block->next = temp_addr;
        temp_addr->prev = avail_block;

        avail_block->used = 1;
        avail_block->size = size;
        avail_block->addr = sbrk(size);

        if (avail_block->addr == (void*)-1) return NULL;
        return avail_block->addr;
    }

    // If there is a block availeble
    avail_block->used = 1;
    return avail_block->addr;
}

void free_block(void* addr) {
    block_t* curr = heap_start;

    while (curr) {
        if (curr->addr == addr) {
            curr->used = 0;

            if (curr->prev && curr->prev->used == 0) {
                curr->prev->size += curr->size;
                curr->prev->next = curr->next;
                curr = curr->prev;
            }

            if (curr->next && curr->next->used == 0) {
                curr->size += curr->next->size;
                curr->next = curr->next->next;
            }

            return;
        }

        curr = curr->next;
    }
}

int main(int argc, char *argv[]) {
    list_printer(heap_start);

    int* x = cool_alloc(sizeof(int));
    *x = 10;
    printf("The address: %p, the value %d\n", x, *x);
    list_printer(heap_start);

    free_block(x);
    list_printer(heap_start);

    int* y = cool_alloc(sizeof(int));
    *y = 20;

    char* str = cool_alloc(sizeof(char) * 25);
    strcpy(str, "Hola buenos dias");

    list_printer(heap_start);

    free_block(y);
    list_printer(heap_start);

    free_block(str);
    list_printer(heap_start);

    char* new = cool_alloc(sizeof(char) * 40);
    list_printer(heap_start);

    char* nn = cool_alloc(sizeof(char) * 30);
    list_printer(heap_start);
    
    return 0;
}