#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#define ALIGNMENT sizeof(int)

typedef struct block_t {
    size_t size;
    int used;
    void* addr;
    struct block_t* next;
    struct block_t* prev;
} block_t;

typedef struct page_t {
    size_t size;
    size_t free_size;
    block_t* first_free;
    struct page_t* next;
    struct page_t* prev;
} page_t;

typedef struct {
    page_t* page;
    block_t* block;
    int found;
} loc_t;

static inline uintptr_t align_up(uintptr_t addr, size_t alignment) {
    if ((alignment & (alignment - 1)) != 0) return 0;

    return (addr + (alignment - 1)) & ~(alignment - 1);
}

static page_t* initial_page = NULL;

loc_t find_loc(page_t* init, size_t size) {
    page_t* curr = init;

    while (curr) {
        if (curr->free_size < size) {
            curr = curr->next;
            continue;
        }

        block_t* curr_block = curr->first_free;
        while (curr_block) {
            if (curr_block->used == 0 && curr_block->size >= size) {
                loc_t loc;
                loc.page = curr;
                loc.block = curr_block;
                loc.found = 1;
                return loc;
            }
            curr_block = curr_block->next;
        }

        curr = curr->next;
    }

    loc_t loc;
    loc.found = 0;
    return loc;
}

void* mmap_alloc(size_t size) {
    size_t page_size = sysconf(_SC_PAGESIZE);

    // adding the page_t size (40) and block_t (40) size that is required for allocations
    size_t size_to_pages = ((size + 80 + page_size - 1) / page_size) * page_size;

    size = align_up(size, ALIGNMENT);

    if (initial_page == NULL) {
        void* addr = mmap(NULL, size_to_pages, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (addr == MAP_FAILED) {
            perror("Error al asignar la memoria");
            return NULL;
        }

        page_t* header = (page_t*)addr;
        initial_page = header;
        header->size = size_to_pages;
        header->free_size = header->size - sizeof(page_t);
        header->first_free = NULL;
        header->prev = NULL;
        header->next = NULL;

        void* next_addr = (void*)((uintptr_t)addr + sizeof(page_t));
        block_t* block = (block_t*)next_addr;
        block->size = size;
        block->used = 1;
        block->addr = next_addr + sizeof(block_t);
        block->prev = NULL;
        header->free_size -= sizeof(block_t) + size;
        header->first_free = block;

        next_addr = (void*)((uintptr_t)next_addr + sizeof(block_t) + size);
        block_t* big_free_block = (block_t*)next_addr;
        big_free_block->size = header->free_size;
        big_free_block->used = 0;
        big_free_block->addr = next_addr + sizeof(block_t);
        big_free_block->prev = block;
        big_free_block->next = NULL;
        block->next = big_free_block;

        // Why this doesnt work???
        header->free_size -= sizeof(block_t);

        return block->addr;
    }

    loc_t loc = find_loc(initial_page, size);
    if (loc.found == 1) {
        page_t* find_page = loc.page;
        block_t* find_block = loc.block;

        find_block->used = 1;
        find_block->size = size;

        // Size of the allocated size + the big_free_block size
        find_page->free_size -= sizeof(block_t) + size;

        void* next_addr = (void*)((uintptr_t)find_block->addr + find_block->size);

        block_t* big_free_block = (block_t*)next_addr;
        big_free_block->size = find_page->free_size;
        big_free_block->used = 0;
        big_free_block->addr = next_addr + sizeof(block_t);
        big_free_block->prev = find_block;
        big_free_block->next = NULL;
        find_block->next = big_free_block;

        return find_block->addr;
    }

    // There is not found loc
    void* addr = mmap(NULL, size_to_pages, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (addr == MAP_FAILED) {
        perror("Error al asignar la memoria");
        return NULL;
    }

    page_t* header = (page_t*)addr;
    header->size = size_to_pages;
    header->free_size = header->size - sizeof(page_t);
    header->first_free = NULL;

    header->prev = NULL;
    page_t* temp = initial_page;
    initial_page = header;
    header->next = temp;
    temp->prev = initial_page;

    void* next_addr = (void*)((uintptr_t)addr + sizeof(page_t));
    block_t* block = (block_t*)next_addr;
    block->size = size;
    block->used = 1;
    block->addr = next_addr + sizeof(block_t);
    block->prev = NULL;
    header->free_size -= sizeof(block_t) + size;
    header->first_free = block;

    next_addr = (void*)((uintptr_t)next_addr + sizeof(block_t) + size);
    block_t* big_free_block = (block_t*)next_addr;
    big_free_block->size = header->free_size;
    big_free_block->used = 0;
    big_free_block->addr = next_addr + sizeof(block_t);
    big_free_block->prev = block;
    big_free_block->next = NULL;
    block->next = big_free_block;

    header->free_size -= sizeof(block_t);

    return block->addr;
}

void mmap_free(void* ptr) {
    page_t* curr = initial_page;
    while (curr) {
        block_t* curr_b = curr->first_free;

        while (curr_b) {
            if (curr_b->addr == ptr) {
                curr_b->used = 0;
                curr->free_size += curr_b->size;

                if (curr_b->prev && curr_b->prev->used == 0) {
                    curr_b->prev->size += curr_b->size;
                    curr_b->prev->next = curr_b->next;
                    if (curr_b->next != NULL) {
                        curr_b->next->prev = curr_b->prev;
                    }
                    curr_b = curr_b->prev;
                    curr_b->size += sizeof(block_t);
                    curr->free_size += sizeof(block_t);
                }

                if (curr_b->next && curr_b->next->used == 0) {
                    curr_b->size += curr_b->next->size;
                    curr_b->next = curr_b->next->next;
                    if (curr_b->next != NULL) {
                        curr_b->next->prev = curr_b;
                    }
                    curr_b->size += sizeof(block_t);
                    curr->free_size += sizeof(block_t);
                }

                break;
            }

            curr_b = curr_b->next;
        }

        // adding the page_t and block_t (40 + 40);
        if (curr->free_size + 80 == curr->size) {
            if (curr->prev)
                curr->prev->next = curr->next;

            if (curr->next)
                curr->next->prev = curr->prev;

            if (curr == initial_page)
                initial_page = curr->next;

            if (munmap(curr, curr->size) == -1) return;

            return;
        }
        curr = curr->next;
    }
}

void mem_map(page_t* init) {
    page_t* curr = init;
    int page_counter = 1;

    while (curr) {
        int block_counter = 1;
        block_t* curr_b = curr->first_free;

        printf("Page #%d, addr: %d\n", page_counter++, (uintptr_t)curr);
        printf("Page size: %d, page free size: %d\n", curr->size, curr->free_size);

        while (curr_b) {
            printf("Block #%d, addr: %d\n", block_counter++, (uintptr_t)curr_b);
            printf("Block size: %d, used: %d\n", curr_b->size, curr_b->used);
            curr_b = curr_b->next;
        }
        curr = curr->next;
        printf("\n");
    }
}

int main(int argc, char *argv[]) {
    int* x = mmap_alloc(sizeof(int));
    *x = 100;

    int* y = mmap_alloc(sizeof(int));
    *y = 10;

    char* s = mmap_alloc(sizeof(char) * 2000);
    mmap_free(y);

    char* z = mmap_alloc(sizeof(int) * 10);
    mmap_free(x);

    char* m = mmap_alloc(sizeof(char) * 4096);

    mmap_free(s);
    mmap_free(m);
    mmap_free(z);

    mem_map(initial_page);
    
    // char* y = mmap_alloc(sizeof(int) * 400);
    // char* x = mmap_alloc(sizeof(char) * 2300);
    //
    // mmap_free(y);
    // mmap_free(x);
    //
    // mem_map(initial_page);

    return 0;
}