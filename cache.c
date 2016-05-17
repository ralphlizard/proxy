/* P locks (decrements) V unlocks (increments) */

#include "cache.h"

/* semaphore that protects connfd during write */
sem_t conn_noread;
/* semaphore that protects clientfd during write */
sem_t client_noread;
/* semaphore that locks new readers out from connfd */
sem_t conn_wait;
/* semaphore that locks new readers out rom clientfd */
sem_t client_wait;
/* number of readers on connfd */
int conn_num;
/* number of readers on clientfd */
int client_num;

cache *my_cache;

void init_cache()
{
  /* initialize semaphores */
  Sem_init(&conn_ex, 0, 1);
  Sem_init(&client_ex, 0, 1);
  Sem_init(&conn_wait, 0, 1);
  Sem_init(&client_wait, 0, 1);
  conn_num = 0;
  client_num = 0;

  /* initialize cache */
  my_cache->size = 0;
  my_cache->count = 0;
  my_cache->front = NULL;
  my_cache->rear = NULL;
}

int read_cache(char *request, int connfd)
{
  int value = -1;
  cache_object *cur_object = my_cache->front;

  if (cur_object == NULL)
    return -1;
  
  P(&mutex);
  reader_num++;
  if (reader_num == 1) /* cache cannot be written to while there is a reader */
    P(&nowrite);
  V(&mutex);
  
  /* look through cache */
  for (int i = 0; i < my_cache->count; i++)
  {
    /* found a hit */
    if (!strcmp(request, cur_object->request))
    {
      /* move to front */
      if (cur_object->prev NULL)
        cur_object->prev->next = cur_object->next;
      if (cur_object->next != NULL)
        cur_object->next->prev = cur_object->prev;
      cur_object->next = my_cache->front;
      cur_object->prev = NULL;
      my_cache->front->prev = cur_object;
      my_cache->front = cur_object
      
      /* send the cache content to client */
      if (rio_writen(connfd, cache_object->buf, cache_object->size)) != -1)
        value = 0;
      break;
    }
    cur_object = cur_object->next;
  }

  P(&mutex)
  reader_num--;
  if (reader_num == 0)
    V(&nowrite);
  V(&mutex);
  return value;
}

int write_cache_and_client (char *request, size_t size)
{
  int value = -1;
  cache_object *cur_object;
  
  P(&nowrite);
  P(&noread);
  if (size <= MAX_OBJECT_SIZE)
  {
    if (rio_readnb(clientrio, buf, size) != -1)
    {
      cur_object = Malloc(sizeof(cur_object));
      cur_object->buf = buf;
      strcpy(cur_object->request, request);
      cur_object->size = size;
      cur_object->next = my_cache->front;
      cur_object->prev = NULL;
      insert(cur_object);
      value = 0;
    }
  }
  V(&noread);
  V(&nowrite)
}

int insert(cache_object *cur_object)
{
  my_cache->front->prev = cur_object;
  my_cache->front = cur_object;
  my_cache->size += cur_object->size;
  
  /* evict LRU entries */
  while (my_cache->size > MAX_CACHE_SIZE)
  {
    my_cache->size -= my_cache->rear->size;
    my_cache->rear = my_cache->rear->prev;
    my_cache->rear->next = NULL;
  }
    
    
    
    
}