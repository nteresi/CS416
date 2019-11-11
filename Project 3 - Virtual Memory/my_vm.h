#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>

//Assume the address space is 32 bits, so the max memory size is 4GB
//Page size is 4KB

//Add any important includes here which you may need
#include <sys/mman.h>
#include <math.h>
#include <string.h>
#include <pthread.h>

#define PGSIZE 4096
#define MAX_MEMSIZE 4UL*1024*1024*1024
#define MEMSIZE 1024*1024*1024
//#define TLB_SIZE 

#define NUM_PGS (MEMSIZE/PGSIZE)

typedef unsigned long pte_t;
typedef unsigned long pde_t;

void* phys_mem = NULL;
unsigned long bitvec[NUM_PGS/32];

int outer_bits;
int inner_bits;
int offset_bits;

pthread_mutex_t mut;

void set_physical_mem();
pte_t* translate(pde_t *pgdir, void *va);
int page_map(pde_t *pgdir, void *va, void* pa);
void* get_next_avail(int num_pages);
bool check_in_tlb(void *va);
void put_in_tlb(void *va, void *pa);
void *a_malloc(unsigned int num_bytes);
void a_free(void *va, int size);
void put_value(void *va, void *val, int size);
void get_value(void *va, void *val, int size);
void mat_mult(void *mat1, void *mat2, int size, void *answer);

void set_bit(int pg);
void clear_bit(int pg);
int test_bit(int pg);

#endif
