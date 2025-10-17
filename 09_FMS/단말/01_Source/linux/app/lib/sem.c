#include "com_include.h"
#include <sys/sem.h>
#include "sem.h"

union semun 
{ 
	int val; 
	struct semid_ds *buf; 
	unsigned short int *array; 
}; 

/* open 과 close 를 위한 sembuf 구조체를 정의한다.  */
struct sembuf mysem_open  = { 0, -1, SEM_UNDO }; /* 세마포어 얻기 */
struct sembuf mysem_close = { 0, 1, SEM_UNDO };  /* 세마포어 돌려주기 */

/* semaphore 초기화 */
static int initial_sem( int semid )
{
	union semun sem_union;

	sem_union.val = 1;

	if( semctl( semid, 0, SETVAL, sem_union ) < 0 )
	{
		printf("\nerr init sem[ %s ]..\n", strerror( errno ) );
		return ERR_RETURN;
	}

	return ERR_SUCCESS;
}

/* semaphore 생성 */
int create_sem( int key   )
{
	int semid = 0;
	
	if ( key <= 0 )
	{
		printf("\nerr create semaphore..key is 0...  \n");
	}

	semid = semget( (key_t )key , 1 , 0660|IPC_CREAT);
	if ( semid < 0 )
	{
		printf("\nerr create[ %d : %s ] semaphore..\n", key, strerror( errno ));
		return ERR_RETURN;
	}	

	if ( initial_sem( semid ) != ERR_SUCCESS )
	{
		destroy_sem( semid , key );
		return ERR_RETURN;
	}

	//printf("\ncreate key[ %d ] semid [ %d ] semaphore..\n", key , semid  );

	return semid;
}

int lock_sem( int semid, int key  )
{
	if ( semid >= 0 && key > 0)
	{
		if( semop( semid, &mysem_open, 1) < 0 ) 
		{ 
			printf("\nsem lock err semid ( %d ).key( %d )..\n", semid , key ); 
		}
	}

	//printf("\nsem lock semid ( %d ).key( %d )..\n", semid , key ); 
	return ERR_SUCCESS;
}

int unlock_sem( int semid , int key )
{
	if ( semid >= 0 && key > 0 )
	{
		if( semop( semid, &mysem_close, 1) < 0 ) 
		{ 
			printf("\nsem unlock err semid ( %d ).key( %d )..\n ", semid, key ); 
		}
	}

	//printf("\nsem unlock semid ( %d ).key( %d )..\n", semid , key ); 
	return ERR_SUCCESS;

}

int destroy_sem( int semid , int key )
{
	union semun sem_union;

	if ( semid >= 0 && key > 0 )
	{
		if( semctl( semid, 0, IPC_RMID, sem_union) < 0 )
		{
			printf("\nerr delete semaphore..semid( %d )..key ( %d )\n", semid, key  );
		}
		else
		{
			//printf("\ndelete semaphore..semid( %d )..key( %d )\n", semid, key  );
		}
	}

	//printf("\nsem destroy semid ( %d ).key( %d )..\n", semid , key ); 
	return ERR_SUCCESS;
}

