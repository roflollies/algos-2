# Hash-Tables
Project 2 for Design of Algorithms, 2017 Semester 1

This assignment as defined in the `specification.pdf` called for several different types of hash tables to be implemented in C.
This included Linear, Cuckoo, and Extendible Hashing. They were to hash arbitrary 64-bit integer keys, and provide generic "look up" and "insert" operations.
The hash tables were also to be *resized dynamically* as more keys were inserted.

A report on the characteristics of each of the types of hashing was also created `report.pdf`.

Usage:
After compiling with `make`, use it with
`./a2 -t <table_type> [-s starting size]`

More instructions can be found in `specification.pdf`
