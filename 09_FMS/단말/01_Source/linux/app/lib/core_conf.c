#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

#include "util.h"
#include "core_conf.h"
#include "com_def.h"

#define MAXBUF_SIZ	256
#if 0
void sys2_sleep( int msec )
{
	struct timespec timeout0;
	struct timespec timeout1;
	struct timespec* tmp;
	struct timespec* t0 = &timeout0;
	struct timespec* t1 = &timeout1;

	t0->tv_sec = msec / 1000;
	t0->tv_nsec = (msec % 1000) * (1000 * 1000);

	while ((nanosleep(t0, t1) == (-1)) && (errno == EINTR))
	{
		tmp = t0;
		t0 = t1;
		t1 = tmp;
	}
}
#endif

conf_t *init_conf( const char *filename, const char *delim )
{
	conf_t *conf;
	conf = (conf_t *)calloc( 1, sizeof( conf_t ));
	if( !conf )
		return NULL;

	conf->filename = (char *)calloc( strlen( filename ) + 1, sizeof( char ));
	if( !conf->filename )
	{
		release_conf( conf );
		return NULL;
	}
	strcpy( conf->filename, filename );

	conf->delim = (char *)calloc( strlen( delim ) + 1, sizeof( char ));
	if( !conf->delim )
	{
		release_conf( conf );
		return NULL;
	}
	strcpy( conf->delim, delim );

	INIT_LIST_HEAD( &conf->elements );

	return conf;
}

conf_t *load_conf( const char *filename, const char *delim )
{
	FILE *fp = NULL;
	char *tok = NULL;
	char *ptr = NULL;
    char *session;
	static char name[MAXBUF_SIZ];
	static char key[MAXBUF_SIZ];
	static char value[MAXBUF_SIZ];
	char read_buf[MAXBUF_SIZ * 2 ];
	int read = 0;

	conf_t *conf;
	conf = init_conf( filename, delim );

	if( !conf )
	{
		//printf("load conf : can not init conf  [ %s ]..\n", filename  );
		return NULL;
	}

	fp = fopen( filename, "r" );
	if( !fp )
	{
		//printf("load conf : can not find file [ %s ]..\n", filename  );
		return conf;
	}

	memset( read_buf, 0, sizeof( read_buf ));
    session = NULL;

	while( fgets( read_buf, sizeof( read_buf ) - 1, fp ) )
	{
		read = 1;

		memset( key, 0, sizeof( key ) );
		memset( value, 0, sizeof( value ) );

		trim( read_buf );
		if( !strlen( read_buf ) || read_buf[0] == '#' )
		{
			//printf("load conf : file [ %s ], read len[ %d ], read_buf0 [ %x ]..\n", filename, strlen( read_buf ), read_buf[ 0 ]  );
			continue;
		}

		ptr =  read_buf;
		tok = strchr( ptr, '#' );
		if( tok )
			*tok = '\0';

		tok = (char *)strsep( &ptr, delim );
		strcpy( key, tok );
		if( ptr )
		{
			tok = (char *)strsep( &ptr, delim );
			strcpy( value, tok );
		}
        else
        {
            int len;

            len = strlen( key );
            if( key[ 0 ] == '[' && key[ len - 1 ] == ']' )
            {
                memset( name, 0, sizeof( name ) );
                strcpy( name, key + 1 );
                name[ len - 2 ] = '\x00';
                trim( name );
                session = name;
            }
        }

		trim( key );
		trim( value );

		if( strlen( key ) && strlen( value ) )
		{
			if( !conf_session_add_value( conf, session, key, value ) )
			{
			
				//printf("core conf error : sesssion[ %s ], key[ %s ], value[ %s ]..\n", session, key, value );
				release_conf( conf );
				fclose( fp );
				return NULL;
			}
		}

		memset( read_buf, 0, sizeof( read_buf ));
	}

	fclose( fp );
	
	if ( read == 0 )
	{
		//printf("core conf error : fail to read conf[ %s ].\n", filename );
	}

	return conf;
}


#define __USED_BUFFER__

#ifdef __USED_BUFFER__


static int save_session_conf_with_buffer( char *buffer, session_element_t *session, char *delim )
{
	char file_tmp[ 1024 ];
	element_t *e;

    if( session->key != NULL && session->key[ 0 ] != '\x00' )
    {
        memset( file_tmp, 0, sizeof( file_tmp ) );
        sprintf( file_tmp, "[%s]\n", session->key );
        strcat( buffer, file_tmp ); 
    }
	while( !UT_ListIsEmpty( &session->elem_list ) )
	{
		e = UT_ListGetObj( session->elem_list.Next, element_t, list );
		if( e->key && e->value )
		{
            memset( file_tmp, 0, sizeof( file_tmp ) );
            sprintf( file_tmp, "%s%s%s\n",  e->key, delim, e->value );
            strcat( buffer, file_tmp ); 
			free( e->key );
			free( e->value );
		}

		UT_ListDel( &e->list );

		free( e );
	}
    return 0;
}

static int save_buffer( FILE *fp, char *buffer )
{
    int len;
	int write_size =0;

    if( fp == NULL )
        return -1;

    len = strlen( buffer );
    if( len == 0 )
        return -1;

	write_size = fwrite( buffer, sizeof( char ), len , fp );

    //fprintf( fp, "%s", buffer );

	if ( len != write_size )
	{
		//printf("Fail to Save File : File Size[ %d ], Write Size[ %d ]..\n", len, write_size );
		return -1;
	}

    return 0;
}

#else

static int save_session_conf( FILE *fp, session_element_t *session, char *delim )
{
	element_t *e;

    if( session->key != NULL && session->key[ 0 ] != '\x00' )
    {
        fprintf( fp, "[%s]\n", session->key );
    }
	while( !UT_ListIsEmpty( &session->elem_list ) )
	{
		e = UT_ListGetObj( session->elem_list.Next, element_t, list );
		if( e->key && e->value )
		{
			fprintf( fp, "%s%s%s\n",  e->key, delim, e->value );
			free( e->key );
			free( e->value );
		}

		UT_ListDel( &e->list );

		free( e );
	}
    return 0;
}

#endif

int save_release_conf( conf_t *conf )
{
	char file_buf[ 10240 ];
	char szfile[ 256] ;

	session_element_t *e;
	FILE *fp = NULL;
	int ret = 0;

	if( !conf )
		return -1;

    memset( file_buf, 0, sizeof( file_buf ) );
	memset( szfile, 0, sizeof( szfile ));
	memcpy( szfile, conf->filename , strlen( conf->filename ));


	while( !UT_ListIsEmpty( &conf->elements ) )
	{
		e = UT_ListGetObj( conf->elements.Next, session_element_t, list );
#ifndef __USED_BUFFER__
        save_session_conf( fp, e, conf->delim );
#else
        save_session_conf_with_buffer( file_buf, e, conf->delim );
#endif
		UT_ListDel( &e->list );
        release_session_element( e );
	}

	if( conf->filename )
		free( conf->filename );

	if( conf->delim )
		free( conf->delim );

	free( conf );


	//printf("save_conf filename [ %s ], size : [ %d ] ...\n",szfile ,  strlen( file_buf ));

	fp = fopen( szfile, "w" );
	if( !fp )
	{
		return -1;
	}

	ret = save_buffer( fp, file_buf );

	fsync(fileno(fp));
	//fflush( fp );
	if( fclose( fp ) != 0 )
	{
		//printf("fclose error :filename [ %s ], size : [ %d ] ...\n",szfile ,  strlen( file_buf ));
	}

	/* 04-28*/
	 system_cmd( "sync" );
	return ret;
}

void release_conf( conf_t *conf )
{
	session_element_t *e;

	if( !conf )
		return;

	while( !UT_ListIsEmpty( &conf->elements ) )
	{
		e = UT_ListGetObj( conf->elements.Next, session_element_t, list );

		UT_ListDel( &e->list );
		release_session_element( e );
	}

	if( conf->filename )
		free( conf->filename );

	if( conf->delim )
		free( conf->delim );

	free( conf );
}

void release_session_element( session_element_t *e )
{
    element_t *elem;

	while( !UT_ListIsEmpty( &e->elem_list ) )
	{
		elem = UT_ListGetObj( e->elem_list.Next, element_t, list );

		UT_ListDel( &elem->list );
		release_element( elem );
	}
    if( e->key != NULL )
        free( e->key );
    free( e );
}

void release_element( element_t *e )
{
	if( !e )
		return;

	if( e->key )
		free( e->key );
	if( e->value )
		free( e->value );
	UT_ListDel( &e->list );
	free( e );
}

static void trace_session_elem( session_element_t *session, char *delim )
{
	tListHead *p;
    element_t *e;

    if( session->key != NULL && session->key[ 0 ] != '\x00' )
    {
        printf( "[%s]\n", session->key );
    }

	UT_ListForEach( p, &session->elem_list )
	{
		e = UT_ListGetObj( p, element_t, list );
		printf( "%s%s%s\n", e->key, delim, e->value );
	}
}

void trace_conf( conf_t *conf )
{
	tListHead *p;
	session_element_t *e;

	if( !conf )
		return;

	printf( "\n\"%s\"\n", conf->filename );

	UT_ListForEach( p, &conf->elements )
	{
		e = UT_ListGetObj( p, session_element_t, list );
        trace_session_elem( e, conf->delim );
	}
}

static session_element_t *internal_find_session( conf_t *conf, const char *name, int create )
{
	tListHead *p;
	session_element_t *e;

	if( !conf )
		return NULL;

	UT_ListForEach( p, &conf->elements )
	{
		e = UT_ListGetObj( p, session_element_t, list );
        if( name != NULL && name[ 0 ] != '\x00' )
        {
            if( strcmp( e->key, name ) == 0 )
                return e;
        }
        else
        {
            if( e->key == NULL )
                return e;
        }
	}
    if( create != 0 )
    {
        e = ( session_element_t * )calloc( 1, sizeof( session_element_t ) );
        if( !e )
            return NULL;

        e->key = NULL;
        if( name != NULL && name[ 0 ] != '\x00' )
        {
            e->key = (char *)calloc( strlen( name ) + 1, sizeof( char ) );
	        strcpy( e->key, name );
        }
	    INIT_LIST_HEAD( &e->list );
	    INIT_LIST_HEAD( &e->elem_list );
	    UT_ListAddTail( &e->list, &conf->elements );
        return e;
    }
    return NULL;
}

int find_session( conf_t *conf, const char *name )
{
    session_element_t * e;

    e = internal_find_session( conf, name, 0 );
    if( e != NULL )
        return 0;
    return -1;
}

static char *internal_conf_add_value( session_element_t *session, const char *key, const char *value )
{
	element_t *e;

	if( !session )
		return NULL;

	if( !key ||  !value || !strlen( key ) || !strlen( value ) )
		return NULL;

	e = (element_t *)calloc( 1, sizeof( element_t ) );
	if( !e )
		return NULL;

	e->key = (char *)calloc( strlen( key ) + 1, sizeof( char ) );
	if( !e->key )
	{
		release_element( e );
		return NULL;
	}
	strcpy( e->key, key );

	e->value = (char *)calloc( strlen( value ) + 1, sizeof( char ) );
	if( !e->value )
	{
		release_element( e );
		return NULL;
	}
	strcpy( e->value, value );

	UT_ListAddTail( &e->list, &session->elem_list );
	// UT_ListAddTail( &e->list, &conf->elements );

	return e->value;
}

char *conf_session_add_value( conf_t *conf, const char *name, const char *key, const char *value )
{
    session_element_t * e;

    e = internal_find_session( conf, name, 1 );
    if( e != NULL )
        return internal_conf_add_value( e, key, value );
    return NULL;
}

char *conf_add_value( conf_t *conf, const char *key, const char *value )
{
    return conf_session_add_value( conf, NULL, key, value );
}

static char *internal_conf_get_value( session_element_t *session, const char *key )
{
	tListHead *p;
	element_t *e;

	UT_ListForEach(p, &session->elem_list)
	{
		e = UT_ListGetObj( p, element_t, list );
		if( !strcasecmp( key, e->key ) )
			return e->value;
	}
	return NULL;
}

char *conf_session_get_value( conf_t *conf, const char *name, const char *key )
{
    session_element_t * e;

    e = internal_find_session( conf, name, 0 );
    if( e != NULL )
        return internal_conf_get_value( e, key );
    return NULL;
}

char *conf_get_value( conf_t *conf, const char *key )
{
    return conf_session_get_value( conf, NULL, key );
}

static char *internal_conf_set_value( session_element_t *session, const char *key, const char *value, int *err )
{
	tListHead *p;
	element_t *e;
	char *old_value;

    *err = 0;
	UT_ListForEach(p, &session->elem_list)
	{
		e = UT_ListGetObj( p, element_t, list );
		if( !strcasecmp( key, e->key ) )
		{
			if( strlen( e->value ) < strlen( value ) )
			{
				old_value = e->value;
				e->value = (char *)calloc( strlen( value ) + 1, sizeof( char ) );
				if( !e->value )
				{
                    *err = -1;
					e->value  = old_value;
					return NULL;
				}
				free( old_value );
			}
			strcpy( e->value, value );
			return e->value;
		}
	}
    *err = -2;
    return NULL;
}

char *conf_session_set_value( conf_t *conf, const char *name, const char *key, const char *value )
{
    int err;
    char *tmp;
    session_element_t * e;

    e = internal_find_session( conf, name, 0 );
    if( e != NULL )
    {
        tmp = internal_conf_set_value( e, key, value, &err );
        if( tmp != NULL || err == -1 )
            return tmp;
    }
	return conf_session_add_value( conf, name, key, value );
}

char *conf_set_value( conf_t *conf, const char *key, const char *value )
{
    return conf_session_set_value( conf, NULL, key, value );
}
