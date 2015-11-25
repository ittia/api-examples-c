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

#ifndef DB_SCHEMA_H_INCLUDED
#define DB_SCHEMA_H_INCLUDED

#include <ittia/db.h>

/**
    Table "readings" contains readings of several temperature sensors.
    Assume that readings is arrived from some source in packets. Every packet contains several readings from several sensors.
    Packet number is monotonically increasing integer.

    Every sensor has it's unique id.
    Reading's values is 'temperature' and 'timepoint'. - If one of those values is wrong whole packet should be ignored.

    Primary key for readings table is (sensorid, timepoint).
 */
#define STORAGE_TABLE "readings"
#define PKEY_INDEX_NAME "pkey"

#define SENSORID_FNO    0
#define PACKETID_FNO    1
#define TIMEPOINT_FNO   2
#define TEMPERATURE_FNO 3


//------- Declarations to use while populating table

/* Populate table using string-type only relative bounds fields. So declare appropriate structure. See
   ittiadb/manuals/users-guide/api-c-database-access.html#relative-bound-fields
 */
typedef struct {
    int32_t   sensorid;
    int64_t   packetid;
    db_ansi_t   timepoint[20];
    db_ansi_t   temperature[10];

} storage_t;

static const db_bind_t binds_def[] = {
    {
        SENSORID_FNO, DB_VARTYPE_SINT32,  DB_BIND_OFFSET( storage_t, sensorid ),
        DB_BIND_SIZE( storage_t, sensorid ), -1, DB_BIND_RELATIVE
    },
    {
        PACKETID_FNO, DB_VARTYPE_SINT64,  DB_BIND_OFFSET( storage_t, packetid ),
        DB_BIND_SIZE( storage_t, packetid ), -1, DB_BIND_RELATIVE
    },
    {
        TIMEPOINT_FNO, DB_VARTYPE_ANSISTR,  DB_BIND_OFFSET( storage_t, timepoint ),
        DB_BIND_SIZE( storage_t, timepoint ), -1, DB_BIND_RELATIVE
    },
    {
        TEMPERATURE_FNO, DB_VARTYPE_ANSISTR,  DB_BIND_OFFSET( storage_t, temperature ),
        DB_BIND_SIZE( storage_t, temperature ), -1, DB_BIND_RELATIVE
    },
};

extern dbs_schema_def_t db_schema;

#endif
