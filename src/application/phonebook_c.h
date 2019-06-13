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
 * @file phonebook_c.h                                                     *
 *                                                                         *
 * Command-line example program demonstrating the ITTIA DB C API.          *
 *-------------------------------------------------------------------------*/

#ifndef PHONEBOOK_C_H
#define PHONEBOOK_C_H 1

#include <ittia/db.h>
#include <ittia/os/os_file.h>

#include <stdlib.h>
#include <wchar.h>

/*-------------------------------------------------------------------------*
 * Constants                                                               *
 *-------------------------------------------------------------------------*/

/* Open/create database files on default file system. */
#define DATABASE_FILE_MODE      OS_FILE_DEFFS
/* Use a local database file. */
#define DATABASE_NAME_LOCAL     "phone_book.ittiadb"
/* Use the IPC client protocol to access database through dbserver. */
#define DATABASE_NAME_SERVER    "idb+tcp://localhost/phone_book.ittiadb"

/* Use 128KiB of RAM for memory storage, when selected. */
#define MEMORY_STORAGE_SIZE     128 * 1024

#define DEFAULT_PICTURE         "unknown.png"
#define CONTACT_TABLE           "contact"
#define CONTACT_BY_ID           "by_id"
#define CONTACT_BY_NAME         "by_name"
#define PHONE_TABLE             "phone_number"
#define PHONE_BY_CONTACT_ID     "by_contact_id"
#define CONTACT_ID_SEQUENCE     "contact_id"

#define MAX_CONTACT_NAME        50   /* Unicode characters */
#define MAX_FILE_NAME           50   /* ANSI characters */
#define DATA_SIZE               1024 /* BLOB chunk size */
#define MAX_PHONE_NUMBER        20   /* phone number length */


/**
 * Types of telephone numbers
 */
typedef enum PhoneNumberType {
    HOME = 0,
        MOBILE,
        WORK,
        FAX,
        PAGER
} PhoneNumberType;

/*-------------------------------------------------------------------------*
 * Types of telephone numbers                                              *
 *-------------------------------------------------------------------------*/
typedef uint32_t contactid_t;

#ifndef min
#define min(x, y) ((x) < (y) ? (x) : (y))
#endif

typedef union {
    int64_t         i64;
    uint64_t        u64;
    char            s[MAX_FILE_NAME + 1];
    wchar_t         w[MAX_FILE_NAME + 1];
} types_t;

/*-------------------------------------------------------------------------*
 * Helper CONTACT table structure                                          *
 *-------------------------------------------------------------------------*/
typedef struct {
    contactid_t id;
    wchar_t     name[MAX_CONTACT_NAME + 1]; /* Extra char for null termination. */
    uint32_t    ring_id;
    db_ansi_t   picture_name[MAX_FILE_NAME + 1];
} contact_t;

/*-------------------------------------------------------------------------*
 * Helper PHONE_NUMBER table structure                                     *
 *-------------------------------------------------------------------------*/
typedef struct {
    contactid_t contact_id;
    db_ansi_t   number[MAX_PHONE_NUMBER + 1];
    uint32_t    type;
    int32_t     speed_dial;
} phone_t;

/*-------------------------------------------------------------------------*
 * CONTACT field IDs                                                       *
 *-------------------------------------------------------------------------*/
#define CONTACT_ID              0
#define CONTACT_NAME            1
#define CONTACT_RING_ID         2
#define CONTACT_PICTURE_NAME    3
#define CONTACT_PICTURE         4
#define CONTACT_NFIELDS         5

/*-------------------------------------------------------------------------*
 * PHONE_NUMBER field IDs                                                  *
 *-------------------------------------------------------------------------*/
#define PHONE_CONTACT_ID        0
#define PHONE_NUMBER            1
#define PHONE_TYPE              2
#define PHONE_SPEED_DIAL        3
#define PHONE_NFIELDS           4


void begin_tx(db_t hdb);
void commit_tx(db_t hdb);

void open_sequences(db_t hdb);
uint32_t seq_next( db_sequence_t hseq );
void populate_tables(db_t hdb);

db_t create_database(int storage_mode, char* database_name);
db_t open_database(int storage_mode, char* database_name);

contactid_t insert_contact(db_t hdb, contact_t contact);
void insert_phone_number(db_t hdb, phone_t phone);
void update_contact_name(db_t hdb, contactid_t id, const wchar_t * newname);

void list_contacts_brief(db_t hdb);
void list_contacts(db_t hdb, int sort);
void remove_contact(db_t hdb, contactid_t contactid);

void get_picture_name(db_t hdb, contactid_t contactid, db_ansi_t *file_name, db_len_t len);
void export_picture(db_t hdb, contactid_t contactid, db_ansi_t *file_name);

#endif
