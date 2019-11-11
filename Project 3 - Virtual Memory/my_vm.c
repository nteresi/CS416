#include "my_vm.h"

/*
Function responsible for allocating and setting your physical memory 
*/
void set_phys_mem() {

	//Allocate physical memory using mmap or malloc; this is the total size of
	//your memory you are simulating

	//HINT: Also calculate the number of physical and virtual pages and allocate
	//virtual and physical bitmaps and initialize them

	pthread_mutex_init(&mut, NULL);

	while ((phys_mem = mmap(NULL, MEMSIZE, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_ANONYMOUS|MAP_PRIVATE, 0, 0)) == MAP_FAILED) {
	}

	// allocate the outer pagetable on the first page in memory
	set_bit(0);

	offset_bits = log(PGSIZE)/log(2);
	outer_bits = (32-offset_bits)/2;
	inner_bits = 32-offset_bits-outer_bits;

}



/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t * translate(pde_t *pgdir, void *va) {
	//HINT: Get the Page directory index (1st level) Then get the
	//2nd-level-page table index using the virtual address.  Using the page
	//directory index and page table index get the physical address

	int pg_outer = (pte_t)va >> (offset_bits+inner_bits);
	int pg_inner = ((pte_t)va & 4190208) >> offset_bits;
	//int offset = (pte_t)va & 4095;

	pthread_mutex_lock(&mut);
	// inner pagetable not allocated
	pte_t* inner = (pte_t*)pgdir[pg_outer];
	if (!inner) {
		return NULL;
	}

	// page not allocated
	pte_t* pg = (pte_t*)inner[pg_inner];
	if (!pg) {
		return NULL;
	}
	pthread_mutex_lock(&mut);

	return pg;

}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int
page_map(pde_t *pgdir, void *va, void *pa)
{

	/*HINT: Similar to translate(), find the page directory (1st level)
	and page table (2nd-level) indices. If no mapping exists, set the
	virtual to physical mapping */

	int pg_outer = (pte_t)va >> (offset_bits+inner_bits);
	int pg_inner = ((pte_t)va & 4190208) >> offset_bits;
	//int offset = (pte_t)va & 4095;

	// if inner pagetable is not allocated, allocate
	pte_t* inner = (pte_t*)pgdir[pg_outer];
	if (!inner) {
		pgdir[pg_outer] = (pte_t)get_next_avail(NUM_PGS);
	}

	// if page is not allocated, allocate
	pte_t* pg = (pte_t*)inner[pg_inner];
	if (!pg) {
		inner[pg_inner] = (pte_t)pa;
		return 0;
	}

	return -1;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
 
	//Use virtual address bitmap to find the next free page

	// search for <num_pages> contiguous pages

	int x, y, z;
	for (x = 1; x < NUM_PGS; x++) {
		if (!test_bit(x)) {
			for (y = 1; y < num_pages; y++) {
				if (test_bit(x+y)) {
					break;
				}
			}

			if (y == num_pages) {
				break;
			}

		}
	}

	// no <num_pages> continous pages found
	if (x == NUM_PGS) {
		return NULL;
	}

	for (z = x; z < x+num_pages; z++) {
		set_bit(z);
	}

	return phys_mem+(x*PGSIZE);

}


/* Function responsible for allocating pages
and used by the benchmark
*/
void *a_malloc(unsigned int num_bytes) {

	//HINT: If the physical memory is not yet initialized, then allocate and initialize.

	/* HINT: If the page directory is not initialized, then initialize the
	page directory. Next, using get_next_avail(), check if there are free pages. If
	free pages are available, set the bitmaps and map a new page. Note, you will 
	have to mark which physical pages are used. */

	// check if the physical memory is initialized; if not initialize
	if (!phys_mem) {
		set_phys_mem();
	}
	pthread_mutex_lock(&mut);

	pde_t* pgdir = phys_mem;
	
	int num_pages;
	if (num_bytes <= PGSIZE) {
		num_pages = 1;
	} else if (num_bytes % PGSIZE == 0) {
		num_pages = num_bytes/PGSIZE;
	} else {
		num_pages = (num_bytes/PGSIZE)+1;
	}

	pte_t* inner;

	int pg_outer, pg_inner;

	// find first allocated inner pagetable
	for (pg_outer = 0; pg_outer < PGSIZE/4; pg_outer++) {
		if (pgdir[pg_outer]) {
			inner = (pte_t*)pgdir[pg_outer];
			
			// find first unallocated page in inner pagetable and allocate it
			for (pg_inner = 0; pg_inner < PGSIZE/4; pg_inner++) {
				if (!inner[pg_inner]) {
					inner[pg_inner] = (pte_t)get_next_avail(num_pages);
					break;
				}
			}

			// if the inner pagetable is full, go to next inner pagetable
			if (pg_inner < PGSIZE/4) {
				break;
			}
		}
	}

	// if no inner pagetables have been allocated yet, then allocate inner pagetable and its first page
	if (pg_outer == PGSIZE/4) {
		pg_outer = 0;
		pg_inner = 0;
		pgdir[0] = (pte_t)get_next_avail(1);
		((pte_t*)pgdir[0])[0] = (pte_t)get_next_avail(num_pages);
		pthread_mutex_unlock(&mut);
		return (void*)0;
	}

	pte_t addr = (pg_outer << (offset_bits+inner_bits)) + (pg_inner << offset_bits);

	pthread_mutex_unlock(&mut);
	return (void*)addr;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void a_free(void *va, int size) {

	//Free the page table entries starting from this virtual address (va)
	// Also mark the pages free in the bitmap
	//Only free if the memory from "va" to va+size is valid
	
	pthread_mutex_lock(&mut);

	int num_pages;
	if (size <= PGSIZE) {
		num_pages = 1;
	} else if (size % PGSIZE == 0) {
		num_pages = size/PGSIZE;
	} else {
		num_pages = (size/PGSIZE)+1;
	}

	pte_t* addr = translate(phys_mem, va);

	int bit_idx = ((pte_t)addr-(pte_t)phys_mem)/PGSIZE;

	int x;
	int valid = 0;
	for (x = 0; x < num_pages; x++) {
		if (test_bit(bit_idx+x)) {
			clear_bit(bit_idx+x);
		}
	}

	pthread_mutex_unlock(&mut);
}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
*/
void put_value(void *va, void *val, int size) {

	/* HINT: Using the virtual address and translate(), find the physical page. Copy
	the contents of "val" to a physical page. NOTE: The "size" value can be larger
	than one page. Therefore, you may have to find multiple pages using translate()
	function.*/

	pthread_mutex_lock(&mut);

	pte_t* addr = translate(phys_mem, va);
	memcpy(addr, val, size);

	pthread_mutex_unlock(&mut);

}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

	/* HINT: put the values pointed to by "va" inside the physical memory at given
	"val" address. Assume you can access "val" directly by derefencing them.
	If you are implementing TLB,  always check first the presence of translation
	in TLB before proceeding forward */

	pthread_mutex_lock(&mut);

	pte_t* addr = translate(phys_mem, va);
	memcpy(val, addr, size);

	pthread_mutex_unlock(&mut);
}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer) {

	/* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
	matrix accessed. Similar to the code in test.c, you will use get_value() to
	load each element and perform multiplication. Take a look at test.c! In addition to 
	getting the values from two matrices, you will perform multiplication and 
	store the result to the "answer array"*/

	pthread_mutex_lock(&mut);

	int a, b, x, y, z, result;
	pte_t address_a, address_b, address_c;

	for (x = 0; x < size; x++) {
		for (y = 0; y < size; y++) {
			result = 0;
			for (z = 0; z < size; z++) {
				address_a = (pte_t)mat1 + ((x * size * sizeof(int))) + (z * sizeof(int));
				address_b = (pte_t)mat2 + ((z * size * sizeof(int))) + (y * sizeof(int));
				get_value((void *)address_a, &a, sizeof(int));
				get_value((void *)address_b, &b, sizeof(int));
				result += a*b;
			}
			address_c = (pte_t)answer + ((x * size * sizeof(int))) + (y * sizeof(int));
			put_value((void*)address_c, &result, sizeof(int));
		}
	}

	pthread_mutex_lock(&mut);

}

/* Set bit to 1 in bitvector
*/
void set_bit(int pg) {
	
	int idx = pg/32;
	int pos = pg%32;

	bitvec[idx] = bitvec[idx] | (1 << pos);
	
}

/* Set bit to 0 in bitvector
*/
void clear_bit(int pg) {

	int idx = pg/32;
	int pos = pg%32;

	bitvec[idx] = bitvec[idx] & ~(1 << pos);
}

/* Check if given bit is a 1 (return 1) or 0 (return 0)
*/
int test_bit(int pg) {

	int idx = pg/32;
	int pos = pg%32;

	if (bitvec[idx] & (1 << pos)) {
		return 1;
	}
	return 0;

}
