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

/*-------------------------------------------------------------------------*
 * @file phonebook_sql.c                                                   *
 *                                                                         *
 * Command-line example program demonstrating the ITTIA DB C API.          *
 *-------------------------------------------------------------------------*/

#include "phonebook_c.h"
#include "dbs_schema.h"
#include "dbs_error_info.h"

#include <stdio.h>
#include <string.h>


/*-------------------------------------------------------------------------*
 * Sequences are made available in global scope for convenience.           *
 *-------------------------------------------------------------------------*/
db_sequence_t contact_sequence = NULL;


static void update_contact_picture(db_t hdb, contactid_t id, const char *file_name);


/*-------------------------------------------------------------------------*
 * Helper function to print an error message and current error code.       *
 *-------------------------------------------------------------------------*/
void print_error_message(char * message, db_cursor_t cursor)
{
    if (get_db_error() == DB_NOERROR) {
        if (message != NULL)
            fprintf(stderr, "ERROR: %s\n", message);
    }
    else {
        char * query_message = NULL;
        dbs_error_info_t info = dbs_get_error_info( get_db_error() );

        if (cursor != NULL)
            db_get_error_message(cursor, &query_message);

        fprintf(stderr, "ERROR %s: %s\n", info.name, info.description);
        if (query_message != NULL)
            fprintf(stderr, "%s\n", query_message);
        if (message != NULL)
            fprintf(stderr, "%s\n", message);

        /* Error has been handled, so clear error state. */
        clear_db_error();
    }
}

/*-------------------------------------------------------------------------*
 * Perform the SQL command, checking for errors.                           *
 *-------------------------------------------------------------------------*/
int execute_command(db_t hdb, const char* cmd, db_cursor_t *c)
{
    db_cursor_t sql_c;

    sql_c = db_prepare_sql_cursor(hdb, cmd, 0);

    if  ( sql_c ) {
        if( !db_is_prepared( sql_c )) {
            print_error_message("Prepare SQL failed.", sql_c);
            db_close_cursor( sql_c );
            return 0;
        }
        else if (db_execute( sql_c, NULL, NULL ) != DB_OK) {
            print_error_message("Execute SQL failed.", sql_c);
            db_close_cursor( sql_c );
            return 0;
        }
        /*-----------------------------------------------------------------*
         * Check to see if the caller wants the cursor returned.           *
         * If not, close the cursor.                                       *
         *-----------------------------------------------------------------*/
        if (c)
            *c = sql_c;
        else
            db_close_cursor( sql_c );

        return 1;
    }
    else {
        print_error_message("Prepare SQL failed.", NULL);
        return 0;
    }
}

/*-------------------------------------------------------------------------*
 * General schema creation routine.                                        *
 *-------------------------------------------------------------------------*/
int create_schema_sql(db_t hdb, int with_picture)
{
    char    buffer[256];
    int     rc;

    /*---------------------------------------------------------------------*
     * Create CONTACT table                                                *
     *---------------------------------------------------------------------*/
    if (with_picture)
        sprintf(buffer,
            "create table contact ("
            "  id uint64 not null,"
            "  name utf16str(%d) not null,"
            "  ring_id uint64,"
            "  picture_name varchar(%d),"
            "  picture blob,"
            "  constraint by_id primary key (id)"
            ")",
            MAX_CONTACT_NAME, MAX_FILE_NAME);
    else
        sprintf(buffer,
            "create table contact ("
            "  id uint64 not null,"
            "  name utf16str(%d) not null,"
            "  ring_id uint64,"
            "  picture_name varchar(%d),"
            "  constraint by_id primary key (id)"
            ")",
            MAX_CONTACT_NAME, MAX_FILE_NAME);
    if  (!(rc = execute_command(hdb, buffer, NULL))) {
        printf( "creating contact table.\n" );
        return rc;
    }
    /*---------------------------------------------------------------------*
     * Create name index on CONTACT table                                  *
     *---------------------------------------------------------------------*/
    if  (!(rc = execute_command(hdb, "create index by_name on contact(name)", NULL))) {
        printf( "creating contact table by_name index.\n" );
        return rc;
    }
    /*---------------------------------------------------------------------*
     * Create PHONE_NUMBER table                                           *
     *---------------------------------------------------------------------*/
    sprintf(buffer,
        "create table phone_number ("
        "  contact_id uint64 not null,"
        "  number ansistr(%d) not null,"
        "  type uint64 not null,"
        "  speed_dial sint64,"
        "  constraint contact_ref foreign key (contact_id) references contact(id)"
        ")",
        MAX_PHONE_NUMBER);
    if  (!(rc = execute_command(hdb, buffer, NULL))) {
        printf( "creating phone_number table.\n" );
        return rc;
    }
    /*---------------------------------------------------------------------*
     * Create contact_id index on PHONE_NUMBER table                       *
     *---------------------------------------------------------------------*/
    if  (!(rc = execute_command(hdb, "create index by_contact_id on phone_number(contact_id)", NULL))) {
        printf( "creating phone_number by_contact_id index.\n" );
        return rc;
    }
    /*---------------------------------------------------------------------*
     * Create CONTACT_ID sequence                                          *
     *---------------------------------------------------------------------*/
    if  (!(rc = execute_command(hdb, "create sequence contact_id start with 1", NULL))) {
        printf( "creating contact_id sequence.\n" );
        return rc;
    }
    return 1;
}

/*-------------------------------------------------------------------------*
 * Begin Transaction Handler.                                              *
 *-------------------------------------------------------------------------*/
void begin_tx(db_t hdb)
{
    execute_command(hdb, "START TRANSACTION", NULL);
    return;
}

/*-------------------------------------------------------------------------*
 * Commit Transaction Handler.                                             *
 *-------------------------------------------------------------------------*/
void commit_tx(db_t hdb)
{
    execute_command(hdb, "COMMIT", NULL);
    return;
}

/*-------------------------------------------------------------------------*
 * Application functions.                                                  *
 *-------------------------------------------------------------------------*/
void open_sequences(db_t hdb)
{
    contact_sequence = db_open_sequence(hdb, CONTACT_ID_SEQUENCE);
}

/*-------------------------------------------------------------------------*
 * Next sequence value helper function.                                    *
 *-------------------------------------------------------------------------*/
uint32_t seq_next( db_sequence_t hseq )
{
    db_seqvalue_t v;
    if (db_next_sequence(hseq, &v) == NULL)
        return 0;
    return v.int32.low;
}

/*-------------------------------------------------------------------------*
 * Add initial records to the contact and phone_number tables.             *
 *-------------------------------------------------------------------------*/
void populate_tables(db_t hdb)
{
    /* Create a few people and phone numbers */
    contact_t bob = {0, L"Bob", 0, DEFAULT_PICTURE};
    phone_t bob_mob = {0, "206-555-1000", MOBILE, -1};
    contact_t sue = {0, L"Sue", 7, DEFAULT_PICTURE};
    phone_t sue_hom = {0, "206-555-3890", HOME, 0};
    contact_t fred = {0, L"Fred", 7, DEFAULT_PICTURE}; /* Fred has many phone numbers */
    phone_t fred_hom = {0, "206-555-1308", HOME, 5};
    phone_t fred_mob = {0, "206-555-2335", MOBILE, -1};
    phone_t fred_pag = {0, "206-555-5361", PAGER, -1};

    /* Begin a database transaction */
    begin_tx(hdb);

    /* Insert a few contacts */
    bob_mob.contact_id = insert_contact(hdb, bob);
    insert_phone_number(hdb, bob_mob);
    sue_hom.contact_id = insert_contact(hdb, sue);
    insert_phone_number(hdb, sue_hom);
    fred_hom.contact_id = fred_mob.contact_id = fred_pag.contact_id = insert_contact(hdb, fred);
    insert_phone_number(hdb, fred_hom);
    insert_phone_number(hdb, fred_mob);
    insert_phone_number(hdb, fred_pag);

    commit_tx(hdb);
}

/*-------------------------------------------------------------------------*
 * Create and populate the database.                                       *
 *-------------------------------------------------------------------------*/
db_t create_database(int storage_mode, char* database_name)
{
    db_t hdb;

    if (storage_mode == DB_FILE_STORAGE) {
        db_file_storage_config_t config;
        db_file_storage_config_init(&config);

        config.file_mode = DATABASE_FILE_MODE;

        /* Create a new file storage database. */
        hdb = db_create_file_storage(database_name, &config);

        db_file_storage_config_destroy(&config);

    }
    else {
        /* Create a new memory storage database. */
        db_memory_storage_config_t config;
        db_memory_storage_config_init(&config);

        /* Specify the amount of memory to allocate for tables and indexes. */
        config.memory_storage_size = MEMORY_STORAGE_SIZE;
        fprintf(stdout, "Creating %d byte memory storage.\n", config.memory_storage_size);
        hdb = db_create_memory_storage(database_name, &config);

        db_memory_storage_config_destroy(&config);
    }

    if (hdb == NULL)
        return NULL;

    if (create_schema_sql(hdb, storage_mode == DB_FILE_STORAGE) <= 0) {
        db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);
        /* remove incomplete database */
        remove(database_name);
        return NULL;
    }

    open_sequences(hdb);

    fputs("Populating tables with sample data\n", stdout);
    populate_tables(hdb);

    return hdb;
}

/*-------------------------------------------------------------------------*
 * Open Database helper function.                                          *
 *-------------------------------------------------------------------------*/
db_t open_database(int storage_mode, char* database_name)
{
    db_t hdb;

    if (storage_mode == DB_FILE_STORAGE) {
        db_file_storage_config_t config;
        db_file_storage_config_init(&config);

        config.file_mode = DATABASE_FILE_MODE;

        /* Open an existing file storage database. */
        hdb = db_open_file_storage(database_name, &config);

        db_file_storage_config_destroy(&config);
    }
    else {
        /* Open an existing memory storage database.
         *
         * Because memory storage is destroyed when the owning process exits,
         * this will fail the first time it is called, unless connecting
         * to ITTIA DB Server.
         */
        hdb = db_open_memory_storage(database_name, NULL);
    }

    if (hdb == NULL)
        return NULL;

    open_sequences(hdb);
    return hdb;
}


/*-------------------------------------------------------------------------*
 * Insert a contact into the database.                                     *
 *-------------------------------------------------------------------------*/
contactid_t insert_contact(db_t hdb, contact_t contact)
{
    db_cursor_t sql_cursor;
    db_row_t    parameters;

    /* Bind parameter values to addresses of local variables. The size of each
     * variable is automatically calculated by the compiler and recorded
     * in each binding to prevent buffer overflows.
     *
     * NOTE: DB_BIND_VAR is used with non-pointer variables. DB_BIND_STR is
     * used with statically allocated strings and arrays. Use DB_BIND_PTR
     * if a variable points to a dynamically allocated memory location
     * whose size cannot be determined automatically by the compiler.
     */
    const db_bind_t row_def[] =
    {
        DB_BIND_VAR(0, DB_VARTYPE_UINT32, contact.id),
        DB_BIND_STR(1, DB_VARTYPE_WCHARSTR, contact.name),
        DB_BIND_VAR(2, DB_VARTYPE_UINT32, contact.ring_id),
        DB_BIND_STR(3, DB_VARTYPE_ANSISTR, contact.picture_name),
    };

    contact.id = (contactid_t)seq_next(contact_sequence);

    /*---------------------------------------------------------------------*
     * Insert new record into contact table.                               *
     * NOTE: The blob field must be handled seperately using API calls.    *
     *---------------------------------------------------------------------*/

    sql_cursor = db_prepare_sql_cursor(hdb,
        "insert into contact (id, name, ring_id, picture_name)"
        "  values ($<integer>0, $<nvarchar>1, $<integer>2, $<varchar>3)",
        0);

    if (!db_is_prepared(sql_cursor)) {
        /* Query could not be prepared, most likely due to a syntax error. */
        print_error_message("Failed to insert contact.", sql_cursor);
        db_close_cursor(sql_cursor);
        return 0;
    }

    /* Create parameter row containing values bound in row_def. */
    parameters = db_alloc_row(row_def, DB_ARRAY_DIM(row_def));

    if (db_execute(sql_cursor, parameters, NULL) == DB_FAIL) {
        print_error_message("Failed to insert contact.", sql_cursor);
    } else {
        update_contact_picture(hdb, contact.id, contact.picture_name);
    }

    db_close_cursor(sql_cursor);
    db_free_row(parameters);

    return contact.id;
}

/*-------------------------------------------------------------------------*
 * Insert a phone number into the database.                                *
 *-------------------------------------------------------------------------*/
void insert_phone_number(db_t hdb, phone_t phone)
{
    db_cursor_t sql_cursor;
    db_row_t parameters;

    sql_cursor = db_prepare_sql_cursor(hdb,
        "insert into phone_number (contact_id, number, type, speed_dial) "
        "  values ($<integer>0, $<varchar>1, $<integer>2, $<integer>3) ",
        0);

    if (!db_is_prepared(sql_cursor)) {
        print_error_message("Failed to insert phone number.", sql_cursor);
        db_close_cursor(sql_cursor);
        return;
    }

    /* Allocate the row dynamically and then manually bind each parameter to
     * a local variable. This approach demonstrates how row bindings can
     * be modified after the row is created. */
    parameters = db_alloc_row(NULL, PHONE_NFIELDS);
    dbs_bind_addr(parameters, 0, DB_VARTYPE_UINT32,  &phone.contact_id, sizeof(phone.contact_id), NULL );
    dbs_bind_addr(parameters, 1, DB_VARTYPE_ANSISTR, (void*)phone.number, sizeof(phone.number), NULL );
    dbs_bind_addr(parameters, 2, DB_VARTYPE_UINT32,  &phone.type, sizeof(phone.type), NULL );
    dbs_bind_addr(parameters, 3, DB_VARTYPE_SINT32,  &phone.speed_dial, sizeof(phone.speed_dial), NULL );

    if (db_execute(sql_cursor, parameters, NULL) == DB_FAIL) {
        print_error_message("Failed to insert phone number.", sql_cursor);
    }

    db_close_cursor(sql_cursor);
    db_free_row(parameters);
}

/*-------------------------------------------------------------------------*
 * Update a contact record in the database.                                *
 *-------------------------------------------------------------------------*/
void update_contact_name(db_t hdb, contactid_t id, const wchar_t * newname)
{
    db_cursor_t sql_cursor;
    db_row_t parameters;

    sql_cursor = db_prepare_sql_cursor(hdb,
        "update contact "
        "  set name = $<nvarchar>1 "
        "  where id = $<integer>0 ",
        0);

    if (!db_is_prepared(sql_cursor)) {
        print_error_message("Failed to update contact name.", sql_cursor);
        db_close_cursor(sql_cursor);
        return;
    }

    /* Create a managed row a buffer for each parameter defined in the
     * query. Use db_set_field_data to copy values from the local variables
     * id and newname into the parameter row's buffers.
     */
    parameters = db_alloc_param_row(sql_cursor);
    db_set_field_data(parameters, 0, DB_VARTYPE_UINT32, &id, sizeof(id));
    db_set_field_data(parameters, 1, DB_VARTYPE_WCHARSTR, newname, (db_len_t) wcslen(newname));

    if (db_execute(sql_cursor, parameters, NULL) == DB_FAIL) {
        print_error_message("Failed to update contact name.", sql_cursor);
    }

    db_close_cursor(sql_cursor);
    db_free_row(parameters);
}

void update_contact_picture(db_t hdb, contactid_t id, const char *file_name)
{
    db_cursor_t contact_cursor;
    FILE        *picture_file;
    db_blob_t   picture;
    db_row_t    r;

    db_table_cursor_t   p;

    /*-----------------------------------------------------------------*
     * FILL IN THE BLOB FIELD.                                         *
     *                                                                 *
     * This is an API-intensive process because of the BLOB type.      *
     * For all other data types we can use the SQL UPDATE command.     *
     *-----------------------------------------------------------------*
     * First we must open a new cursor on the CONTACT_TABLE.           *
     *-----------------------------------------------------------------*/
    db_table_cursor_init(&p);
    p.index = CONTACT_BY_ID;
    p.flags = DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE;
    contact_cursor = db_open_table_cursor(hdb, CONTACT_TABLE, &p);
    db_table_cursor_destroy(&p);

    /*-----------------------------------------------------------------*
     * Next, we must allocate a row to contain the record data         *
     *-----------------------------------------------------------------*/
    r = db_alloc_row(NULL, 2);

    /*-----------------------------------------------------------------*
     * Next, we must bind the id to the allocated row.                 *
     * This allows us to do a seek to find the record which was just   *
     * inserted into the CONTACT_TABLE.                                *
     *-----------------------------------------------------------------*/
    dbs_bind_addr(r, CONTACT_ID, DB_VARTYPE_UINT32, &id, sizeof(id), NULL );

    if (db_seek(contact_cursor, DB_SEEK_EQUAL, r, NULL, 1) == DB_FAIL) {
        db_close_cursor(contact_cursor);
        return;
    }

    /*-----------------------------------------------------------------*
     * Now we bind our BLOB buffer to the CONTACT_TABLE row.           *
     *-----------------------------------------------------------------*/
    dbs_bind_addr(r, CONTACT_PICTURE, DB_VARTYPE_BLOB, &picture, sizeof(picture), NULL );

    /*-----------------------------------------------------------------*
     * OPEN THE PICTURE FILE SO WE CAN STORE IT IN THE BLOB BUFFER     *
     *-----------------------------------------------------------------*/
    if (file_name[0] == '\0') {
        db_set_null(r, CONTACT_PICTURE_NAME);
        picture.blob_size = DB_FIELD_NULL;
        db_update(contact_cursor, r, NULL);
    }
    else if ((picture_file = fopen(file_name, "rb")) != NULL) {
        /*-------------------------------------------------------------*
         * Prepare BLOB variables                                      *
         *-------------------------------------------------------------*/
        int num_chunks = 0;
        int bytes_read = 0;
        char data[DATA_SIZE];   /* 1024 bytes */

        picture.chunk_data = (void*)data;

        /*-------------------------------------------------------------*
         * READ CHUNKS OF THE PICTURE FILE INTO THE BLOB FIELD         *
         *-------------------------------------------------------------*/
        while((bytes_read = (int)fread(data, 1, DATA_SIZE, picture_file)) > 0)
        {
            picture.offset = DATA_SIZE * num_chunks;
            picture.chunk_size = bytes_read;

            /*---------------------------------------------------------*
             * Call db_update() to insert the BLOB chunk into the      *
             * picture field of the CONTACT_TABLE.                     *
             *---------------------------------------------------------*/
            if (db_update(contact_cursor, r, NULL) == DB_FAIL) {
                print_error_message("Failed to update picture data.", contact_cursor);
                break;
            }
            num_chunks++;
        }
        fclose(picture_file);

    } else { /* ERROR opening picture file */
        fprintf(stderr, "ERROR: Cannot open \"%s\" for attachment.\n",
            file_name);
        db_set_null(r, CONTACT_PICTURE_NAME);
        picture.blob_size = DB_FIELD_NULL;
        db_update(contact_cursor, r, NULL);
    }

    db_free_row(r);
    db_close_cursor(contact_cursor);
}


/*-------------------------------------------------------------------------*
 * List contact records with brief information.                            *
 *-------------------------------------------------------------------------*/
void list_contacts_brief(db_t hdb)
{
    db_cursor_t sql_cursor;

    if (!execute_command(hdb,
        "select id, name"
        "  from contact"
        "  order by name",
        &sql_cursor))
    {
        printf("in list_contacts_brief function.\n");
    }
    else {
    db_row_t        r;
    types_t         v;

        r = db_alloc_cursor_row(sql_cursor);

        for (db_seek_first(sql_cursor); !db_eof(sql_cursor); db_seek_next(sql_cursor)) {
            if (!db_fetch(sql_cursor, r, NULL))
                break;

            /* id */
            db_get_field_data(r, 0, DB_VARTYPE_UINT64, &v, sizeof(v.u64));
            printf("%lld\t", v.u64);

            /* name */
            db_get_field_data(r, 1, DB_VARTYPE_WCHARSTR, &v, sizeof(v.w));
            printf("%ls\n", v.w);
        }

        db_free_row(r);
        db_close_cursor(sql_cursor);
                        }

    return;
}

/*-------------------------------------------------------------------------*
 * List contact records.                                                   *
 *-------------------------------------------------------------------------*/
void list_contacts(db_t hdb, int sort)
{
    db_cursor_t sql_cursor;

    const char* query_by_name =
        "select A.id, A.name, A.ring_id, A.picture_name, B.number, B.type, B.speed_dial"
        "  from contact A, phone_number B"
        "  where A.id = B.contact_id"
        "  order by A.name, B.type";
    const char* query_by_id =
        "select A.id, A.name, A.ring_id, A.picture_name, B.number, B.type, B.speed_dial"
        "  from contact A, phone_number B"
        "  where A.id = B.contact_id"
        "  order by A.id, B.type";
    const char* query_by_ring_id_name =
        "select A.id, A.name, A.ring_id, A.picture_name, B.number, B.type, B.speed_dial"
        "  from contact A, phone_number B"
        "  where A.id = B.contact_id"
        "  order by A.ring_id, A.name, B.type";
    const char* query;

    /* Choose the query for the selected sort order. */
    switch (sort) {
        case 0:
            query = query_by_id;
                        break;
        case 1:
            query = query_by_name;
                        break;
        case 2:
            query = query_by_ring_id_name;
                        break;
        default:
            return;
    }

    if (!execute_command(hdb, query, &sql_cursor))
    {
        printf("in list_contacts function.\n");
    }
    else {
        db_row_t        r;
        uint64_t        prev_id = 0;
        types_t         v;
        db_len_t        len;

        r = db_alloc_cursor_row(sql_cursor);

        for (db_seek_first(sql_cursor); !db_eof(sql_cursor); db_seek_next(sql_cursor)) {
            if (!db_fetch(sql_cursor, r, NULL))
                        break;

            /* id */
            db_get_field_data(r, 0, DB_VARTYPE_UINT64, &v, sizeof(v.u64));

            /* Display each contact's details only once. Relies on ordering
             * by contact first. */
            if  (v.u64 != prev_id) {
                printf("%sId: %lld\n", prev_id == 0 ? "" : "\n", v.u64);
                prev_id = v.u64;

                /* name */
                db_get_field_data(r, 1, DB_VARTYPE_WCHARSTR, &v, sizeof(v.w));
                printf("Name: %ls\n", v.w);

                /* ring_tone */
                db_get_field_data(r, 2, DB_VARTYPE_SINT64, &v, sizeof(v.i64));
                printf("Ring tone id: %lld\n", v.i64);

                /* picture_name */
                len = db_get_field_data(r, 3, DB_VARTYPE_ANSISTR, &v, sizeof(v.s));
                printf("Picture name: %s\n", (len > 0) ? v.s : "");
            }

            /* phone_number */
            db_get_field_data(r, 4, DB_VARTYPE_ANSISTR, &v, sizeof(v.s));
                        printf("Phone number: %s ", v.s );

            /* phone type */
            db_get_field_data(r, 5, DB_VARTYPE_UINT64, &v, sizeof(v.u64));
                        switch (v.u64) {
                            case HOME:   printf("(Home");   break;
                            case MOBILE: printf("(Mobile"); break;
                            case WORK:   printf("(Work");   break;
                            case FAX:    printf("(Fax");    break;
                            case PAGER:  printf("(Pager");  break;
                        }

            /* speed dial */
            db_get_field_data(r, 6, DB_VARTYPE_SINT64, &v, sizeof(v.i64));
                        printf((v.i64 >= 0) ? ", speed dial %lld)\n" : ")\n", v.i64);
                }
        printf("\n");

        db_free_row(r);
        db_close_cursor(sql_cursor);
        }

    return;
}

/*-------------------------------------------------------------------------*
 * Delete contact records.                                                 *
 *-------------------------------------------------------------------------*/
void remove_contact(db_t hdb, contactid_t contactid)
{
    db_cursor_t sql_cursor;
    db_row_t    parameters;

    /* Bind parameter 0 to contactid. */
    const db_bind_t parameters_bind[] = {
        DB_BIND_VAR(0, DB_VARTYPE_UINT32, contactid),
    };

    parameters = db_alloc_row(parameters_bind, DB_ARRAY_DIM(parameters_bind));

    sql_cursor = db_prepare_sql_cursor(hdb,
        "delete from phone_number"
        "  where contact_id = $<integer>0 ", 0);

    if (db_is_prepared(sql_cursor) <= 0) {
        print_error_message("Failed to remove contact's .", sql_cursor);
        db_close_cursor(sql_cursor);
        db_free_row(parameters);
        return;
    }

    if (db_execute(sql_cursor, parameters, NULL) == DB_FAIL) {
        print_error_message("Failed to remove contact.", sql_cursor);
    }

    sql_cursor = db_prepare_sql_cursor(hdb,
        "delete from contact"
        "  where id = $<integer>0 ", 0);

    if (db_is_prepared(sql_cursor) <= 0) {
        print_error_message("Failed to remove contact.", sql_cursor);
        db_close_cursor(sql_cursor);
        db_free_row(parameters);
        return;
    }

    if (db_execute(sql_cursor, parameters, NULL) == DB_FAIL) {
        print_error_message("Failed to remove contact.", sql_cursor);
    }

    db_close_cursor(sql_cursor);
    db_free_row(parameters);
}


/*-------------------------------------------------------------------------*
 * Retrieve the name of the picture file                                   *
 *-------------------------------------------------------------------------*/
void get_picture_name(db_t hdb, contactid_t contactid, db_ansi_t *file_name, db_len_t len)
{
    db_cursor_t sql_cursor;
    db_row_t    parameters;
    db_len_t    ind;

    /* Bind parameter 0 to contactid. */
    const db_bind_t parameters_bind[] = {
        DB_BIND_VAR(0, DB_VARTYPE_UINT32, contactid),
    };
    /* Bind select result 0 to the file_name buffer. */
    const db_bind_t result_bind[] = {
        DB_BIND_PTR_IND(0, DB_VARTYPE_ANSISTR, file_name, len, &ind),
    };

    sql_cursor = db_prepare_sql_cursor(hdb, "select picture_name from contact where id = $<integer>0", 0);

    if (!db_is_prepared(sql_cursor)) {
        print_error_message("Failed to get picture name.", sql_cursor);
        db_close_cursor(sql_cursor);
        return;
    }

    parameters = db_alloc_row(parameters_bind, DB_ARRAY_DIM(parameters_bind));

    if (db_execute(sql_cursor, parameters, NULL) == DB_FAIL) {
        print_error_message("Failed to get picture name.", sql_cursor);
    }
    else {
        /* Position cursor on the first returned row. */
        if (db_seek_first(sql_cursor) != DB_FAIL) {
            /* Fetch result into file_name buffer. */
            db_row_t result = db_alloc_row(result_bind, DB_ARRAY_DIM(result_bind));
            db_fetch(sql_cursor, result, NULL);
            /* If picture name is NULL, set file name to an empty string. */
            if (ind == DB_FIELD_NULL)
                file_name[0] = '\0';
            db_free_row(result);
        }
    }

    db_close_cursor(sql_cursor);
    db_free_row(parameters);
}

/*-------------------------------------------------------------------------*
 * Export the picture file to disk                                         *
 *                                                                         *
 * This is an API intensive process because of the BLOB type.              *
 * For all other data types we can use the easier data retrieval method    *
 * demonstrated in the get_picture_name() function.                        *
 *-------------------------------------------------------------------------*/
void export_picture(db_t hdb, contactid_t contactid, db_ansi_t *file_name)
{
    db_cursor_t sql_cursor;
    db_row_t    parameters;
    db_blob_t   blob;

    /* Bind parameter 0 to contactid. */
    const db_bind_t parameters_bind[] = {
        DB_BIND_VAR(0, DB_VARTYPE_UINT32, contactid),
    };
    /* Bind select result 0 to blob. */
    const db_bind_t result_bind[] = {
        DB_BIND_VAR(0, DB_VARTYPE_BLOB, blob),
    };

    sql_cursor = db_prepare_sql_cursor(hdb, "select picture from contact where id = $<integer>0", 0);

    if (!db_is_prepared(sql_cursor)) {
        print_error_message("Failed to get picture.", sql_cursor);
        db_close_cursor(sql_cursor);
        return;
    }

    parameters = db_alloc_row(parameters_bind, DB_ARRAY_DIM(parameters_bind));

    if (db_execute(sql_cursor, parameters, NULL) == DB_FAIL) {
        print_error_message("Failed to get picture.", sql_cursor);
    }
    else {
        /* Position cursor on the first returned row. */
        if (db_seek_first(sql_cursor) != DB_FAIL) {
            db_row_t result = db_alloc_row(result_bind, DB_ARRAY_DIM(result_bind));

            /*---------------------------------------------------------*
             * Prepare BLOB variables.                                 *
             *---------------------------------------------------------*/
            memset(&blob, 0, sizeof(blob));
            blob.offset = 0;
            blob.chunk_data = NULL;
            blob.chunk_size = 0;
            blob.blob_size  = 0;

            /*---------------------------------------------------------*
             * fetch the blob data structure.                          *
             *---------------------------------------------------------*/
            if  (db_fetch(sql_cursor, result, NULL)) {
                FILE* picture_file;

                /*-----------------------------------------------------*
                 * Open the output file.                               *
                 *-----------------------------------------------------*/
                if ((picture_file = fopen(file_name, "wb")) != NULL) {
                    char buffer[DATA_SIZE];

                    blob.chunk_data = buffer;
                    blob.chunk_size = DATA_SIZE;

                    /*-----------------------------------------------------*
                     * Read BLOB field and export contents to disk.        *
                     *-----------------------------------------------------*/
                    for (blob.offset = 0; blob.offset < blob.blob_size; blob.offset += DATA_SIZE) {
                        if  (db_fetch(sql_cursor, result, NULL)) {
                            fwrite(blob.chunk_data, blob.actual_size, 1, picture_file);
                        }
                    }

                    fclose(picture_file);

                } else {
                    printf("ERROR: Cannot open %s\n", file_name);
                }
            }

            db_free_row(result);
        }
    }

    db_close_cursor(sql_cursor);
    db_free_row(parameters);
}
