% ITTIA DB SQL Memory Storage Examples

Memory storage is an alternative back-end for database tables that reside only in main memory. Applications use memory storage when read and update performance is so critical that data loss is sometimes acceptable. Memory tables are not saved automatically when the database is closed, but otherwise behave just like disk tables.

The Memory Storage examples show how to create tables in memory and leverage their unique properties.

# memory_storage_capacity

The Memory Storage Capacity example uses ITTIA DB SQL memory tables to simulate an indexed in-memory cache. The cache size is strictly limited, so old cache entries are removed as necessary. This example demonstrates:

 - Creating a memory storage database with a limited capacity.
 - Removing old records to reclaim storage.

# memory_disk_hybrid

The Memory-Disk Hybrid database example stores persistent and temporary data together in a hybrid ITTIA DB SQL database. Memory tables must be repopulated when the database is reopened, while disk tables are preserved. This example demonstrates:

 - Creating a hybrid storage with disk and memory tables.

# memory_storage_embedded_server

The Memory Storage Embedded Sever example shows how to start an in-memory database server in the background of an application.
