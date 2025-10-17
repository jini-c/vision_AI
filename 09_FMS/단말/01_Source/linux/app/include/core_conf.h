#ifndef _CORE_CONF_H_
#define _CORE_CONF_H_

#include <stdlib.h>

#include "UT_List.h"

#define SECTION		1
#define PAIR		2
#define ARRAY		3

#define CONF_TYPE		0x01
#define INI_TYPE		0x02
#define ARRAY_TYPE		0x04

typedef struct element_t
{
	tListHead list;
	char *key;
	char *value;
}__attribute__((packed)) element_t;

typedef struct session_element_t
{
	tListHead list;
    tListHead elem_list;
	char *key;
}__attribute__((packed)) session_element_t;

typedef struct conf_t
{
	char *filename;
	char *delim;
	tListHead elements;
}__attribute__((packed)) conf_t;

conf_t *init_conf( const char *filename, const char *delim );
conf_t *load_conf( const char *filename, const char *delim );
int save_release_conf( conf_t *conf );
int find_session( conf_t *conf, const char *name );

void release_conf( conf_t *conf );
void release_session_element( session_element_t *e );
void release_element( element_t *e );
int commit_conf( conf_t *conf );
void trace_conf( conf_t *conf );

char *conf_add_value( conf_t *conf, const char *key, const char *value );
char *conf_get_value( conf_t *conf, const char *key );
char *conf_set_value( conf_t *conf, const char *key, const char *value );

char *conf_session_add_value( conf_t *conf, const char *name, const char *key, const char *value );
char *conf_session_get_value( conf_t *conf, const char *name, const char *key );
char *conf_session_set_value( conf_t *conf, const char *name, const char *key, const char *value );

#if 0
conf_t *rtu_init_conf( const char *filename, const char *delim );
conf_t *rtu_load_conf( const char *filename, const char *delim );
int rtu_save_conf( conf_t *conf );
char *rtu_conf_get_value( conf_t *conf, const char *key );
char *rtu_conf_set_value( conf_t *conf, const char *key , const char *value );
#endif

#endif
