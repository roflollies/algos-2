/* * * * * * * * *
 * Dynamic hash table using cuckoo hashing, resolving collisions by switching
 * keys between two tables with two separate hash functions
 *
 * created for COMP20007 Design of Algorithms - Assignment 2, 2017
 * by ...
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


#include "cuckoo.h"

#include <windows.h>
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define RESET   "\x1b[0m"

#define EMPTY 0

// an inner table represents one of the two internal tables for a cuckoo
// hash table. it stores two parallel arrays: 'slots' for storing keys and
// 'inuse' for marking which entries are occupied
typedef struct inner_table {
	int64 *slots;	// array of slots holding keys
	bool  *inuse;	// is this slot in use or not?
} InnerTable;

// a cuckoo hash table stores its keys in two inner tables
struct cuckoo_table {
	InnerTable *table1; // first table
	InnerTable *table2; // second table
	int size;			// size of each table
};

void upsize_table(CuckooHashTable *table, int size);
void upsize_inner(InnerTable *table, int size);
InnerTable *new_inner_table(int size);
//void upsize_table(CuckooHashTable *table, int size);
bool try_insert(CuckooHashTable *table, int64 size, int orig_pos, int64 key, int loop);

// initialise a cuckoo hash table with 'size' slots in each table
CuckooHashTable *new_cuckoo_hash_table(int size) {
	//printf(CYAN);
	//fprintf(stderr, "not yet implemented\n");
	CuckooHashTable *cuckoo = malloc(sizeof* cuckoo);
	assert(cuckoo != NULL);
	// Create two new inner tables (use helpter function here)
	cuckoo->table1 = new_inner_table(size);
	//printf("Successfully made table 1!\n");
	cuckoo->table2 = new_inner_table(size);
	//printf("Successfully made table 2!\n");
	cuckoo->size = size;
	// Then create a cuckoo table and link these to the inner tables
	//printf("Successfully made cuckoo table!\n");
	return cuckoo;
}

// free all memory associated with 'table'
void free_cuckoo_hash_table(CuckooHashTable *table) {
	//fprintf(stderr, "not yet implemented\n");
	//printf("Freeing table...\n");
	free(table->table1->slots);
	free(table->table1->inuse);
	free(table->table2->slots);
	free(table->table2->inuse);
	free(table->table1);
	free(table->table2);
	free(table);
	//printf("Success!\n");
}


// insert 'key' into 'table', if it's not in there already
// returns true if insertion succeeds, false if it was already in there
bool cuckoo_hash_table_insert(CuckooHashTable *table, int64 key) {
	//fprintf(stderr, "not yet implemented\n");
	//printf("Inserting %d...\n", key);
	if (cuckoo_hash_table_lookup(table, key) == true){
		//printf(RED "%d is already in the table!\n" RESET, key);
		return false;
	}
	bool result = try_insert(table, key, (h1(key)%table->size), key, EMPTY);
	//printf("Success!\n");
	return result;
}


// lookup whether 'key' is inside 'table'
// returns true if found, false if not
bool cuckoo_hash_table_lookup(CuckooHashTable *table, int64 key) {
	//fprintf(stderr, "not yet implemented\n");
	//printf("Looking up %d...", key);
	int pos1 = h1(key) % table->size;
	int pos2 = h2(key) % table->size;
	if (table->table1->slots[pos1] == key){
		//printf(GREEN "Key found in table 1 at position %d\n" RESET, pos1);
		return true;
	}
	else if (table->table2->slots[pos2] == key){
		//printf(GREEN "Key found in table 2 at position %d\n" RESET, pos2);
		return true;
	}
	return false;
}


// print the contents of 'table' to stdout
void cuckoo_hash_table_print(CuckooHashTable *table) {
	assert(table);
	printf("--- table size: %d\n", table->size);

	// print header
	printf("                    table one         table two\n");
	printf("                  key | address     address | key\n");
	
	// print rows of each table
	int i;
	for (i = 0; i < table->size; i++) {

		// table 1 key
		if (table->table1->inuse[i]) {
			printf(" %20llu ", table->table1->slots[i]);
		} else {
			printf(" %20s ", "-");
		}

		// addresses
		printf("| %-9d %9d |", i, i);

		// table 2 key
		if (table->table2->inuse[i]) {
			printf(" %llu\n", table->table2->slots[i]);
		} else {
			printf(" %s\n",  "-");
		}
	}

	// done!
	printf("--- end table ---\n");
}


// print some statistics about 'table' to stdout
void cuckoo_hash_table_stats(CuckooHashTable *table) {
	//fprintf(stderr, "not yet implemented\n");
	//printf("This table has still not yet exploded.\n");
}

// Helper Functions!

// Creates an inner table
InnerTable *new_inner_table(int size) {
	int i;

	InnerTable *table = malloc(sizeof(*table));
	assert(table != NULL);
	table->slots = malloc(sizeof(*table->slots) * size);
	assert(table->slots != NULL);
	table->inuse = malloc(sizeof(*table->inuse) * size);
	assert(table->inuse != NULL);
	//printf("Malloc success!\n");
	for (i = 0; i < size; i++) {
		table->inuse[i] = false;
		//printf("Setting slot %d to false.\n", i);
	}
	return table;
}

// Recursive function which performs cuckoo hash
bool try_insert(CuckooHashTable *table, int64 key, int orig_pos, int64 orig_key, int loop){
	int init_pos; 
	//printf(CYAN "Initial pos is: %d", init_pos);
	// base case
	loop++;
	InnerTable *inner_table;
	if (loop % 2 == 0) {
		inner_table = table->table2;
		init_pos = h2(key) % table->size;
	}
	else {
		inner_table = table->table1;
		init_pos = h1(key) % table->size;
	}
	if ((init_pos == orig_pos) && (key == orig_key) && loop > 1 && (loop % 2 == 1)) {
		//printf("Doubling Table...\n");
		upsize_table(table, table->size*2);
		//printf("%d key", key);
		try_insert(table, key, orig_pos, orig_key, EMPTY);
		return true;
	}
	// check if there is already something in the position
	if (inner_table->inuse[init_pos] == true){
		//printf(YELLOW "Table 1 Slot %d already occupied by %d, rehashing.\n" RESET, init_pos, table->table1->slots[init_pos]);
		int64 rehash_key = inner_table->slots[init_pos];
		inner_table->slots[init_pos] = key;
		try_insert(table, rehash_key, orig_pos, orig_key, loop);
	}
	else {
		//printf(GREEN "Successfully inserted %d!\n" RESET, key);
		inner_table->inuse[init_pos] = true;
		inner_table->slots[init_pos] = key;
		return true;
	}
}

void upsize_table(CuckooHashTable *table, int size) {
	assert(table);
	assert(size < MAX_TABLE_SIZE && "error: table has grown too large!");
	int i;
	int64 *old_keys_1 = table->table1->slots;
	int64 *old_keys_2 = table->table2->slots;
	bool *old_inuse_1 = table->table1->inuse;
	bool *old_inuse_2 = table->table2->inuse;
	int old_size = table->size;
	upsize_inner(table->table1, size);
	upsize_inner(table->table2, size);
	table->size = size;
	for (i = 0; i < old_size; i++) {
		if (old_inuse_1[i] == true){
			cuckoo_hash_table_insert(table, old_keys_1[i]);
		}
		if (old_inuse_2[i] == true){
			cuckoo_hash_table_insert(table, old_keys_2[i]);
		}
	}
	free(old_keys_1);
	free(old_keys_2);
	free(old_inuse_1);
	free(old_inuse_2);
}

void upsize_inner(InnerTable *table, int size){
	int i;
	table->slots = malloc((sizeof *table->slots) * size);
	assert(table->slots);
	table->inuse = malloc((sizeof *table->inuse) * size);
	assert(table->inuse);
	for (i = 0; i < size; i++) {
		table->inuse[i] = false;
	}
}


/*
static void initialise_table(CuckooHashTable *table, int size) {
	assert(size < MAX_TABLE_SIZE && "error: table has grown too large!");

	table->slots = malloc((sizeof *table->slots) * size);
	assert(table->slots);
	table->inuse = malloc((sizeof *table->inuse) * size);
	assert(table->inuse);
	int i;
	for (i = 0; i < size; i++) {
		table->inuse[i] = false;
	}

	table->size = size;
	table->load = 0;
}


// double the size of the internal table arrays and re-hash all
// keys in the old tables
static void double_table(CuckooHashTable *table) {
	table = new_cuckoo_hash_table(table->size * 2);
	double_inner(table->table1);
	double_inner(table->table2);
	int64 *oldslots = table->slots;
	bool  *oldinuse = table->inuse;
	int oldsize = table->size;


	int i;
	for (i = 0; i < oldsize; i++) {
		if (oldinuse[i] == true) {
			linear_hash_table_insert(table, oldslots[i]);
		}
	}

	free(oldslots);
	free(oldinuse);
}
*/