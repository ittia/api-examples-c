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
 * @file phonebook_console.c                                               *
 *                                                                         *
 * Command-line example program demonstrating the ITTIA DB C API.          *
 *-------------------------------------------------------------------------*/

#include "phonebook_c.h"
#include "dbs_error_info.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


/*-------------------------------------------------------------------------*
 * Display and prompt for menu choices.                                    *
 *-------------------------------------------------------------------------*/
int menu(void)
{
    char buffer[50];
    int choice = -1;
    fputs( "\
------ Phone Book ------\n\
\n\
1) Add contact\n\
2) Remove contact\n\
3) Add phone number to existing contact\n\
4) Rename contact\n\
5) List contacts by name\n\
6) List contacts by id\n\
7) List contacts by ring id, name\n\
8) Export picture from existing contact\n\
9) Backup database file\n\
10) Watch contact name for changes\n\
0) Quit\n\
\n\
Enter the number of your choice: ", stdout );

    fflush(stdout);
    fscanf(stdin, "%d", &choice);

    /*---------------------------------------------------------------------*
     * Ignore remaining characters on the input line.                      *
     *---------------------------------------------------------------------*/
    fgets(buffer, 50, stdin);
    fputs("\n", stdout);

    return choice;
}

/*---------------------------------------------------------------------*
 * Open/Create the database.                                           *
 *---------------------------------------------------------------------*/
db_t open_or_create_database(char* database_name, int storage_mode)
{
    db_t hdb;

    hdb = open_database(storage_mode, database_name);

    if (hdb != NULL)
        return hdb;

    if (get_db_error() == DB_ENOENT) {
        printf("Database \"%s\" is not found, creating a new database.\n",
            database_name);

        /*-------------------------------------------------------------*
         * Create the database.                                        *
         *-------------------------------------------------------------*/
        hdb = create_database(storage_mode, database_name);
        if (hdb == NULL) {
            dbs_error_info_t info = dbs_get_error_info( get_db_error() );
            printf("Unable to create database \"%s\".\nError %s (%d): %s.\n",
                database_name, info.name, get_db_error(), info.description);
        }
    }
    else {
        dbs_error_info_t info = dbs_get_error_info( get_db_error() );
        printf("Unable to open existing database \"%s\".\nError %s (%d): %s.\n",
            database_name, info.name, get_db_error(), info.description);
    }

    return hdb;
}

/*-------------------------------------------------------------------------*
 * Prompt for a contact ID selection.                                      *
 *-------------------------------------------------------------------------*/
contactid_t select_contact(db_t hdb)
{
    contactid_t id;

    fputs("Id\tName\n", stdout);
    fputs("--\t----\n", stdout);

    begin_tx(hdb);
    list_contacts_brief(hdb);
    commit_tx(hdb);

    fputs("Enter id number: ", stdout);
    fflush(stdout);
    scanf("%d", &id);
    scanf ("%*[^\n]"); scanf ("%*c");

    return id;
}

/*-------------------------------------------------------------------------*
 * Prompt for a phone number.                                              *
 *-------------------------------------------------------------------------*/
void add_phone_number(db_t hdb, contactid_t contactid)
{
    char buffer[MAX_PHONE_NUMBER + 1];

    phone_t phone = {contactid, "", 0, -1};

    fputs("------ Add Phone Number ------\n", stdout);
    if (!contactid) {
        /*-----------------------------------------------------------------*
         * No contact id was given, so ask the user for one.               *
         *-----------------------------------------------------------------*/
        phone.contact_id = select_contact(hdb);
    }

    fputs("Phone number: ", stdout);
    fflush(stdout);
    fgets(buffer, MAX_PHONE_NUMBER, stdin);
    if (buffer != NULL) { buffer[strlen(buffer)-1] = '\0'; }
    strcpy(phone.number, buffer);

    fputs("Phone number type: \n\
0) Home\n\
1) Mobile\n\
2) Work\n\
3) Fax\n\
4) Pager\n\
Enter the number of your choice: ", stdout);
    fflush(stdout);
    scanf("%d", &phone.type);
    scanf("%*[^\n]");

    fputs("Speed dial number (-1=none): ", stdout);
    fflush(stdout);
    scanf("%d", &phone.speed_dial);
    scanf ("%*[^\n]"); scanf ("%*c");

    begin_tx(hdb);
    insert_phone_number(hdb, phone);
    commit_tx(hdb);
}

/*-------------------------------------------------------------------------*
 * Insert a contact into the database.                                     *
 *-------------------------------------------------------------------------*/
void add_contact(db_t hdb)
{
    db_ansi_t buffer[MAX_FILE_NAME + MAX_CONTACT_NAME + 1];

    contact_t contact = { 0, L"", 0, "" };

    fputs("------ Add Contact ------\n", stdout);
    fputs("Name: ", stdout);
    fflush(stdout);
    fgets(buffer, MAX_CONTACT_NAME, stdin);
    if (buffer != NULL) { buffer[strlen(buffer)-1] = '\0'; } /* remove the '\n' */

    mbstowcs(contact.name, buffer, MAX_CONTACT_NAME);

    fputs("Ring tone id number: ", stdout);
    fflush(stdout);
    scanf("%d", &contact.ring_id);
    scanf ("%*[^\n]"); scanf ("%*c");

    fputs("Picture file (\"" DEFAULT_PICTURE "\"): ", stdout);
    fflush(stdout);
    fgets(buffer, MAX_FILE_NAME, stdin);
    if(buffer[0] == '\n')  {
        strcpy(contact.picture_name, DEFAULT_PICTURE);
    } else {
        buffer[strlen(buffer)-1] = '\0'; /* remove the '\n' */
        strcpy(contact.picture_name, buffer);
    }

    begin_tx(hdb);
    contact.id = insert_contact(hdb, contact);
    commit_tx(hdb);

    add_phone_number(hdb, contact.id);
}

/*-------------------------------------------------------------------------*
 * Modify a contact record by prompting for and changing contact name.     *
 *-------------------------------------------------------------------------*/
void rename_contact(db_t hdb)
{
    db_ansi_t buffer[MAX_CONTACT_NAME + 1];
    wchar_t newname[MAX_CONTACT_NAME + 1];
    contactid_t id;

    fputs("------ Rename Contact ------\n", stdout);
    id = select_contact(hdb);

    fputs("New name: ", stdout);
    fflush(stdout);
    fgets(buffer, MAX_CONTACT_NAME, stdin);
    if (buffer != NULL) { buffer[strlen(buffer)-1] = '\0'; } /* remove the '\n' */

    mbstowcs(newname, buffer, MAX_CONTACT_NAME);

    begin_tx(hdb);
    update_contact_name(hdb, id, newname);
    commit_tx(hdb);
}

/*-------------------------------------------------------------------------*
 * Prompt for and delete a contact record.                                 *
 *-------------------------------------------------------------------------*/
void remove_contact_app(db_t hdb)
{
    contactid_t id;

    fputs("------ Remove Contact ------\n", stdout);
    id = select_contact(hdb);

    begin_tx(hdb);
    remove_contact(hdb, id);
    commit_tx(hdb);
}

/*-------------------------------------------------------------------------*
 * Prompt for and export a picture file from the blob field.               *
 *-------------------------------------------------------------------------*/
void export_picture_app(db_t hdb)
{
    contactid_t contactid;
    db_ansi_t buffer[MAX_FILE_NAME + 1];
    db_ansi_t file_name[MAX_FILE_NAME + 1];

    fputs("------ Export Picture ------\n", stdout);
    contactid = select_contact(hdb);

    begin_tx(hdb);
    get_picture_name(hdb, contactid, file_name, MAX_FILE_NAME + 1);
    commit_tx(hdb);
    printf("Choose a filename for picture (default=%s): ", file_name);
    fflush(stdout);
    fgets(buffer, MAX_FILE_NAME, stdin);
    if (buffer[0] != '\n') {
        buffer[strlen(buffer)-1] = '\0';
        strcpy(file_name, buffer);
    }

    begin_tx(hdb);
    export_picture(hdb, contactid, file_name);
    commit_tx(hdb);
}

/*-------------------------------------------------------------------------*
 * Prompt for a file name and save a backup copy of the database.          *
 *-------------------------------------------------------------------------*/
void backup_database(db_t hdb)
{
    db_ansi_t buffer[MAX_FILE_NAME + 1];
    db_ansi_t file_name[MAX_FILE_NAME + 1];

    fputs("------ Backup Database ------\n", stdout);

    printf("Choose a filename for the backup database: ");
    fflush(stdout);
    fgets(buffer, MAX_FILE_NAME, stdin);
    if (buffer[0] != '\n') {
        buffer[strlen(buffer)-1] = '\0';
        strcpy(file_name, buffer);
    }

    printf("Saving backup file. Other connections can continue working during backup.\n");
    if ( db_backup(hdb, file_name, 0, 0) == DB_OK) {
        printf("Backup complete.\n");
    }
    else {
        dbs_error_info_t info = dbs_get_error_info( get_db_error() );
        printf("Backup to file \"%s\" failed.\nError %s (%d): %s.\n",
            file_name, info.name, get_db_error(), info.description);
    }
}

/*-------------------------------------------------------------------------*
 * Watch the contact table for changes to the name field and deleted rows. *
 *-------------------------------------------------------------------------*/
void watch_contact_name(db_t hdb)
{
    enum {
        WATCH1
    };

    db_result_t rc;

    db_event_t evt;
    db_row_t wrow, waux;

    static const db_table_cursor_t contact_cursor_def =
    {
        CONTACT_BY_NAME, /* index */
        DB_SCAN_FORWARD | DB_LOCK_SHARED /* flags */
    };

    int wait_seconds = 0;

    /* Allocate empty rows, which db_wait_ex will reallocate with fields */
    /* appropriate to each event. */
    wrow = db_alloc_row(NULL, 1);
    waux = db_alloc_row(NULL, 1);

    /* Watch the contact table for inserts and updates. Include a copy of modified */
    /* fields in each event, including former values in case of an update. */
    db_watch_table(hdb, CONTACT_TABLE,
                   DB_WATCH_ROW_INSERT | DB_WATCH_ROW_UPDATE | DB_WATCH_ROW_DELETE
                   | DB_WATCH_PRIMARY_KEY | DB_WATCH_MODIFIED_FIELDS
                   | DB_WATCH_FORMER_VALUES,
                   WATCH1);

    printf("Watching contact table for inserts and updates.\n");
    printf("Rename a contact to create an event:\n");
    printf("\n");

    rename_contact(hdb);

    printf("\n");
    printf("Other connections can also create events by adding or renaming contacts.\n");
    printf("Use ITTIA DB Server to share a database between multiple connections.\n");
    printf("\n");

    do {
        rc = db_wait_ex(hdb, WAIT_MILLISEC(wait_seconds * 1000), &evt, wrow, waux);

        if (rc == DB_FAIL) {
            if (get_db_error() == DB_ELOCKED)
                printf("Event queue is empty.\n");
            else
                printf("Error waiting for event.\n");
        }
        else if (evt.event_tag == DB_WATCH_ROW_UPDATE) {
            if (evt.u.row_update.utid == WATCH1) {
                if (db_is_null(wrow, CONTACT_NAME) < 0) {
                    /* The CONTACT_NAME field was not recorded in the event. */
                    printf("Row updated, but name was not changed.\n");
                }
                else if (db_is_null(wrow, CONTACT_NAME) == 0) {
                    wchar_t name[MAX_CONTACT_NAME];
                    wchar_t orig_name[MAX_CONTACT_NAME];
                    db_ansi_t buffer[MAX_CONTACT_NAME + 1];

                    db_get_field_data(wrow, CONTACT_NAME, DB_VARTYPE_WCHARSTR, name, sizeof(name));
                    db_get_field_data(waux, CONTACT_NAME, DB_VARTYPE_WCHARSTR, orig_name, sizeof(orig_name));
                    printf("Name changed from \"%ls\" to \"%ls\" in update event.\n", orig_name, name);
                    printf("Enter a new name to change it again, or leave it blank to keep this name.\n");

                    fputs("Name: ", stdout);
                    fflush(stdout);
                    fgets(buffer, MAX_CONTACT_NAME, stdin);
                    if (buffer != NULL) { buffer[strlen(buffer)-1] = '\0'; } /* remove the '\n' */

                    if (buffer[0] != '\0') {
                        contactid_t id;

                        db_get_field_data(wrow, CONTACT_ID, DB_VARTYPE_UINT32, &id, sizeof(id));
                        mbstowcs(name, buffer, MAX_CONTACT_NAME);
                        begin_tx(hdb);
                        update_contact_name(hdb, id, name);
                        commit_tx(hdb);
                    }
                }
            }
        }
        else if (evt.event_tag == DB_WATCH_ROW_INSERT) {
            wchar_t name[MAX_CONTACT_NAME];

            /* The contact name column is not null, so it is always modified on insert. */
            db_get_field_data(wrow, CONTACT_NAME, DB_VARTYPE_WCHARSTR, name, sizeof(name));
            printf("A new contact, \"%ls\", was added.\n", name);
        }
        else if (evt.event_tag == DB_WATCH_ROW_DELETE) {
            contactid_t id;

            /* The name of the deleted contact is not available without DB_WATCH_ALL_FIELDS. */
            db_get_field_data(wrow, CONTACT_ID, DB_VARTYPE_UINT32, &id, sizeof(id));
            printf("A contact with id %d was deleted.\n", (int)id);
        }

        fputs("Wait how many seconds for next event (-1 to stop)? ", stdout);
        fflush(stdout);
        scanf("%d", &wait_seconds);
        scanf ("%*[^\n]"); scanf ("%*c");
    } while (wait_seconds >= 0);

    db_free_row(wrow);
    db_free_row(waux);

    db_unwatch_table(hdb, CONTACT_TABLE);
}

/*=========================================================================*
 *=========================================================================*
 * MAIN PROCESSING LOOP.                                                   *
 *=========================================================================*
 *=========================================================================*/
void run(db_t hdb)
{
    int choice = -1;
    for (choice = menu(); choice != 0; choice = menu()) {
        switch (choice) {
            case 1: /* Add contact. */
                add_contact(hdb);
                break;
            case 2: /* Remove contact. */
                remove_contact_app(hdb);
                break;
            case 3: /* Add phone number to existing contact */
                add_phone_number(hdb, 0);
                break;
            case 4: /* Rename contact */
                rename_contact(hdb);
                break;
            case 5: /* List contacts. */
                printf("------ Contacts ------\n");
                begin_tx(hdb);
                list_contacts(hdb, CONTACT_NAME);
                commit_tx(hdb);
                break;
            case 6: /* List contacts. */
                printf("------ Contacts ------\n");
                begin_tx(hdb);
                list_contacts(hdb, CONTACT_ID);
                commit_tx(hdb);
                break;
            case 7: /* List contacts. */
                printf("------ Contacts ------\n");
                begin_tx(hdb);
                list_contacts(hdb, CONTACT_RING_ID);
                commit_tx(hdb);
                break;
            case 8: /* Export picture from existing contact. */
                export_picture_app(hdb);
                break;
            case 9: /* Backup database. */
                backup_database(hdb);
                break;
            case 10: /* Watch contact name for changes. */
                watch_contact_name(hdb);
                break;
            default:
                printf("Unknown option: %d\n", choice);
        }
    }
}

/*-------------------------------------------------------------------------*
 * Display and prompt for connection method menu choices.                  *
 *-------------------------------------------------------------------------*/
int connection_menu(void)
{
    char buffer[50];
    int choice = -1;
    fputs( "\
------ Phone Book ------\n\
Choose a database connection method:\
\n\
1) Open file storage\n\
2) Open memory storage\n\
3) Connect to ITTIA DB Server on localhost, open file storage\n\
4) Connect to ITTIA DB Server on localhost, open memory storage\n\
0) Quit\n\
\n\
Enter the number of your choice: ", stdout );

    fflush(stdout);
    fscanf(stdin, "%d", &choice);

    /*---------------------------------------------------------------------*
     * Ignore remaining characters on the input line.                      *
     *---------------------------------------------------------------------*/
    fgets(buffer, 50, stdin);
    fputs("\n", stdout);

    return choice;
}

/*=========================================================================*
 *=========================================================================*
 * PROGRAM ENTRY POINT                                                     *
 *=========================================================================*
 *=========================================================================*/
int example_main(int argc, char* argv[])
{
    db_t    hdb = NULL;
    int     connection_method;
    char*   database_name;
    int     storage_mode;

    /* Prompt for a connection method. */
    do {
        connection_method = connection_menu();
        if (connection_method == 0)
            return EXIT_FAILURE;

        /* Check the library disposition before connecting to a server. */
        if ((connection_method == 3 || connection_method == 4) &&
            db_info(NULL, DB_INFO_DISPOSITION) == DB_DISPOSITION_STANDALONE)
        {
            connection_method = 0;
            printf("This is a stand-alone build of ITTIA DB SQL.\n");
            printf("Client/server is not supported; select another option.\n\n");
        }
    } while (connection_method < 1 || connection_method > 4);

    /* First choice is file vs. memory storage. */
    storage_mode = (connection_method - 1) & 0x1 ? DB_MEMORY_STORAGE : DB_FILE_STORAGE;
    /* Second choice is local vs. server storage. */
    database_name = (connection_method - 1) & 0x2 ? DATABASE_NAME_SERVER : DATABASE_NAME_LOCAL;

    hdb = open_or_create_database(database_name, storage_mode);

    /* Start server if a connection error occurs. */
    if (hdb == NULL && get_db_error() == DB_ESOCKETOPEN ) {
        printf("Cannot connect to server. Starting server in this process.\n");
        db_server_start(NULL);
        hdb = open_or_create_database(database_name, storage_mode);
    }

    if (hdb == NULL) {
        db_done_ex(NULL);
        return EXIT_FAILURE;
    }

    /*---------------------------------------------------------------------*
     * Run the application.                                                *
     *---------------------------------------------------------------------*/
    run(hdb);

    /*---------------------------------------------------------------------*
     * Close and shutdown database.                                        *
     *---------------------------------------------------------------------*/
    db_shutdown(hdb, DB_SOFT_SHUTDOWN, NULL);

    return EXIT_SUCCESS;
}
