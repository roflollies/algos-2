/* * * * * * * * *
* Dynamic hash table using a combination of extendible hashing and cuckoo
* hashing with a single keys per bucket, resolving collisions by switching keys 
* between two tables with two separate hash functions and growing the tables 
* incrementally in response to cycles
*
* created for COMP20007 Design of Algorithms - Assignment 2, 2017
* by Samuel Xu
* Uses code retrieved from xtndbl1.c and linear.c created by 
* Matt Farrugia <matt.farrugia@unimelb.edu.au>
*/
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include "xuckoon.h"
/*
// Colours for debugging
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
// macro to calculate the rightmost n bits of a number x
#define rightmostnbits(n, x) (x) & ((1 << (n)) - 1)

typedef struct stats {
	int nbuckets;	// how many distinct buckets does the table point to
	int nkeys;		// how many keys are being stored in the table
	int time;		// how much CPU time has been used to insert/lookup keys
					// in this table
} Stats;
// a bucket stores a single key (full=true) or is empty (full=false)
// it also knows how many bits are shared between possible keys, and the first 
// table address that references it
typedef struct xuckoon_bucket {
	int id;			// a unique id for this bucket, equal to the first address
					// in the table which points to it
	int depth;		// how many hash value bits are being used by this bucket
	int nkeys;		// number of keys currently contained in this bucket
	int64 *keys;	// the keys stored in this bucket
} Bucket;

// an inner table is an extendible hash table with an array of slots pointing 
// to buckets holding up to 1 key, along with some information about the number 
// of hash value bits to use for addressing
typedef struct inner_table {
	Bucket **buckets;	// array of pointers to buckets
	int size;			// how many entries in the table of pointers (2^depth)
	int depth;			// how many bits of the hash value to use (log2(size))
	int bucketsize;		// maximum number of keys per bucket
} InnerTable;

// a xuckoon hash table is just two inner tables for storing inserted keys
struct xuckoon_table {
	InnerTable *table1;
	InnerTable *table2;
	Stats stats;
};


void try_xuckoon_insert(XuckoonHashTable *table, int64 key, int orig_pos, 
						int64 orig_key, int loop, int orig_table);

// create a new bucket first referenced from 'first_address', based on 'depth'
// bits of its keys' hash values
static Bucket *new_bucket(int first_address, int depth, int bucketsize) {
	Bucket *bucket = malloc(sizeof *bucket);
	assert(bucket);
	bucket->keys = malloc(sizeof(int64) * bucketsize);
	assert(bucket->keys);

	bucket->id = first_address;
	bucket->depth = depth;
	bucket->nkeys = 0;
	return bucket;
}

static InnerTable *new_inner_table(int bucketsize) {
	InnerTable *table = malloc(sizeof(*table));
	assert(table);

	table->size = 1;
	table->buckets = malloc(sizeof *table->buckets);
	assert(table->buckets);
	table->depth = 0;
	table->buckets[0] = new_bucket(0, 0, bucketsize);
	table->bucketsize = bucketsize;
	//printf("finish table\n");
	return table;
};

static void double_table(InnerTable *table) {
	int size = table->size * 2;
	assert(size < MAX_TABLE_SIZE && "error: table has grown too large!");

	//printf(RED "table size: %d\n" RESET, size);
	// get a new array of twice as many bucket pointers, and copy pointers down
	table->buckets = realloc(table->buckets, (sizeof *table->buckets) * size);
	assert(table->buckets);
	int i;
	for (i = 0; i < table->size; i++) {
		table->buckets[table->size + i] = table->buckets[i];
	}

	// finally, increase the table size and the depth we are using to hash keys
	table->size = size;
	table->depth++;
}

static void reinsert_key(XuckoonHashTable *table, int64 key, int table_no) {
	int address;
	InnerTable *inner_table;
	if (table_no == 1) {
		inner_table = table->table1;	
		address = rightmostnbits(inner_table->depth, h1(key));
	}
	else {
		inner_table = table->table2;
		address = rightmostnbits(inner_table->depth, h2(key));	
	}
	inner_table->buckets[address]->keys[inner_table->buckets[address]->nkeys] = key;
	inner_table->buckets[address]->nkeys++;
}

// split the bucket in 'table' at address 'address', growing table if necessary
static void split_bucket(XuckoonHashTable *table, int address, int table_no) {
	//printf("split bucket\n");

	InnerTable *inner_table;
	if (table_no == 1) {
		inner_table = table->table1;
	}
	else {
		inner_table = table->table2;
	}
	// FIRST,
	// do we need to grow the table?
	if (inner_table->buckets[address]->depth == inner_table->depth) {
		// yep, this bucket is down to its last pointer
		double_table(inner_table);
	}
	// either way, now it's time to split this bucket


	// SECOND,
	// create a new bucket and update both buckets' depth
	Bucket *bucket = inner_table->buckets[address];
	int depth = bucket->depth;
	int first_address = bucket->id;

	int new_depth = depth + 1;
	bucket->depth = new_depth;

	// new bucket's first address will be a 1 bit plus the old first address
	int new_first_address = 1 << depth | first_address;
	Bucket *newbucket = new_bucket(new_first_address, new_depth, inner_table->bucketsize);
	
	// THIRD,
	// redirect every second address pointing to this bucket to the new bucket
	// construct addresses by joining a bit 'prefix' and a bit 'suffix'
	// (defined below)

	// suffix: a 1 bit followed by the previous bucket bit address
	int bit_address = rightmostnbits(depth, first_address);
	int suffix = (1 << depth) | bit_address;

	// prefix: all bitstrings of length equal to the difference between the new
	// bucket depth and the table depth
	// use a for loop to enumerate all possible prefixes less than maxprefix:
	int maxprefix = 1 << (inner_table->depth - new_depth);

	int prefix;
	for (prefix = 0; prefix < maxprefix; prefix++) {
		
		// construct address by joining this prefix and the suffix
		int a = (prefix << new_depth) | suffix;

		// redirect this table entry to point at the new bucket
		inner_table->buckets[a] = newbucket;
	}

	// FINALLY,
	// filter the key from the old bucket into its rightful place in the new 
	// table (which may be the old bucket, or may be the new bucket)
	int64 *keys = malloc(sizeof(int64) * inner_table->bucketsize);
	int count = bucket->nkeys;
	int i;
	for (i = count-1; i > -1; i--) {
		keys[i] = bucket->keys[i];
		bucket->nkeys--;
	}
	// reinsert keys
	for (i = 0; i < count; i++) {
		reinsert_key(table, keys[i], table_no);
	}
	//xuckoon_hash_table_print(table);
}

// initialise an extendible cuckoo hash table
XuckoonHashTable *new_xuckoon_hash_table(int bucketsize) {
	XuckoonHashTable *cuckoo = malloc(sizeof* cuckoo);
	assert(cuckoo != NULL);
	// Create two new inner tables (use helpter function here)
	cuckoo->table1 = new_inner_table(bucketsize);
	//printf("Successfully made table 1!\n");
	cuckoo->table2 = new_inner_table(bucketsize);
	//printf("Successfully made table 2!\n");
	// Then create a cuckoo table and link these to the inner tables
	//printf("Successfully made cuckoo table!\n");
	cuckoo->stats.nbuckets = 1;
	cuckoo->stats.nkeys = 0;
	cuckoo->stats.time = 0;
	return cuckoo;
}


// free all memory associated with 'table'
void free_xuckoon_hash_table(XuckoonHashTable *table) {
	assert(table);

	// loop backwards through the array of pointers, freeing buckets only as we
	// reach their first reference
	// (if we loop through forwards, we wouldn't know which reference was last)
	int i;
	for (i = table->table1->size-1; i >= 0; i--) {
		if (table->table1->buckets[i]->id == i) {
			free(table->table1->buckets[i]);
		}
	}
	for (i = table->table2->size-1; i >= 0; i--) {
		if (table->table2->buckets[i]->id == i) {
			free(table->table2->buckets[i]);
		}
	}

	// free the array of bucket pointers
	free(table->table1->buckets);
	free(table->table2->buckets);
	free(table->table1);
	free(table->table2);
	
	// free the table struct itself
	free(table);	
}


// insert 'key' into 'table', if it's not in there already
// returns true if insertion succeeds, false if it was already in there
bool xuckoon_hash_table_insert(XuckoonHashTable *table, int64 key) {
	int start_time = clock();
	assert(table);

	// is this key already there?
	if (xuckoon_hash_table_lookup(table, key) == true) {
		table->stats.time += clock() - start_time;
		return false;
	}
	// initialise variables
	int hash;
	int address;

	// insert value into the table with less keys
	if (table->table1->size <= table->table2->size) {
		hash = h1(key);
		address = rightmostnbits(table->table1->depth, hash);
		try_xuckoon_insert(table, key, address, key, 0, 1);
	}
	else {
		hash = h2(key);
		address = rightmostnbits(table->table2->depth, hash);
		try_xuckoon_insert(table, key, address, key, 1, 2);
	}
	// add time elapsed to total CPU time before returning
	table->stats.time += clock() - start_time;
	return true;
}

// Function looks up value in the hash table
bool xuckoon_hash_table_lookup(XuckoonHashTable *table, int64 key) {
	assert(table);
	int start_time = clock(); // start timing

	// calculate table address for this key
	int address1 = rightmostnbits(table->table1->depth, h1(key));
	int address2 = rightmostnbits(table->table2->depth, h2(key));
	
	// look for the key in that bucket (unless it's empty)
	bool found = false;
	int i;
	for (i = 0; i < table->table1->buckets[address1]->nkeys; i++) {
		if (table->table1->buckets[address1]->keys[i] == key) {
			// found it!
			found = true;
		}
	}
	for (i = 0; i < table->table2->buckets[address2]->nkeys; i++) {
		if (table->table2->buckets[address2]->keys[i] == key) {
			// found it!
			found = true;
		}
	}

	// add time elapsed to total CPU time before returning result
	table->stats.time += clock() - start_time;
	return found;
}


// print the contents of 'table' to stdout
void xuckoon_hash_table_print(XuckoonHashTable *table) {
	assert(table != NULL);

	printf("--- table ---\n");

	// loop through the two tables, printing them
	InnerTable *innertables[2] = {table->table1, table->table2};
	int t;
	for (t = 0; t < 2; t++) {
		// print header
		printf("table %d\n", t+1);

		printf("  table:               buckets:\n");
		printf("  address | bucketid   bucketid [key]\n");
		
		// print table and buckets
		int i;
		for (i = 0; i < innertables[t]->size; i++) {
			// table entry
			printf("%9d | %-9d ", i, innertables[t]->buckets[i]->id);

			// if this is the first address at which a bucket occurs, print it now
			if (innertables[t]->buckets[i]->id == i) {
				//printf("Bucketsize: %d", innertables[t]->bucketsize);
				printf("%9d ", innertables[t]->buckets[i]->id);
				// print the bucket's contents
				printf("[");

				for(int j = 0; j < innertables[t]->bucketsize; j++) {
					if (j < innertables[t]->buckets[i]->nkeys) {
						printf(" %llu", innertables[t]->buckets[i]->keys[j]);
					} else {
						printf(" -");
					}
				}
				printf(" ]");
			}
			// end the line
			printf("\n");
		}
	}
	printf("--- end table ---\n");
}

// print some statistics about 'table' to stdout
void xuckoon_hash_table_stats(XuckoonHashTable *table) {
	assert(table);

	printf("--- table stats ---\n");

	// print some stats about state of the table
	printf("current tab 1 size: %d\n", table->table1->size);
	printf("current tab 2 size: %d\n", table->table2->size);
	printf("    number of keys: %d\n", table->stats.nkeys);
	printf(" number of buckets: %d\n", table->stats.nbuckets);

	// also calculate CPU usage in seconds and print this
	float seconds = table->stats.time * 1.0 / CLOCKS_PER_SEC;
	printf("    CPU time spent: %.6f sec\n", seconds);
	
	printf("--- end stats ---\n");
}

// Recursive function which performs cuckoo hash
void try_xuckoon_insert(XuckoonHashTable *table, int64 key, int orig_pos, 
							int64 orig_key, int loop, int orig_table){
	int address; 
	int hash;
	int table_no;
	// Check which table the function is currently in. Assign variables
	// accordingly.
	loop++;
	InnerTable *inner_table;
	if (loop % 2 == 0) {
		inner_table = table->table2;
		hash = h2(key);
		table_no = 2;
	}
	else {
		inner_table = table->table1;
		hash = h1(key);
		table_no = 1;
	}
	
	address = rightmostnbits(inner_table->depth, hash);
	// If bucket is full, then split before doing anything until there is space
	while (inner_table->buckets[address]->nkeys == inner_table->bucketsize) {
		split_bucket(table, address, table_no);
		// recalculate address
		address = rightmostnbits(inner_table->depth, hash);
	}
		// If there is a long cuckoo chain (according to spec) then split.
	if (((address == orig_pos) && (key == orig_key) && loop > 2) || 
		(loop > (100))) {
		while (inner_table->buckets[address]->nkeys == inner_table->bucketsize) {
			// split bucket
			split_bucket(table, address, table_no);
			address = rightmostnbits(inner_table->depth, hash);
		}
		// reinsert key
		try_xuckoon_insert(table, key, orig_pos, orig_key, EMPTY, orig_table);
		return;
	}
	// check if there is already something in the position
	if (xuckoon_hash_table_lookup(table, key) == true){
		// check if there is already something in the position, if there is,
		// then push new value into that bucket, and take the last key to be 
		// rehashed and try inserting the rehash key into the opposite table
		int64 rehash_key = inner_table->buckets[address]->keys[inner_table->buckets[address]->nkeys];
		inner_table->buckets[address]->keys[inner_table->buckets[address]->nkeys] = key;
		try_xuckoon_insert(table, rehash_key, orig_pos, orig_key, loop, orig_table);
		return;
	}
	else {
		// otherwise, just insert the key and return true
		inner_table->buckets[address]->keys[inner_table->buckets[address]->nkeys] = key;
		inner_table->buckets[address]->nkeys++;
		table->stats.nkeys++;
		return;
	}
}
