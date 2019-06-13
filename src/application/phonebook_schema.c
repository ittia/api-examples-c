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
#include "phonebook_c.h"
#include "dbs_schema.h"

/* TABLE CONTACT */
static db_fielddef_t contact_fields[] =
{
    { CONTACT_ID,           "id",           DB_COLTYPE_UINT64,   0,                0, DB_NOT_NULL, 0 },
    { CONTACT_NAME,         "name",         DB_COLTYPE_WCHARSTR, MAX_CONTACT_NAME, 0, DB_NOT_NULL, 0 },
    { CONTACT_RING_ID,      "ring_id",      DB_COLTYPE_UINT64,   0,                0, DB_NULLABLE, 0 },
    { CONTACT_PICTURE_NAME, "picture_name", DB_COLTYPE_ANSISTR,  MAX_FILE_NAME,    0, DB_NULLABLE, 0 },
    { CONTACT_PICTURE,      "picture",      DB_COLTYPE_BLOB,     0,                0, DB_NULLABLE, 0 }
};

static db_indexfield_t contact_by_id_fields[] =
{
    { CONTACT_ID }
};

static db_indexfield_t contact_by_name_fields[] =
{
    { CONTACT_NAME }
};

static db_indexdef_t contact_indexes[] =
{
    { DB_ALLOC_INITIALIZER(),       /* db_alloc */
        DB_INDEXTYPE_DEFAULT,       /* index_type */
        CONTACT_BY_ID,              /* index_name */
        DB_PRIMARY_INDEX,           /* index_mode */
        DB_ARRAY_DIM(contact_by_id_fields),  /* nfields */
        contact_by_id_fields },     /* fields */

    { DB_ALLOC_INITIALIZER(),        /* db_alloc */
        DB_INDEXTYPE_DEFAULT,        /* index_type */
        CONTACT_BY_NAME,             /* index_name */
        DB_MULTISET_INDEX,           /* index_mode */
        DB_ARRAY_DIM(contact_by_name_fields), /* nfields */
        contact_by_name_fields },    /* fields */
};

/* TABLE PHONE_NUMBER */
static db_fielddef_t phone_number_fields[] =
{
    { PHONE_CONTACT_ID, "contact_id",   DB_COLTYPE_UINT64,  0,                0, DB_NOT_NULL, 0 },
    { PHONE_NUMBER,     "number",       DB_COLTYPE_ANSISTR, MAX_PHONE_NUMBER, 0, DB_NOT_NULL, 0 },
    { PHONE_TYPE,       "type",         DB_COLTYPE_UINT64,  0,                0, DB_NOT_NULL, 0 },
    { PHONE_SPEED_DIAL, "speed_dial",   DB_COLTYPE_SINT64,  0,                0, DB_NULLABLE, 0 }
};

static db_indexfield_t phone_number_by_contact_id_fields[] =
{
    { PHONE_CONTACT_ID }
};

static db_indexdef_t phone_number_indexes[] =
{

    { DB_ALLOC_INITIALIZER(),
        DB_INDEXTYPE_DEFAULT,                    /* index_type */
        PHONE_BY_CONTACT_ID,                     /* index_name */
        DB_MULTISET_INDEX,                       /* index_mode */
        DB_ARRAY_DIM(phone_number_by_contact_id_fields),  /* nfields */
        phone_number_by_contact_id_fields },     /* fields */
};

db_foreign_key_def_t phone_number_foreign_keys[] = {
    {
        "CONTACT_REF",
        /* Reference the contact table. */
        CONTACT_TABLE,
        /* Match type only applies to compound keys (on more than one column.) */
        DB_FK_MATCH_SIMPLE,
        /* Protect referenced rows from update or delete. */
        DB_FK_ACTION_RESTRICT, /* update_rule */
        DB_FK_ACTION_RESTRICT, /* delete_rule */
        /* Checks cannot be deferred until commit. */
        DB_FK_NOT_DEFERRABLE,
        /* Checks occur immediately by default. */
        DB_FK_CHECK_IMMEDIATE,
        /* Reference the ID field in the contact table. */
        1,
        {
            { PHONE_CONTACT_ID, CONTACT_ID },
        }
    }
};

/* Database schema. */
static db_tabledef_t phonebook_tables[] =
{
    { DB_ALLOC_INITIALIZER(),
        DB_TABLETYPE_DEFAULT,
        CONTACT_TABLE,
        DB_ARRAY_DIM(contact_fields),
                     contact_fields,
        DB_ARRAY_DIM(contact_indexes),
                     contact_indexes,
        0, NULL,
    },

    { DB_ALLOC_INITIALIZER(),
        DB_TABLETYPE_DEFAULT,
        PHONE_TABLE,
        DB_ARRAY_DIM(phone_number_fields),
                     phone_number_fields,
        DB_ARRAY_DIM(phone_number_indexes),
                     phone_number_indexes,
        DB_ARRAY_DIM(phone_number_foreign_keys),
                     phone_number_foreign_keys,
    },

};

db_seqdef_t phonebook_sequences[] =
{
    { CONTACT_ID_SEQUENCE, {{ 1, 0}} },

};

dbs_schema_def_t phonebook_schema =
{
    DB_ARRAY_DIM(phonebook_tables),
                 phonebook_tables,

    DB_ARRAY_DIM(phonebook_sequences),
                 phonebook_sequences
};


/* Database schema without BLOB fields. */
db_tabledef_t nopicture_phonebook_tables[] =
{
    { DB_ALLOC_INITIALIZER(),
        DB_TABLETYPE_DEFAULT,
        CONTACT_TABLE,
        DB_ARRAY_DIM(contact_fields) - 1,
                     contact_fields,
        DB_ARRAY_DIM(contact_indexes),
                     contact_indexes,
        0, NULL,
    },

    { DB_ALLOC_INITIALIZER(),
        DB_TABLETYPE_DEFAULT,
        PHONE_TABLE,
        DB_ARRAY_DIM(phone_number_fields),
                     phone_number_fields,
        DB_ARRAY_DIM(phone_number_indexes),
                     phone_number_indexes,
        DB_ARRAY_DIM(phone_number_foreign_keys),
                     phone_number_foreign_keys,
    },

};

dbs_schema_def_t nopicture_phonebook_schema =
{
    DB_ARRAY_DIM(nopicture_phonebook_tables),
                 nopicture_phonebook_tables,

    DB_ARRAY_DIM(phonebook_sequences),
                 phonebook_sequences
};

