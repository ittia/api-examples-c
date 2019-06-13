/**************************************************************************/
/*                                                                        */
/*      Copyright (c) 2005-2019 by ITTIA L.L.C. All rights reserved.      */
/*                                                                        */
/*  This software is copyrighted by and is the sole property of ITTIA     */
/*  L.L.C.  All rights, title, ownership, or other interests in the       */
/*  software remain the property of ITTIA L.L.C.  This software may only  */
/*  be used in accordance with the corresponding license agreement.  Any  */
/*  unauthorized use, duplication, transmission, distribution, or         */
/*  disclosure of this software is expressly forbidden.                   */
/*                                                                        */
/*  This Copyright notice may not be removed or modified without prior    */
/*  written consent of ITTIA L.L.C.                                       */
/*                                                                        */
/*  ITTIA L.L.C. reserves the right to modify this software without       */
/*  notice.                                                               */
/*                                                                        */
/*  info@ittia.com                                                        */
/*  http://www.ittia.com                                                  */
/*                                                                        */
/*                                                                        */
/**************************************************************************/

#include <ittia/db.h>

#include <stdio.h>
#include <time.h>

/* Performance test settings. */

#define EXAMPLE_DATABASE "performance.ittiadb"

const int ROW_COUNT = 100;

#define GENERATE_ID(i) ((i) * 1103515245 + 12345)

/* Current time offset in milliseconds. */
time_t milliseconds();

/* Database schema: table, field, and index definitions. */

#define T_ID 0
#define T_N 1
#define T_S 2

db_fielddef_t table_t_fields[] = {
    { T_ID, "ID", DB_COLTYPE_UINT32,    0, 0, DB_NOT_NULL, NULL, 0 },
    { T_N,  "N",  DB_COLTYPE_SINT32,    0, 0, DB_NULLABLE, NULL, 0 },
    { T_S,  "S",  DB_COLTYPE_UTF8STR, 100, 0, DB_NULLABLE, NULL, 0 }
};
db_tabledef_t table_t = {
    DB_ALLOC_INITIALIZER(),
    DB_TABLETYPE_DEFAULT,
    "T",
    DB_ARRAY_DIM(table_t_fields),
    table_t_fields,
    0,
    NULL
};

db_indexfield_t index_t_id_fields[] = {
    { T_ID, 0 }
};
db_indexdef_t index_t_id = {
    DB_ALLOC_INITIALIZER(),
    DB_INDEXTYPE_DEFAULT,
    "ID",
    DB_MULTISET_INDEX,
    DB_ARRAY_DIM(index_t_id_fields),
    index_t_id_fields,
};

/* Main program. */

int example_main(int argc, char* argv[])
{
    const char * const example_database = argc > 1 ? argv[1] : EXAMPLE_DATABASE;

    db_file_storage_config_t config;
    db_t database;
    db_cursor_t t_cursor, t_ordered_cursor;
    db_row_t t_row;
    db_table_cursor_t ordered_cursor_def;
    time_t start;
    int i;

    /* Bind database fields to local variables. */

    uint32_t id;
    int32_t n;
    char s[100];

    db_bind_t t_binds[] = {
        DB_BIND_VAR(T_ID, DB_VARTYPE_UINT32,  id),
        DB_BIND_VAR(T_N,  DB_VARTYPE_SINT32,  n),
        DB_BIND_STR(T_S,  DB_VARTYPE_UTF8STR, s)
    };

    t_row = db_alloc_row(t_binds, DB_ARRAY_DIM(t_binds));

    /* Create the database, table, and index. */

    db_file_storage_config_init(&config);
#ifdef PAGE_SIZE
    /* Set permanent storage I/O block size for this database file. */
    config.page_size = PAGE_SIZE;
#endif
#ifdef PAGE_CACHE_SIZE
    /* Limit page cache memory footprint. */
    config.buffer_count = PAGE_CACHE_SIZE / config.page_size;
#endif

    database = db_create_file_storage(example_database, &config);
    db_file_storage_config_destroy(&config);

    if (database == NULL)
    {
        printf("Error opening database: %d", (int) get_db_error());
        return 1;
    }

    db_create_table(database, table_t.table_name, &table_t, 0);
    db_create_index(database, table_t.table_name, index_t_id.index_name, &index_t_id);

    /* Configure transactions. */
    db_set_tx_default(database, DB_GROUP_COMPLETION | DB_READ_COMMITTED);

    /* Open unordered table cursor. */
    t_cursor = db_open_table_cursor(database, table_t.table_name, NULL);

    /* Open table cursor ordered by the fields in the ID index. */
    db_table_cursor_init(&ordered_cursor_def);
    ordered_cursor_def.index = index_t_id.index_name;
    ordered_cursor_def.flags = DB_CAN_MODIFY;
    t_ordered_cursor = db_open_table_cursor(database, table_t.table_name, &ordered_cursor_def);
    db_table_cursor_destroy(&ordered_cursor_def);

    /* Insert rows. */

    printf("Insert %d rows: ", ROW_COUNT);
    fflush(stdout);
    start = milliseconds();

    for(i = 1; i <= ROW_COUNT; i++) {
        db_begin_tx(database, 0);

        id = GENERATE_ID(i);
        n = ROW_COUNT / 2 - i;
        sprintf(s, "%d", i);
        db_insert(t_cursor, t_row, NULL, 0);

        db_commit_tx(database, 0);
    }

    printf("%d milliseconds\n", (int) (milliseconds() - start));

    /* Table scan: iterate over all rows in unspecified order. */

    printf("Table scan %d rows: ", ROW_COUNT);
    fflush(stdout);
    start = milliseconds();

    db_begin_tx(database, 0);
    for (db_seek_first(t_cursor); !db_eof(t_cursor); db_seek_next(t_cursor))
    {
        db_fetch(t_cursor, t_row, NULL);
    }
    db_commit_tx(database, 0);

    printf("%d milliseconds\n", (int) (milliseconds() - start));

    /* Index scan: iterate over all rows in ID order. */

    printf("Index scan %d rows: ", ROW_COUNT);
    fflush(stdout);
    start = milliseconds();

    db_begin_tx(database, 0);
    for (db_seek_first(t_ordered_cursor); !db_eof(t_ordered_cursor); db_seek_next(t_ordered_cursor))
    {
        db_fetch(t_ordered_cursor, t_row, NULL);
    }
    db_commit_tx(database, 0);

    printf("%d milliseconds\n", (int) (milliseconds() - start));

    /* Select each row by index seek. */

    printf("Index seek %d rows: ", ROW_COUNT);
    fflush(stdout);
    start = milliseconds();

    db_begin_tx(database, 0);
    for(i = 1; i <= ROW_COUNT; i++) {
        id = GENERATE_ID(i);
        db_seek(t_ordered_cursor, DB_SEEK_EQUAL, t_row, NULL, 1);
        db_fetch(t_ordered_cursor, t_row, NULL);
    }
    db_commit_tx(database, 0);

    printf("%d milliseconds\n", (int) (milliseconds() - start));

    /* Update each row in the table by index seek. */

    printf("Update %d table rows: ", ROW_COUNT);
    fflush(stdout);
    start = milliseconds();

    for(i = 1; i <= ROW_COUNT; i++) {
        db_begin_tx(database, 0);

        id = GENERATE_ID(i);
        db_seek(t_ordered_cursor, DB_SEEK_EQUAL, t_row, NULL, 1);
        db_fetch(t_ordered_cursor, t_row, NULL);

        n = -n;
        s[0] += '\x30';
        db_update(t_ordered_cursor, t_row, NULL);

        db_commit_tx(database, 0);
    }

    printf("%d milliseconds\n", (int) (milliseconds() - start));

    /* Delete all rows. */

    printf("Delete %d rows: ", ROW_COUNT);
    fflush(stdout);
    start = milliseconds();

    db_seek_first(t_cursor);
    while (!db_eof(t_cursor))
    {
        db_begin_tx(database, 0);
        db_delete(t_cursor, DB_DELETE_SEEK_NEXT);
        db_commit_tx(database, 0);
    }

    printf("%d milliseconds\n", (int) (milliseconds() - start));

    db_close_cursor(t_cursor);
    db_close_cursor(t_ordered_cursor);

    db_shutdown(database, 0, NULL);

    db_free_row(t_row);

    return 0;
}


/* Utility functions. */

#if defined(_WIN32)
#include <windows.h>

time_t milliseconds()
{
    return GetTickCount();
}
#elif defined(OS_UCOS_III)
#include <os.h>

time_t milliseconds()
{
    OS_ERR err;
    /* Tick rate should fall in the range 10 to 1000 Hz. */
    return OSTimeGet(&err) * (1000 / OS_CFG_TICK_RATE_HZ);
}

#else
#include <sys/time.h>

time_t milliseconds()
{
    struct timeval tm;
    gettimeofday(&tm, NULL);
    return (time_t)(tm.tv_sec * 1000 + tm.tv_usec / 1000);
}
#endif
