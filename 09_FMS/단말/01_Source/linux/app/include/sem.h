#ifndef _SEM_H__
#define _SEM_H__

int create_sem( int key );
int lock_sem( int semid , int key );
int unlock_sem( int semid , int key );
int destroy_sem( int semid , int key );

#endif


