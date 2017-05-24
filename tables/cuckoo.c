/* * * * * * * * *
 * Dynamic hash table using cuckoo hashing, resolving collisions by switching
 * keys between two tables with two separate hash functions
 *
 * created for COMP20007 Design of Algorithms - Assignment 2, 2017
 * by Samuel Xu
 * 
 * Uses code retrieved from linear.c created by 
 * Matt Farrugia <matt.farrugia@unimelb.edu.au>
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include "cuckoo.h"

/*
#include <windows.h>
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define RESET   "\x1b[0m"
*/

#define EMPTY 0

typedef struct stats {
	int nkeys;		// how many keys are being stored in the table
	int time;		// how much CPU time has been used to insert/lookup keys
					// in this table
} Stats;

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
	Stats stats;
};

void upsize_table(CuckooHashTable *table, int size);
void upsize_inner(InnerTable *table, int size);
InnerTable *new_inner_table(int size);
void try_insert(CuckooHashTable *table, int64 size, int orig_pos, 
				int64 key, int loop);

// initialise a cuckoo hash table with 'size' slots in each table
CuckooHashTable *new_cuckoo_hash_table(int size) {
	// Create a cuckoo table
	CuckooHashTable *cuckoo = malloc(sizeof* cuckoo);
	assert(cuckoo != NULL);
	// Create two new inner tables (use helpter function here)
	cuckoo->table1 = new_inner_table(size);
	cuckoo->table2 = new_inner_table(size);
	cuckoo->size = size;
	cuckoo->stats.time = 0;
	cuckoo->stats.nkeys = 0;
	return cuckoo;
}

// free all memory associated with 'table'
void free_cuckoo_hash_table(CuckooHashTable *table) {
	// Free inner table arrays
	free(table->table1->slots);
	free(table->table1->inuse);
	free(table->table2->slots);
	free(table->table2->inuse);
	// Free inner tables
	free(table->table1);
	free(table->table2);
	// Free table
	free(table);
}


// insert 'key' into 'table', if it's not in there already
// returns true if insertion succeeds, false if it was already in there
bool cuckoo_hash_table_insert(CuckooHashTable *table, int64 key) {
	// Check if the key is already in the table, if so, return false
	int start_time = clock(); // start timing
	if (cuckoo_hash_table_lookup(table, key) == true){
		table->stats.time += clock() - start_time;
		return false;
	}
	// call recursive function with the key and hash. If false, then
	// return unsuccessful insert, else return success
	try_insert(table, key, (h1(key)%table->size), key, EMPTY);
	table->stats.time += clock() - start_time;
	return true;
}


// lookup whether 'key' is inside 'table'
// returns true if found, false if not
bool cuckoo_hash_table_lookup(CuckooHashTable *table, int64 key) {
	int start_time = clock(); 
	// Check both positions the key could possibly be in
	int pos1 = h1(key) % table->size;
	int pos2 = h2(key) % table->size;
	// If key is found, return true
	if (table->table1->slots[pos1] == key){
		table->stats.time += clock() - start_time;		
		return true;
	}
	else if (table->table2->slots[pos2] == key){
		table->stats.time += clock() - start_time;
		return true;
	}
	table->stats.time += clock() - start_time;
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
	assert(table != NULL);
	printf("--- table stats ---\n");
	// print some information about the table
	printf("current size: %d slots\n", table->size);
	printf("current load: %d items\n", table->stats.nkeys);
	printf(" load factor: %.3f%%\n", table->stats.nkeys * 100.0 / table->size*2);
	// also calculate CPU usage in seconds and print this
	float seconds = table->stats.time * 1.0 / CLOCKS_PER_SEC;
	printf("    CPU time spent: %.6f sec\n", seconds);
	printf("--- end stats ---\n");
}

// Helper Functions!

// Creates an inner table
InnerTable *new_inner_table(int size) {
	int i;
	// Malloc the new inner table
	InnerTable *table = malloc(sizeof(*table));
	assert(table != NULL);
	// Malloc the size of the inner table slots array
	table->slots = malloc(sizeof(*table->slots) * size);
	assert(table->slots != NULL);
	// Malloc the size of the inner table inuse array
	table->inuse = malloc(sizeof(*table->inuse) * size);
	assert(table->inuse != NULL);

	// Set all inuse values to false
	for (i = 0; i < size; i++) {
		table->inuse[i] = false;
	}
	return table;
}

// Recursive function which performs cuckoo hash
void try_insert(CuckooHashTable *table, int64 key, int orig_pos, 
				int64 orig_key, int loop){
	int init_pos; 
	// Increment loop
	loop++;
	// Initialise hash and inner table according to the loop
	InnerTable *inner_table;
	if (loop % 2 == 0) {
		inner_table = table->table2;
		init_pos = h2(key) % table->size;
	}
	else {
		inner_table = table->table1;
		init_pos = h1(key) % table->size;
	}
	// If it's table 1, and the key has been inserted into this slot before
	// (i.e. infinite cuckoo chain), then upsize both tables and reinsert
	// the original key
	if ((init_pos == orig_pos) && (key == orig_key) && (loop > 1) && 
		(loop % 2 == 1)) {
		upsize_table(table, table->size*2);
		try_insert(table, key, orig_pos, orig_key, EMPTY);
		return;
	}
	// check if there is already something in the position
	if (inner_table->inuse[init_pos] == true){
		// if there's already something in that position, then insert and
		// recursively call insert with the opposite table and the key
		// taken out
		int64 rehash_key = inner_table->slots[init_pos];
		inner_table->slots[init_pos] = key;
		try_insert(table, rehash_key, orig_pos, orig_key, loop);
		return;
	}
	else {
		// If there's nothing there, insert!
		inner_table->inuse[init_pos] = true;
		inner_table->slots[init_pos] = key;
		table->stats.nkeys++;
	}
}

// Function doubles the size of the table
void upsize_table(CuckooHashTable *table, int size) {
	// Check the table for size and emptiness
	assert(table);
	assert(size < MAX_TABLE_SIZE && "error: table has grown too large!");
	int i;
	// Copy old keys into their respective arrays
	int64 *old_keys_1 = table->table1->slots;
	int64 *old_keys_2 = table->table2->slots;
	bool *old_inuse_1 = table->table1->inuse;
	bool *old_inuse_2 = table->table2->inuse;
	int old_size = table->size;
	// remake inner tables with new size
	upsize_inner(table->table1, size);
	upsize_inner(table->table2, size);
	// update table size
	table->size = size;
	// Reinsert old keys into respective tables
	for (i = 0; i < old_size; i++) {
		if (old_inuse_1[i] == true){
			cuckoo_hash_table_insert(table, old_keys_1[i]);
		}
		if (old_inuse_2[i] == true){
			cuckoo_hash_table_insert(table, old_keys_2[i]);
		}
	}
	// Free arrays
	free(old_keys_1);
	free(old_keys_2);
	free(old_inuse_1);
	free(old_inuse_2);
}

// function doubles the size of an inner table
void upsize_inner(InnerTable *table, int size){
	int i;
	// Malloc the table arrays
	table->slots = malloc((sizeof *table->slots) * size);
	assert(table->slots);
	table->inuse = malloc((sizeof *table->inuse) * size);
	assert(table->inuse);
	// Set all slots to false
	for (i = 0; i < size; i++) {
		table->inuse[i] = false;
	}
}
