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
    struct block_t* next_free;
    struct block_t* prev_free;
    struct page_t* page;
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
static block_t* free_head = NULL;

loc_t find_loc(size_t size) {
    if (free_head == NULL) {
        loc_t loc;
        loc.found = 0;
        return loc;
    }

    block_t* curr = free_head;

    while(curr) {
        if (curr->used == 0 && curr->size >= size) {
            loc_t loc;
            loc.page = curr->page;
            loc.block = curr;
            loc.found = 1;
            return loc;
        }
        curr = curr->next_free;
    }

    loc_t loc;
    loc.found = 0;
    return loc;
}

void* mmap_alloc(size_t size) {
    size_t page_size = sysconf(_SC_PAGESIZE);

    size_t size_to_pages = ((size + sizeof(page_t) + sizeof(block_t) + page_size - 1) / page_size) * page_size;

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
        block->page = header;

        next_addr = (void*)((uintptr_t)next_addr + sizeof(block_t) + size);
        block_t* big_free_block = (block_t*)next_addr;
        big_free_block->size = header->free_size;
        big_free_block->used = 0;
        big_free_block->addr = next_addr + sizeof(block_t);
        big_free_block->prev = block;
        big_free_block->page = header;

        free_head = big_free_block;
        free_head->next_free = NULL;
        free_head->prev_free = NULL;
        
        block->next = big_free_block;

        // Why this doesnt work???
        header->free_size -= sizeof(block_t);

        return block->addr;
    }

    loc_t loc = find_loc(size);
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
        big_free_block->page = find_page;
        find_block->next = big_free_block;

        big_free_block->next_free = find_block->next_free;
        big_free_block->prev_free = find_block->prev_free;

        if (find_block->prev_free) {
            find_block->prev_free->next_free = big_free_block;
        } else {
            free_head = big_free_block;
        }

        if (find_block->next_free) {
            find_block->next_free->prev_free = big_free_block;
        }

        return find_block->addr;
    }

    // If there is not found loc
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
    block->page = header;
    header->free_size -= sizeof(block_t) + size;
    header->first_free = block;

    next_addr = (void*)((uintptr_t)next_addr + sizeof(block_t) + size);
    block_t* big_free_block = (block_t*)next_addr;
    big_free_block->size = header->free_size;
    big_free_block->used = 0;
    big_free_block->addr = next_addr + sizeof(block_t);
    big_free_block->prev = block;
    big_free_block->next = NULL;
    big_free_block->page = header;
    block->next = big_free_block;

    big_free_block->prev_free = NULL;
    big_free_block->next_free = free_head;
    free_head->prev_free = big_free_block;
    free_head = big_free_block;

    header->free_size -= sizeof(block_t);

    return block->addr;
}

void mmap_free(void* ptr) {
    page_t* curr = initial_page;
    block_t* curr_block = NULL;

    while (curr) {
        block_t* curr_b = curr->first_free;

        while (curr_b) {
            if (curr_b->addr == ptr) {
                curr_b->used = 0;
                curr->free_size += curr_b->size;

                if (curr_b->prev && curr_b->prev->used == 0) {
                    if (curr_b->prev_free) {
                        curr_b->prev_free->next_free = curr_b->next_free;
                    } else {
                        free_head = curr_b->next_free;
                    }

                    if (curr_b->next_free) {
                        curr_b->next_free->prev_free = curr_b->prev_free;
                    }

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
                    if (curr_b->next->prev_free) {
                        curr_b->next->prev_free->next_free = curr_b->next->next_free;
                    } else {
                        free_head = curr_b->next->next_free;
                    }

                    if (curr_b->next->next_free) {
                        curr_b->next->next_free->prev_free = curr_b->next->prev_free;
                    }

                    curr_b->size += curr_b->next->size;
                    curr_b->next = curr_b->next->next;
                    if (curr_b->next != NULL) {
                        curr_b->next->prev = curr_b;
                    }
                    curr_b->size += sizeof(block_t);
                    curr->free_size += sizeof(block_t);
                }

                curr_b->prev_free = NULL;
                curr_b->next_free = free_head;

                if (free_head) {
                    free_head->prev_free = curr_b;
                }

                curr_block = curr_b;
                free_head = curr_b;

                break;
            }

            curr_b = curr_b->next;
        }

        if (curr->free_size + sizeof(page_t) + sizeof(block_t) == curr->size) {
            if (curr == initial_page) {
                initial_page = curr->next;
            }

            if (curr->prev)
                curr->prev->next = curr->next;

            if (curr->next)
                curr->next->prev = curr->prev;

            // Free list remove block
            if (curr_block->prev_free) {
                curr_block->prev_free->next_free = curr_block->next_free;
            } else {
                free_head = curr_block->next_free;
            }

            if (curr_block->next_free) {
                curr_block->next_free->prev_free = curr_block->prev_free;
            }

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

    mmap_free(x);

    char* hola = mmap_alloc(sizeof(char) * 4000);
    mmap_free(hola);
    char* buenas = mmap_alloc(sizeof(char) * 3900);
    char* adios = mmap_alloc(sizeof(char) * 500);

    int* p1 = mmap_alloc(1024);
    for (int i = 0; i < 256; i++) p1[i] = i;
    mmap_free(p1);

    int* p2 = mmap_alloc(512);
    for (int i = 0; i < 128; i++) p2[i] = i + 1;
    int* p3 = mmap_alloc(256);
    for (int i = 0; i < 64; i++) p3[i] = (i + 1) * 2;

    void* z = mmap_alloc(0);
    mmap_free(NULL);
    if (z) {
        mmap_free(z);
    }

    // Estrés con múltiples asignaciones y liberaciones
    void* arr[10];
    for (int i = 0; i < 10; i++) {
        arr[i] = mmap_alloc(100 * (i + 1));
    }
    for (int i = 0; i < 10; i += 2) {
        mmap_free(arr[i]);
    }
    for (int i = 1; i < 10; i += 2) {
        mmap_free(arr[i]);
    }
    mem_map(initial_page);

    if (initial_page == NULL) return 0;
    printf("FREE LIST: \n");
    block_t* free_curr = free_head;
    int block_counter = 1;

    while (free_curr != NULL) {
        printf("Block #%d, addr: %d\n", block_counter++, (uintptr_t)free_curr);
        printf("Block size: %d, used: %d\n", free_curr->size, free_curr->used);
        free_curr = free_curr->next_free;
    }

    return 0;
}
