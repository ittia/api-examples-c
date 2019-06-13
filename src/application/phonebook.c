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

/** @file phonebook.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 */

#include "phonebook_c.h"
#include "dbs_schema.h"
#include "dbs_error_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Schema is defined by data structures in phonebook_c_schema.c */
extern dbs_schema_def_t phonebook_schema;

extern dbs_schema_def_t nopicture_phonebook_schema;


/* Local functions. */
db_result_t update_contact_picture(db_t hdb, db_cursor_t contact_cursor, const char *file_name);

/**
 * This relative binding array describes the relationship between the
 * phone_number table and the phone_t struct data type. It is allocated
 * statically so that it can be used by multiple functions.
 */
static const db_bind_t phone_number_binds[] =
{
    DB_BIND_STRUCT_MEMBER(PHONE_CONTACT_ID, DB_VARTYPE_UINT32,  phone_t, contact_id),
    DB_BIND_STRUCT_MEMBER(PHONE_NUMBER,     DB_VARTYPE_ANSISTR, phone_t, number),
    DB_BIND_STRUCT_MEMBER(PHONE_TYPE,       DB_VARTYPE_UINT32,  phone_t, type),
    DB_BIND_STRUCT_MEMBER(PHONE_SPEED_DIAL, DB_VARTYPE_SINT32,  phone_t, speed_dial),
};

/**
 * Helper function to print an error message and current error code.
 */
void print_error_message(char * message, db_cursor_t cursor)
{
    if (get_db_error() == DB_NOERROR) {
        if (message != NULL)
            fprintf(stderr, "ERROR: %s\n", message);
    } else {
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



/**
 * Begin transaction handler.
 */
void begin_tx(db_t hdb)
{
    db_begin_tx(hdb, DB_DEFAULT_ISOLATION | DB_LOCK_SHARED);
    return;
}

/**
 * Commit transaction handler.
 */
void commit_tx(db_t hdb)
{
    db_commit_tx(hdb, DB_FORCED_COMPLETION);
    return;
}


/**
 * Next sequence value helper function.
 */
uint32_t seq_next( db_sequence_t hseq )
{
    db_seqvalue_t v;

    if (db_next_sequence(hseq, &v) == NULL)
        return 0;
    return v.int32.low;
}

/* Sequences are made available in global scope for convenience. */
db_sequence_t contact_sequence = NULL;
db_sequence_t phone_sequence = NULL;

/**
 * Insert a contact into the database.
 */
contactid_t insert_contact(db_t hdb, contact_t contact)
{
    db_row_t    r;
    db_cursor_t contact_cursor;

    /* Use static array of bindings corresponding to the contact table schema. */
    const db_bind_t row_def[] =
    {
        DB_BIND_VAR(CONTACT_ID, DB_VARTYPE_UINT32, contact.id),
        DB_BIND_STR(CONTACT_NAME, DB_VARTYPE_WCHARSTR, contact.name),
        DB_BIND_VAR(CONTACT_RING_ID, DB_VARTYPE_UINT32, contact.ring_id),
        DB_BIND_STR(CONTACT_PICTURE_NAME, DB_VARTYPE_ANSISTR, contact.picture_name),
    };

    db_table_cursor_t p;

    db_table_cursor_init(&p);
    p.index = CONTACT_BY_ID;
    p.flags = DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE;
    contact_cursor = db_open_table_cursor(hdb, CONTACT_TABLE, &p);
    db_table_cursor_destroy(&p);

    r = db_alloc_row(row_def, DB_ARRAY_DIM(row_def));

    contact.id = (contactid_t)seq_next(contact_sequence);

    /* Insert a row into the contact table using data in the variables
     * bound in the row definition. DB_INSERT_SEEK_NEW will position
     * the cursor on the inserted row after insert.
     */
    db_insert(contact_cursor, r, NULL, DB_INSERT_SEEK_NEW);

    /* Update the contact's picture from a file. */
    if (update_contact_picture(hdb, contact_cursor, contact.picture_name) == DB_FAIL) {
        /* Loading picture failed: set picture_name to NULL. */
        db_set_null(r, CONTACT_PICTURE_NAME);

        /* When an update is performed, all bound fields are copied into the
         * database. Since only the picture_name field has changed, first
         * remove the bindings for all other fields. */
        db_unbind_field(r, CONTACT_ID);
        db_unbind_field(r, CONTACT_NAME);
        db_unbind_field(r, CONTACT_RING_ID);
        db_update(contact_cursor, r, NULL);
    }

    /* Row contents are now in the database, so the row object is no
     * longer needed. */
    db_free_row(r);

    db_close_cursor(contact_cursor);
    return contact.id;
}

void insert_phone_number(db_t hdb, phone_t phone)
{
    /* Use manual binding. */
    db_row_t    r;
    db_cursor_t phone_cursor;

    db_table_cursor_t p;

    db_table_cursor_init(&p);
    p.index = PHONE_BY_CONTACT_ID;
    p.flags = DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE;
    phone_cursor = db_open_table_cursor(hdb, PHONE_TABLE, &p);
    db_table_cursor_destroy(&p);

    /* Allocate the row dynamically and then manually bind each field to
     * a local variable. This approach demonstrates how row bindings can
     * be modified after the row is created. */
    r = db_alloc_row(NULL, PHONE_NFIELDS);
    dbs_bind_addr(r, PHONE_CONTACT_ID, DB_VARTYPE_UINT32,  &phone.contact_id, sizeof(phone.contact_id), NULL );
    dbs_bind_addr(r, PHONE_NUMBER,     DB_VARTYPE_ANSISTR, (void*)phone.number, sizeof(phone.number), NULL );
    dbs_bind_addr(r, PHONE_TYPE,       DB_VARTYPE_UINT32,  &phone.type, sizeof(phone.type), NULL );
    dbs_bind_addr(r, PHONE_SPEED_DIAL, DB_VARTYPE_SINT32,  &phone.speed_dial, sizeof(phone.speed_dial), NULL );

    if (db_insert(phone_cursor, r, NULL, 0) == DB_FAIL)
        print_error_message("Error in insert_phone_number()", phone_cursor);

    db_close_cursor(phone_cursor);
    db_free_row(r);
}

void update_contact_name(db_t hdb, contactid_t id, const wchar_t * newname)
{
    /* Use manual binding both to seek and update. */
    db_cursor_t c;
    db_row_t r;

    db_table_cursor_t p;

    db_table_cursor_init(&p);
    /* Use an index that begins with the search criteria to avoid a table scan. */
    p.index = CONTACT_BY_ID;
    p.flags = DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE;
    c = db_open_table_cursor(hdb, CONTACT_TABLE, &p);
    db_table_cursor_destroy(&p);

    /* Allocate space to bind one field to this row at a time. */
    r = db_alloc_row(NULL, 1);

    /* Seek to contact id. */
    dbs_bind_addr(r, CONTACT_ID, DB_VARTYPE_UINT32, &id, sizeof(id), NULL );
    db_add_filter(c, DB_SEEK_EQUAL, r, NULL, NULL, 0);
    if (db_seek_first(c) == DB_FAIL)
        return;
    /* c is now positioned on the row identified by contact_id. Remove the
     * binding for this field to prevent it from being included in the
     * subsequent update. */
    db_unbind_field( r, CONTACT_ID );

    /* Update contact name only. */
    dbs_bind_addr(r, CONTACT_NAME, DB_VARTYPE_WCHARSTR, (void*)newname, wcslen(newname) * sizeof(wchar_t), NULL );
    if (db_update(c, r, NULL) == DB_FAIL)
        print_error_message("Error in update_contact_name()", c);

    db_close_cursor(c);
    db_free_row(r);
    db_close_cursor(c);
}

/*
 * Update the picture data for an existing contact.
 *
 * Picture is a blob field, which can hold arbitrarily large data. A blob
 * is updated by breaking the file into chunks that are small enough to fit
 * in memory and calling db_update() for each chunk.
 */
db_result_t update_contact_picture(db_t hdb, db_cursor_t contact_cursor, const char *file_name)
{
    db_blob_t picture;
    FILE *picture_file;

    /* Bind to the picture field only, since db_update will be called many times. */
    const db_bind_t row_def[] =
    {
        DB_BIND_VAR(CONTACT_PICTURE, DB_VARTYPE_BLOB, picture),
    };

    /* Open picture file */
    if (file_name[0] == '\0') {
        return DB_OK;
    }
    else if ((picture_file = fopen(file_name, "rb")) != NULL) {
        db_row_t r = db_alloc_row(row_def, DB_ARRAY_DIM(row_def));

        /* Prepare BLOB variables */
        int num_chunks = 0;
        int bytes_read = 0;
        char data[DATA_SIZE];
                picture.chunk_data = (void*)data;

        /* Store picture into BLOB field */
        while((bytes_read = (int)fread(data, 1, DATA_SIZE, picture_file)) > 0)
        {
            picture.offset = DATA_SIZE * num_chunks;
            picture.chunk_size = bytes_read;
            /* Update all fields bound to r. */
            if (db_update(contact_cursor, r, NULL) == DB_FAIL) {
                print_error_message("Error updating contact picture.", contact_cursor);
                break;
            }
            num_chunks++;
        }

        db_free_row(r);

        fclose(picture_file);

        return DB_OK;
    } else {
        fprintf(stderr, "Cannot open file \"%s\" for attachment.\n",
            file_name);

        return DB_FAIL;
    }
}

void list_contacts_brief(db_t hdb)
{
    db_cursor_t contact_cursor;
    db_row_t contact_row;

    /* Define a cursor that is sorted by the fields in the "by_name" index.
     * The cursor will have the ability to scan the table forward, cannot
     * modify the table, and can share row locks with other read-only
     * connections that access this table. */
    static const db_table_cursor_t contact_cursor_def =
    {
        CONTACT_BY_NAME, /* index */
        DB_SCAN_FORWARD | DB_LOCK_SHARED /* flags */
    };

    /* Open a cursor to traverse the contact table. */
    contact_cursor = db_open_table_cursor(hdb, CONTACT_TABLE, &contact_cursor_def );

    /* Create a managed row to store fields when they are fetched from the
     * table. The cursor argument is used only to enumerate the fields.
     * This row could be used again later with another cursor that has the
     * same list of fields. */
    contact_row = db_alloc_cursor_row( contact_cursor );

    /* Iterate over each row in the cursor. */
    for (db_seek_first(contact_cursor); !db_eof(contact_cursor) ; db_seek_next(contact_cursor))
    {
        uint32_t contact_id;
        wchar_t contact_name[MAX_CONTACT_NAME + 1];

        /* Fetch full row data to contact_row. */
        db_fetch(contact_cursor, contact_row, NULL);

        /* Extract field data from the managed row. Type conversion may occur. */
        db_get_field_data(contact_row, CONTACT_ID, DB_VARTYPE_UINT32,
            &contact_id, sizeof(contact_id));
        db_get_field_data(contact_row, CONTACT_NAME, DB_VARTYPE_WCHARSTR,
            contact_name, sizeof(contact_name));

        printf("%lu\t%ls\n", (unsigned long)contact_id, contact_name);
    }

    db_close_cursor(contact_cursor);
    db_free_row(contact_row);
}

void list_contacts(db_t hdb, int sort)
{
    db_cursor_t contact_cursor, phone_cursor;
    db_row_t contact_row, phone_row;
    phone_t phone;
    contactid_t contactid;

    /* Use static cursor definitions for brevity. */
    static const db_table_cursor_t contact_cursor_def =
    {
        CONTACT_BY_NAME, /* index */
        DB_SCAN_FORWARD | DB_LOCK_SHARED /* flags */
    };
    static const db_table_cursor_t phone_cursor_def =
    {
        PHONE_BY_CONTACT_ID, /* index */
        DB_SCAN_FORWARD | DB_LOCK_SHARED /* flags */
    };

    /* Contact sort order choices. */
    static const db_indexfield_t sort_by_id[] = {
        { CONTACT_ID, 0 }
    };
    static const db_indexfield_t sort_by_name[] = {
        { CONTACT_NAME, 0 }
    };
    static const db_indexfield_t sort_by_ring_id_name[] = {
        { CONTACT_RING_ID, 0 },
        { CONTACT_NAME, 0 }
    };

    /* Open cursors to traverse the contact and phone tables. */
    contact_cursor = db_open_table_cursor(hdb, CONTACT_TABLE, &contact_cursor_def );
    phone_cursor = db_open_table_cursor(hdb, PHONE_TABLE, &phone_cursor_def );

    /* Create a managed row with buffers for all fields in the contact_cursor.
     * Because no local variables are associated with these fields, the
     * data must be accessed using db_get_field_data and db_set_field_data.
     * This technique is useful when the schema is not known at compile.
     */
    contact_row = db_alloc_cursor_row( contact_cursor );

    /* Use relative row: data will be fetched to an object. */
    phone_row = db_alloc_row(phone_number_binds, DB_ARRAY_DIM(phone_number_binds));

    /* Bind a single variable (contactid) to both rows. The CONTACT_ID field in
     * contact_row, PHONE_CONTACT_ID field in phone_row and the local variable
     * contactid will all share the same memory. Changes to one will be
     * immediately reflected in both others.
     */
    dbs_bind_addr( contact_row, CONTACT_ID, DB_VARTYPE_UINT32, &contactid, sizeof(contactid), NULL);
    dbs_bind_addr( phone_row, PHONE_CONTACT_ID, DB_VARTYPE_UINT32, &contactid, sizeof(contactid), NULL);

    /* Sort the contact cursor. */
    switch (sort) {
        case CONTACT_ID:
            /* Sort contacts by ID. */
            db_sort(contact_cursor, sort_by_id, DB_ARRAY_DIM(sort_by_id));
            break;
        case CONTACT_NAME:
            /* Sort contacts by name. */
            /* Since the index CONTACT_BY_NAME was used, this has no effect. */
            db_sort(contact_cursor, sort_by_name, DB_ARRAY_DIM(sort_by_name));
            break;
        case CONTACT_RING_ID:
            /* Sort contacts by ring_id, then by name if ring_id values are the same. */
            db_sort(contact_cursor, sort_by_ring_id_name, DB_ARRAY_DIM(sort_by_ring_id_name));
            break;
        default:
            return;
    }

    for (db_seek_first(contact_cursor); !db_eof(contact_cursor) ; db_seek_next(contact_cursor))
    {
        const db_indexfield_t phone_filter_field = { PHONE_CONTACT_ID, 0 };
        int32_t ring_tone;
        wchar_t contact_name[MAX_CONTACT_NAME + 1];
        db_ansi_t file_name[MAX_FILE_NAME + 1];
        /* Fetch full row data to contact_row. */
        db_fetch(contact_cursor, contact_row, NULL);

        /* Extract field data from the managed row, doing conversion if
         * necessary. */
        db_get_field_data(contact_row, CONTACT_NAME, DB_VARTYPE_WCHARSTR,
            contact_name, sizeof(contact_name));

        /* Access field data directly using db_get_field_buffer(). */
        printf("Id: %lu\n", (long unsigned)*(uint32_t*)db_get_field_buffer(contact_row, CONTACT_ID));
        printf("Name: %ls\n", contact_name);

        /* Extract field data using db_get_field_data() */
        db_get_field_data(contact_row, CONTACT_RING_ID, DB_VARTYPE_SINT32, &ring_tone, sizeof(ring_tone) );
        printf("Ring tone id: %lu\n", (long unsigned) ring_tone);

        if (!db_is_null(contact_row, CONTACT_PICTURE_NAME)) {
            db_get_field_data(contact_row, CONTACT_PICTURE_NAME,
                DB_VARTYPE_ANSISTR, file_name,
                db_get_field_size(contact_row, CONTACT_PICTURE_NAME));
            printf("Picture name: %s\n", file_name);
        }

        /* Limit the range of phone_cursor to rows matching the value of
         * PHONE_CONTACT_ID in phone_row. This value was automatically set when
         * CONTACT_ID was fetched to contact_row because both fields are bound
         * to the same memory location. */
        db_remove_filters(phone_cursor);
        db_add_filter(phone_cursor, DB_SEEK_EQUAL, phone_row, &phone, &phone_filter_field, 1);

        for (db_seek_first(phone_cursor); !db_eof(phone_cursor); db_seek_next(phone_cursor)) {
            /* Each row in the range is fetched to phone using the relative
             * bindings in phone_row. */
            db_fetch(phone_cursor, phone_row, &phone);

            /* print the phone row */
            printf("Phone number: %s (", phone.number);
            switch (phone.type) {
                case HOME:   fputs("Home", stdout); break;
                case MOBILE: fputs("Mobile", stdout); break;
                case WORK:   fputs("Work", stdout); break;
                case FAX:    fputs("Fax", stdout); break;
                case PAGER:  fputs("Pager", stdout); break;
            }

            if (phone.speed_dial >= 0)
                printf(", speed dial %d", phone.speed_dial);;
            fputs(")\n", stdout);
        }
        printf("\n");
    }

    db_close_cursor(contact_cursor);
    db_close_cursor(phone_cursor);

    db_free_row(contact_row);
    db_free_row(phone_row);
}

void remove_contact(db_t hdb, contactid_t contactid)
{
    db_cursor_t contact_cursor, phone_cursor;
    db_row_t contact_row, phone_row;

    static const db_table_cursor_t contact_cursor_def =
    {
        CONTACT_BY_ID, /* index */
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE /* flags */
    };
    static const db_table_cursor_t phone_cursor_def =
    {
        PHONE_BY_CONTACT_ID, /* index */
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE /* flags */
    };

    /* Bind the contactid variable to both rows to use like a foreign key */
    const db_bind_t contact_bind[] = {
        DB_BIND_VAR(CONTACT_ID, DB_VARTYPE_UINT32, contactid),
    };
    const db_bind_t phone_bind[] = {
        DB_BIND_VAR(PHONE_CONTACT_ID, DB_VARTYPE_UINT32, contactid),
    };

    contact_cursor = db_open_table_cursor(hdb, CONTACT_TABLE, &contact_cursor_def );
    phone_cursor = db_open_table_cursor(hdb, PHONE_TABLE, &phone_cursor_def );

    contact_row = db_alloc_row(contact_bind, 1);
    phone_row = db_alloc_row(phone_bind, 1);

    /* Search for the contact */
    db_add_filter(contact_cursor, DB_SEEK_EQUAL, contact_row, NULL, NULL, 0);
    if (db_seek_first(contact_cursor) == DB_OK) {
        /* Remove related telephone numbers. */
        db_add_filter(phone_cursor, DB_SEEK_EQUAL, phone_row, NULL, NULL, 0);
        db_seek_first(phone_cursor);
        while (!db_eof(phone_cursor))
        {
            /* Delete row and move cursor to the next row matching the filter. */
            db_delete(phone_cursor, DB_DELETE_SEEK_NEXT);
        }

        /* Remove the current contact after referenced fields are removed. */
        db_delete(contact_cursor, 0);
    } else {
        printf("Could not find contact with id %d\n", contactid);
    }

    db_close_cursor(contact_cursor);
    db_close_cursor(phone_cursor);

    db_free_row(contact_row);
    db_free_row(phone_row);
}

void get_picture_name(db_t hdb, contactid_t contactid, db_ansi_t *file_name, db_len_t len)
{
    db_cursor_t contact_cursor;
    db_row_t contact_row;
    db_len_t    ind;

    static const db_table_cursor_t contact_cursor_def =
    {
        CONTACT_BY_ID, /* index */
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE /* flags */
    };

    contact_cursor = db_open_table_cursor(hdb, CONTACT_TABLE, &contact_cursor_def );

    /* manually bind the row */
    contact_row = db_alloc_row(NULL, 2);
    dbs_bind_addr(contact_row, CONTACT_ID, DB_VARTYPE_UINT32, &contactid, sizeof(contactid), NULL);
    dbs_bind_addr(contact_row, CONTACT_PICTURE_NAME, DB_VARTYPE_ANSISTR, file_name, len, &ind);

    /* search for the contact */
    if (db_seek(contact_cursor, DB_SEEK_EQUAL, contact_row, NULL, 1) == DB_OK) {
        /* retrieve the file_name */
        db_fetch(contact_cursor, contact_row, NULL);
        /* If picture name is NULL, set file name to an empty string. */
        if (ind == DB_FIELD_NULL)
            file_name[0] = '\0';
    }
    else
        printf("Could not find contact with id %d\n", contactid);

    db_close_cursor(contact_cursor);
    db_free_row(contact_row);
}

void export_picture(db_t hdb, contactid_t contactid, db_ansi_t *file_name)
{
    db_cursor_t contact_cursor;
    db_row_t contact_row, contact_row_picture;
    db_blob_t blob = { 0 };

    FILE *picture_file;

    static const db_table_cursor_t contact_cursor_def =
    {
        CONTACT_BY_ID, /* index */
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE /* flags */
    };

    contact_cursor = db_open_table_cursor(hdb, CONTACT_TABLE, &contact_cursor_def );

    /* manually bind ID and PICTURE fields of the row  */
    contact_row = db_alloc_row(NULL, 2);
    dbs_bind_addr(contact_row, CONTACT_ID, DB_VARTYPE_UINT32, &contactid, sizeof(contactid), NULL);
    dbs_bind_addr(contact_row, CONTACT_PICTURE, DB_VARTYPE_BLOB, &blob, sizeof(blob), NULL);

    contact_row_picture = db_alloc_row(NULL, 1);
    dbs_bind_addr(contact_row_picture, CONTACT_PICTURE, DB_VARTYPE_BLOB, &blob, sizeof(blob), NULL);

    /* search for the contact */
    if (db_seek(contact_cursor, DB_SEEK_EQUAL, contact_row, NULL, 1) == DB_OK) {

        /* Prepare BLOB variables */
        memset(&blob, 0, sizeof(blob));
        blob.offset = 0;
        blob.chunk_data = NULL;
        blob.chunk_size = 0;
        blob.blob_size  = 0;

        /* fetch file_name and blob.blob_size */
        db_fetch(contact_cursor, contact_row, NULL);

        /* Open file */
        if ((picture_file = fopen(file_name, "wb")) != NULL) {

            char buffer[DATA_SIZE];

            blob.chunk_data = buffer;
            blob.chunk_size = DATA_SIZE;

            /* Export file from BLOB to disk */
            for(blob.offset = 0; blob.offset < blob.blob_size; blob.offset += DATA_SIZE)
            {
                db_fetch(contact_cursor, contact_row_picture, NULL);
                fwrite(blob.chunk_data, blob.actual_size, 1, picture_file);
            }

            fclose(picture_file);

        } else {
            fprintf(stderr, "Cannot open %s\n", file_name);
        }

    } else {
        printf("Could not find contact with id %d\n", contactid);
    }

    db_close_cursor(contact_cursor);
    db_free_row(contact_row);
}

void open_sequences(db_t hdb)
{
    contact_sequence = db_open_sequence(hdb, CONTACT_ID_SEQUENCE);
}

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
    db_begin_tx(hdb, DB_DEFAULT_ISOLATION | DB_LOCK_EXCLUSIVE);

    /* Insert a few contacts */
    bob_mob.contact_id = insert_contact(hdb, bob);
    insert_phone_number(hdb, bob_mob);
    sue_hom.contact_id = insert_contact(hdb, sue);
    insert_phone_number(hdb, sue_hom);
    fred_hom.contact_id = fred_mob.contact_id = fred_pag.contact_id = insert_contact(hdb, fred);
    insert_phone_number(hdb, fred_hom);
    insert_phone_number(hdb, fred_mob);
    insert_phone_number(hdb, fred_pag);

    db_commit_tx(hdb, DB_FORCED_COMPLETION);
}

/**
 * Initialize and populate the database.
 */
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

        if (hdb == NULL)
            return NULL;

        if (dbs_create_schema(hdb, &phonebook_schema) < 0) {
            db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);
            /* Remove incomplete database. */
            remove(database_name);
            return NULL;
        }
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

        if (hdb == NULL)
            return NULL;

        if (dbs_create_schema(hdb, &nopicture_phonebook_schema) < 0) {
            db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);
            /* Remove incomplete database. */
            remove(database_name);
            return NULL;
        }
    }

    open_sequences(hdb);

    fputs("Populating tables with sample data\n", stdout);
    populate_tables(hdb);

    return hdb;
}

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

    if (!dbs_check_schema(hdb, &phonebook_schema)) {
        fprintf(stderr, "WARNING: schema conflict in %s.", database_name);
    }

    open_sequences(hdb);

    return hdb;
}

