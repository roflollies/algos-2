/* * * * * * * * *
 * Dynamic hash table using extendible hashing with multiple keys per bucket,
 * resolving collisions by incrementally growing the hash table
 *
 * created for COMP20007 Design of Algorithms - Assignment 2, 2017
 * by ...
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "xtndbln.h"

#include <windows.h>
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define RESET   "\x1b[0m"

#define EMPTY 0

// macro to calculate the rightmost n bits of a number x
#define rightmostnbits(n, x) (x) & ((1 << (n)) - 1)

// a bucket stores an array of keys
// it also knows how many bits are shared between possible keys, and the first 
// table address that references it
typedef struct xtndbln_bucket {
	int id;			// a unique id for this bucket, equal to the first address
					// in the table which points to it
	int depth;		// how many hash value bits are being used by this bucket
	int nkeys;		// number of keys currently contained in this bucket
	int64 *keys;	// the keys stored in this bucket
} Bucket;

typedef struct stats {
	int nbuckets;	// how many distinct buckets does the table point to
	int nkeys;		// how many keys are being stored in the table
	int time;		// how much CPU time has been used to insert/lookup keys
					// in this table
} Stats;
// a hash table is an array of slots pointing to buckets holding up to 
// bucketsize keys, along with some information about the number of hash value 
// bits to use for addressing
struct xtndbln_table {
	Bucket **buckets;	// array of pointers to buckets
	int size;			// how many entries in the table of pointers (2^depth)
	int depth;			// how many bits of the hash value to use (log2(size))
	int bucketsize;		// maximum number of keys per bucket
	Stats stats;
};

// create a new bucket first referenced from 'first_address', based on 'depth'
// bits of its keys' hash values
static Bucket *new_bucket(int first_address, int depth, int bucketsize) {
	//printf(GREEN "New bucket at %d size.\n" RESET, bucketsize);
	Bucket *bucket = malloc(sizeof *bucket);
	assert(bucket);

	bucket->keys = malloc(sizeof(int64) * bucketsize);
	assert(bucket->keys);

	bucket->id = first_address;
	bucket->depth = depth;
	bucket->nkeys = 0;
	//printf(GREEN "Finish bucket.\n" RESET);
	return bucket;
}

// double the table of bucket pointers, duplicating the bucket pointers in the
// first half into the new second half of the table
static void double_table(XtndblNHashTable *table) {
	int size = table->size * 2;
	assert(size < MAX_TABLE_SIZE && "error: table has grown too large!");

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


// reinsert a key into the hash table after splitting a bucket --- we can assume
// that there will definitely be space for this key because it was already
// inside the hash table previously
// use 'xtndbl1_hash_table_insert()' instead for inserting new keys
static void reinsert_key(XtndblNHashTable *table, int64 key) {
	//printf(YELLOW "reinserting %d..." RESET, key);
	int address = rightmostnbits(table->depth, h1(key));
	table->buckets[address]->keys[table->buckets[address]->nkeys] = key;
	table->buckets[address]->nkeys++;
	//printf(GREEN "reinsert success!\n" RESET);
}

// split the bucket in 'table' at address 'address', growing table if necessary
static void split_bucket(XtndblNHashTable *table, int address) {
	//xtndbln_hash_table_print(table);
	// FIRST,
	// do we need to grow the table?
	if (table->buckets[address]->depth == table->depth) {
		// yep, this bucket is down to its last pointer
		double_table(table);
	}
	// either way, now it's time to split this bucket
	

	// SECOND,
	// create a new bucket and update both buckets' depth
	Bucket *bucket = table->buckets[address];
	int depth = bucket->depth;
	int first_address = bucket->id;

	int new_depth = depth + 1;
	bucket->depth = new_depth;
	// new bucket's first address will be a 1 bit plus the old first address
	int new_first_address = 1 << depth | first_address;
	//xtndbln_hash_table_print(table);
	Bucket *newbucket = new_bucket(new_first_address, new_depth, table->bucketsize);
	//xtndbln_hash_table_print(table);
	table->stats.nbuckets++;
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
	int maxprefix = 1 << (table->depth - new_depth);

	int prefix;
	for (prefix = 0; prefix < maxprefix; prefix++) {
		
		// construct address by joining this prefix and the suffix
		int a = (prefix << new_depth) | suffix;

		// redirect this table entry to point at the new bucket
		table->buckets[a] = newbucket;
	}
	//xtndbln_hash_table_print(table);
	// FINALLY,
	// filter the key from the old bucket into its rightful place in the new 
	// table (which may be the old bucket, or may be the new bucket)

	// remove and reinsert the key
	// remove keys
	int64 *keys = malloc(sizeof(int64) * table->bucketsize);
	int count = bucket->nkeys;
	int i;
	for (i = count-1; i > -1; i--) {
		keys[i] = bucket->keys[i];
		bucket->nkeys--;
	}
	// reinsert keys
	for (i = 0; i < count; i++) {
		//printf(RED "reinserting %d...\n" RESET, keys[i]);
		reinsert_key(table, keys[i]);
		//xtndbln_hash_table_print(table);
	}
	//free(keys);
}




// initialise an extendible hash table with 'bucketsize' keys per bucket
XtndblNHashTable *new_xtndbln_hash_table(int bucketsize) {
	//fprintf(stderr, "not yet implemented\n");
	// make a new table
	// malloc table
	// create a new bucket of depth bucketsize
	//printf("New table.\n");
	XtndblNHashTable *table = malloc(sizeof(*table));
	assert(table);

	table->size = 1;
	table->buckets = malloc(sizeof *table->buckets);
	assert(table->buckets);
	table->depth = 0;
	table->buckets[0] = new_bucket(0, 0, bucketsize);
	table->bucketsize = bucketsize;

	table->stats.nbuckets = 1;
	table->stats.nkeys = 0;
	table->stats.time = 0;
	//printf("finish table\n");
	return table;
}


// free all memory associated with 'table'
void free_xtndbln_hash_table(XtndblNHashTable *table) {
	assert(table);
	//printf(YELLOW "Freeing...\n" RESET);
	// loop backwards through the array of pointers, freeing buckets only as we
	// reach their first reference
	// (if we loop through forwards, we wouldn't know which reference was last)
	int i, j;
	for (i = table->size-1; i >= 0; i--) {
		if (table->buckets[i]->id == i) {
			//printf(YELLOW "Freeing id %d...\n" RESET, i);
			//for (j = 0; j < table->buckets[i]->nkeys; j++) {
			//printf(YELLOW "Attempting to free keys\n");
			free(table->buckets[i]->keys);
			//printf(GREEN "Finished freeing keys!\n");
			//}
			free(table->buckets[i]);
			//printf(YELLOW "Freed bucket!\n");
		}
	}

	// free the array of bucket pointers
	free(table->buckets);
	
	// free the table struct itself
	free(table);
	//printf(GREEN "Freedom!" RESET);
}



// insert 'key' into 'table', if it's not in there already
// returns true if insertion succeeds, false if it was already in there
bool xtndbln_hash_table_insert(XtndblNHashTable *table, int64 key) {
	//fprintf(stderr, "not yet implemented\n");
	assert(table);
	int start_time = clock(); // start timing
	
	// calculate table address
	int hash = h1(key);
	int address = rightmostnbits(table->depth, hash);
	
	// is this key already there?
	int i;
	for (i = 0; i < table->buckets[address]->nkeys; i++) {
		if (table->buckets[address]->keys[i] == key) {
			table->stats.time += clock() - start_time; // add time elapsed
			//printf("key already here!\n");
			return false;
		}
	} 

	// if not, make space in the table until our target bucket has space
	while (table->buckets[address]->nkeys == table->bucketsize) {
		split_bucket(table, address);

		// and recalculate address because we might now need more bits
		address = rightmostnbits(table->depth, hash);
	}

	// there's now space! we can insert this key
	//printf(GREEN "inserting %d" RESET, key);
	table->buckets[address]->keys[table->buckets[address]->nkeys] = key;
	table->buckets[address]->nkeys++;
	//printf("%d\n", table->buckets[address]->keys[table->buckets[address]->nkeys-1]);
	table->stats.nkeys++;

	// add time elapsed to total CPU time before returning
	table->stats.time += clock() - start_time;
	return true;
}


// lookup whether 'key' is inside 'table'
// returns true if found, false if not
bool xtndbln_hash_table_lookup(XtndblNHashTable *table, int64 key) {
	assert(table);
	int start_time = clock(); // start timing

	// calculate table address for this key
	int address = rightmostnbits(table->depth, h1(key));
	
	// look for the key in that bucket (unless it's empty)
	bool found = false;
	int i;
	for (i = 0; i < table->buckets[address]->nkeys; i++) {
		if (table->buckets[address]->keys[i] == key) {
			// found it!
			found = true;
		}
	}

	// add time elapsed to total CPU time before returning result
	table->stats.time += clock() - start_time;
	return found;
}


// print the contents of 'table' to stdout
void xtndbln_hash_table_print(XtndblNHashTable *table) {
	assert(table);
	printf("--- table size: %d\n", table->size);

	// print header
	printf("  table:               buckets:\n");
	printf("  address | bucketid   bucketid [key]\n");
	
	// print table and buckets
	int i;
	for (i = 0; i < table->size; i++) {
		// table entry
		printf("%9d | %-9d ", i, table->buckets[i]->id);

		// if this is the first address at which a bucket occurs, print it now
		if (table->buckets[i]->id == i) {
			printf("%9d ", table->buckets[i]->id);

			// print the bucket's contents
			printf("[");
			for(int j = 0; j < table->bucketsize; j++) {
				if (j < table->buckets[i]->nkeys) {
					printf(" %llu", table->buckets[i]->keys[j]);
				} else {
					printf(" -");
				}
			}
			printf(" ]");
		}
		// end the line
		printf("\n");
	}

	printf("--- end table ---\n");
}


// print some statistics about 'table' to stdout
void xtndbln_hash_table_stats(XtndblNHashTable *table) {
	assert(table);

	printf("--- table stats ---\n");

	// print some stats about state of the table
	printf("current table size: %d\n", table->size);
	printf("    number of keys: %d\n", table->stats.nkeys);
	printf(" number of buckets: %d\n", table->stats.nbuckets);

	// also calculate CPU usage in seconds and print this
	float seconds = table->stats.time * 1.0 / CLOCKS_PER_SEC;
	printf("    CPU time spent: %.6f sec\n", seconds);
	
	printf("--- end stats ---\n");
}
