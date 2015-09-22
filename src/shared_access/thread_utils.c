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

#include "thread_utils.h"

#include <stdlib.h>
#include <ittia/os/os_thread.h>
#include <ittia/os/os_mutex.h>

int
thread_spawn( thread_proc_t proc, void * arg, int flags, os_thread_t ** h )
{
    return os_thread_spawn( proc, arg, DEFAULT_STACK_SIZE, flags, h );
}

int
thread_join( os_thread_t * h )
{
    return os_thread_join( h );
}
/*
struct mutex_t {
    os_mutex_t * mutex;
};
*/
int
mutex_init(mutex_t * mutex)
{
    mutex->mutex = malloc( sizeof(os_mutex_t) );
    return os_mutex_init( (os_mutex_t *)mutex->mutex );
} 

int
mutex_destroy(mutex_t * mutex)
{
    int rc = os_mutex_destroy( (os_mutex_t *)mutex->mutex );
    free(mutex->mutex);
    return rc;
}

int
mutex_trylock(mutex_t * mutex)
{
    return os_mutex_trylock( (os_mutex_t *)mutex->mutex );
}

int
mutex_lock(mutex_t * mutex)
{
    return os_mutex_lock( (os_mutex_t *)mutex->mutex );
}

int
mutex_unlock(mutex_t * mutex)
{
    return os_mutex_unlock( (os_mutex_t *)mutex->mutex );
}
