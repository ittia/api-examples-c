/**************************************************************************/
/*                                                                        */
/*      Copyright (c) 2005-2015 by ITTIA L.L.C. All rights reserved.      */
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

/** @file unicode_character_strings.c
 *
 * Command-line example program demonstrating the ITTIA DB C API.
 */

#include "dbs_schema.h"
#include "dbs_error_info.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <ittia/db.h>

#define EXAMPLE_DATABASE "unicode_character_strings.ittiadb"

// Declare DB schema inline

#define STORAGE_TABLE           "storage"
#define STORAGE_PKEY_INDEX_NAME  "pkey_storage"

#define ID_FNO 0
#define EN_FNO 1
#define JP_FNO 2
#define FR_FNO 3
#define ES_FNO 4
#define RU_FNO 5

#define MAX_TEXT_LEN 50

static db_fielddef_t storage_fields[] =
{
    { ID_FNO, "id",      DB_COLTYPE_SINT32,      0,                0, DB_NOT_NULL, 0 },
    { EN_FNO, "en",      DB_COLTYPE_UTF8STR,     MAX_TEXT_LEN, 0, DB_NULLABLE, 0 },
    { JP_FNO, "jp",      DB_COLTYPE_UTF8STR,     MAX_TEXT_LEN, 0, DB_NULLABLE, 0 },
    { FR_FNO, "fr",      DB_COLTYPE_UTF8STR,     MAX_TEXT_LEN, 0, DB_NULLABLE, 0 },
    { ES_FNO, "es",      DB_COLTYPE_UTF8STR,     MAX_TEXT_LEN, 0, DB_NULLABLE, 0 },
    { RU_FNO, "ru",      DB_COLTYPE_UTF8STR,     MAX_TEXT_LEN, 0, DB_NULLABLE, 0 },
};
static db_indexfield_t storage_pkey_fields[] = { { ID_FNO },  };

static db_indexdef_t storage_indexes[] =
{
    { DB_ALLOC_INITIALIZER(),       /* db_alloc */
      DB_INDEXTYPE_DEFAULT,         /* index_type */
      STORAGE_PKEY_INDEX_NAME,      /* index_name */
      DB_PRIMARY_INDEX,             /* index_mode */
      DB_ARRAY_DIM(storage_pkey_fields),  /* nfields */
      storage_pkey_fields },              /* fields  */
};

/* Database schemas. */
static db_tabledef_t tables[] = 
{
    { 
        DB_ALLOC_INITIALIZER(),
        DB_TABLETYPE_DEFAULT,
        STORAGE_TABLE,
        DB_ARRAY_DIM(storage_fields),
        storage_fields,
        DB_ARRAY_DIM(storage_indexes),  // Indexes array size
        storage_indexes,                // Indexes array
        0, NULL,
    },
};

dbs_schema_def_t db_schema = 
{
    DB_ARRAY_DIM(tables),
    tables
};

typedef struct {
    uint32_t    id;
    char        en[MAX_TEXT_LEN + 1];
    char        jp[MAX_TEXT_LEN + 1]; 
    char        fr[MAX_TEXT_LEN + 1]; 
    char        es[MAX_TEXT_LEN + 1]; 
    char        ru[MAX_TEXT_LEN + 1]; 
} storage_t;

static const db_bind_t binds_def[] = {
    { ID_FNO, DB_VARTYPE_SINT32,   DB_BIND_OFFSET( storage_t, id ),  DB_BIND_SIZE( storage_t, id ), -1, DB_BIND_RELATIVE }, 
    { EN_FNO, DB_VARTYPE_UTF8STR,  DB_BIND_OFFSET( storage_t, en ),  DB_BIND_SIZE( storage_t, en ), -1, DB_BIND_RELATIVE }, 
    { JP_FNO, DB_VARTYPE_UTF8STR,  DB_BIND_OFFSET( storage_t, jp ),  DB_BIND_SIZE( storage_t, jp ), -1, DB_BIND_RELATIVE }, 
    { FR_FNO, DB_VARTYPE_UTF8STR,  DB_BIND_OFFSET( storage_t, fr ),  DB_BIND_SIZE( storage_t, fr ), -1, DB_BIND_RELATIVE }, 
    { ES_FNO, DB_VARTYPE_UTF8STR,  DB_BIND_OFFSET( storage_t, es ),  DB_BIND_SIZE( storage_t, es ), -1, DB_BIND_RELATIVE }, 
    { RU_FNO, DB_VARTYPE_UTF8STR,  DB_BIND_OFFSET( storage_t, ru ),  DB_BIND_SIZE( storage_t, ru ), -1, DB_BIND_RELATIVE }, 
};

/**
* Print an error message for a failed database operation.
*/
static void
print_error_message( const char * message, ... )
{
    va_list va;

    va_start( va, message );
    if ( get_db_error() == DB_NOERROR ) {
        if ( NULL != message ) {
            fprintf( stderr, "ERROR: " );
            vfprintf( stderr, message, va );
            fprintf( stderr, "\n" );
        }
    }
    else {
        dbs_error_info_t info = dbs_get_error_info( get_db_error() );

        fprintf( stderr, "ERROR %s: %s\n", info.name, info.description );

        if ( NULL != message ) {
            vfprintf( stderr, message, va );
            fprintf( stderr, "\n" );
        }

        /* The error has been handled by the application, so clear error state. */
        clear_db_error();
    }
    va_end( va );
}

db_t 
create_database(char* database_name, dbs_schema_def_t *schema)
{
    db_t hdb;

    /* Create a new file storage database with default parameters. */
    hdb = db_create_file_storage(database_name, NULL);

    if (hdb == NULL)
        return NULL;

    if (dbs_create_schema(hdb, schema) < 0) {
        db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);
        /* Remove incomplete database. */
        remove(database_name);
        return NULL;
    }
  
    return hdb;
}

static storage_t texts[] = {
    { 1, "Folio",       "フォリオ",         {0},                "Folio",            "Фолио"         },
    { 2, "General",     "一般",             {0},                "General",          "Основные"      },
    { 3, "Generic",     "汎用",             "Générique",        "Genérico",         "Общее"         },
    { 4, "Grayscale",   "グレースケール",   "Niveaux de gris",  "Escale de grises", "Оттенки серого"},
    { 5, "Light",       "薄い",             "Clair",            "Ligero",           "Светлый"       },
    { 6, "Medium",      "紙質" ,            "Moyen",            "Media",            "Средний"       },
    { 7, "No",          "いいえ",           "Non",              "No",               "Нет"           },
    { 8, "No Content",  "中身がありません", "Aucun contenu",    "No hay contenido", "Нет контента"  },
};

int
load_data( db_t hdb )
{
    db_result_t rc = DB_OK;

    db_row_t row;
    db_table_cursor_t p = {
        NULL,   //< No index
        DB_CAN_MODIFY | DB_LOCK_EXCLUSIVE
    };
    db_cursor_t c;

    c = db_open_table_cursor(hdb, STORAGE_TABLE, &p);

    row = db_alloc_row( binds_def, DB_ARRAY_DIM( binds_def ) );
    if( NULL == row ) {
        print_error_message( "Couldn't allocate row to pupulate 'storage' table\n" );
        return EXIT_FAILURE;
    }

    int i;
    for( i = 0; i < DB_ARRAY_DIM(texts) && DB_OK == rc; ++i )
        rc = db_insert(c, row, &texts[i], 0);        

    db_free_row( row );
    rc = DB_OK == rc ? db_commit_tx( hdb, 0 ) : rc;

    if( DB_OK != rc ) {
        print_error_message( "Couldn't pupulate 'storage' table\n" );
    }
    db_close_cursor(c);

    return DB_OK == rc ? EXIT_SUCCESS : EXIT_FAILURE;
}

int 
find_text( db_t hdb, const char * textid, const char * locale )
{
    int rc = EXIT_FAILURE;
    db_result_t db_rc = DB_OK;
    db_row_t row;
    db_cursor_t c;
    db_table_cursor_t p = {
        STORAGE_PKEY_INDEX_NAME,   //< No index
        DB_SCAN_FORWARD | DB_LOCK_EXCLUSIVE
    };
    char en[MAX_TEXT_LEN + 1] = {0};
    char tr[MAX_TEXT_LEN + 1] = {0};

    int target_fno = EN_FNO;
    if( 0 == strncmp( "jp", locale, 2 ) )
        target_fno = JP_FNO;
    else if( 0 == strncmp( "fr", locale, 2 ) )
        target_fno = FR_FNO;
    else if( 0 == strncmp( "es", locale, 2 ) )
        target_fno = ES_FNO;
    else if( 0 == strncmp( "ru", locale, 2 ) )
        target_fno = RU_FNO;

    c = db_open_table_cursor(hdb, STORAGE_TABLE, &p);

    row = db_alloc_row( NULL, 2 );
    if( NULL == row ) {
        print_error_message( "Couldn't pupulate 'storage' table\n" );
        return EXIT_FAILURE;
    }

    dbs_bind_addr( row, ID_FNO, DB_VARTYPE_ANSISTR, (char*)textid, strlen(textid), NULL );    
    if( DB_OK == db_seek( c, DB_SEEK_FIRST_EQUAL, row, 0, 1 ) )
    {
        db_unbind_field( row, ID_FNO );
        dbs_bind_addr( row, EN_FNO, DB_VARTYPE_UTF8STR, en, MAX_TEXT_LEN, NULL );
        if( target_fno != EN_FNO )
            dbs_bind_addr( row, target_fno, DB_VARTYPE_UTF8STR, tr, MAX_TEXT_LEN, NULL );
        if( DB_OK == db_fetch( c, row, 0 ) ) {
            fprintf( stdout, "Message. Id: %s, en: [%s], %s: [%s]\n",
                     textid, en, locale, target_fno == EN_FNO ? "" : tr
                );
            rc = EXIT_SUCCESS;
        }
    }
    if( rc != EXIT_SUCCESS ) 
        print_error_message( "Couldn't find translated text for messageid %s\n", textid );

    db_free_row( row );
    db_close_cursor( c );

    return rc;
}

int
example_main(int argc, char **argv) 
{
    if( 2 < argc && 0 == strncmp( "-h", argv[1], 2 ) ) {
        fprintf( stdout, 
                 "Usage:\n"
                 "    %s <messageid> <locale>\n"
                 "Parameters:\n"
                 "    messageid - Integer in range [1-%d]. '1' by default\n"
                 "    locale    - One of en, jp, fr, es, ru. 'ru' by default\n"
                 "",
                 argv[0], DB_ARRAY_DIM(texts)
            );
    }

    db_t hdb = create_database( EXAMPLE_DATABASE, &db_schema );

    int rc = EXIT_FAILURE;

    if( hdb != NULL ) {        
        rc = load_data( hdb );
        char * id = 0;
        if( 0 == rc )
            rc = find_text( hdb, argc > 1 ? argv[1] : "1", argc > 2 ? argv[2] : "ru" );
    }

    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

    return rc;
}

