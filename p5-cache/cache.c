#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include "cache.h"
#include "cache_stats.h"
#include "print_helpers.h"

cache_t *make_cache(int capacity, int block_size, int assoc, enum protocol_t protocol)
{
  cache_t *cache = malloc(sizeof(cache_t));
  cache->stats = make_cache_stats();

  cache->capacity = capacity;     // in Bytes
  cache->block_size = block_size; // in Bytes
  cache->assoc = assoc;           // 1, 2, 3... etc.
  cache->protocol = protocol;

  // FIX THIS CODE!
  // first, correctly set these 5 variables. THEY ARE ALL WRONG
  // note: you may find math.h's log2 function useful
  cache->n_total_cache_line = capacity / block_size;
  cache->n_set = capacity / (assoc * block_size);
  cache->n_offset_bit = log2(block_size);
  cache->n_index_bit = log2(cache->n_set);
  cache->n_tag_bit = 32 - (cache->n_offset_bit) - (cache->n_index_bit);

  // next create the cache lines and the array of LRU bits
  // - malloc an array with n_rows
  // - for each element in the array, malloc another array with n_col
  // FIX THIS CODE!

  cache->lines =
      malloc((cache->n_set) * sizeof(cache_line_t *));
  for (int i = 0; i < (cache->n_set); i++)
  {
    (cache->lines)[i] = malloc(assoc * sizeof(cache_line_t));
  }

  cache->lru_way = malloc((cache->n_set) * sizeof(int));

  // initializes cache tags to 0, dirty bits to false,
  // state to INVALID, and LRU bits to 0
  // FIX THIS CODE!
  for (int i = 0; i < (cache->n_set); i++)
  {
    for (int j = 0; j < assoc; j++)
    {
      (cache->lines[i][j]).tag = 0;
      (cache->lines[i][j]).dirty_f = 0;
      (cache->lines[i][j]).state = INVALID;
    }
    cache->lru_way[i] = 0;
  }

  return cache;
}

unsigned long get_cache_tag(cache_t *cache, unsigned long addr)
{
  // FIX THIS CODE!
  return addr >> (32 - (cache->n_tag_bit));
}

unsigned long get_cache_index(cache_t *cache, unsigned long addr)
{
  // FIX THIS CODE!
  return (addr >> cache->n_offset_bit) & ~(~0 << cache->n_index_bit);
}

unsigned long get_cache_block_addr(cache_t *cache, unsigned long addr)
{
  // FIX THIS CODE!
  return (addr >> (cache->n_offset_bit)) << (cache->n_offset_bit);
}

void update_lru(cache_t *cache, int the_set, int touched_way)
{
  if (cache->assoc == 1)
  {
    return;
  }
  if (touched_way < (cache->assoc - 1))
  {
    cache->lru_way[the_set] = touched_way + 1;
  }
  else
  {
    cache->lru_way[the_set] = 0;
  }
}

/* this method takes a cache, an address, and an action
 * it proceses the cache access. functionality in no particular order: 
 *   - look up the address in the cache, determine if hit or miss
 *   - update the LRU_way, cacheTags, state, dirty flags if necessary
 *   - update the cache statistics (call update_stats)
 * return true if there was a hit, false if there was a miss
 * Use the "get" helper functions above. They make your life easier.
 */
bool access_cache(cache_t *cache, unsigned long addr, enum action_t action)
{
  // FIX THIS CODE!
  unsigned long tag = get_cache_tag(cache, addr);
  unsigned long index = get_cache_index(cache, addr);
  log_set(index);
  for (int a = 0; a < cache->assoc; a++) //Loop checking for tag matches
  {
    if (tag == cache->lines[index][a].tag)
    {
      log_way(a);
      if (action == LD_MISS || action == ST_MISS) //When there is a tag match but action is a snoop
      {
        if (cache->protocol == NONE)
        {
          update_stats(cache->stats, true, false, false, action);
        }
        else if (cache->protocol == VI)
        {
          update_stats(cache->stats, cache->lines[index][a].state == VALID, false, false, action);
          cache->lines[index][a].state = INVALID;
        }
      }
      else //When there is a tag match but action isn't a snoop
      {
        //update_stats(cache->stats, true, cache->lines[index][a].dirty_f, false, action);
        if (action == STORE)
        {
          if (cache->protocol == NONE)
          {
            cache->lines[index][a].dirty_f = 1;
            cache->lines[index][a].state = VALID;
          }
          else if (cache->protocol == VI)
          {
            cache->lines[index][a].dirty_f = 1;
            cache->lines[index][a].state = VALID;
          }
        }
        update_lru(cache, index, a);
	update_stats(cache->stats, true, false, false, action);
      }
      return true;
    }
  }
  log_way(cache->lru_way[index]);
  if (action == LD_MISS || action == ST_MISS) //Full cache is checked but no tag match, action is a snoop
  {
    if (cache->protocol == NONE)
    {
      update_stats(cache->stats, false, false, false, action);
    }
    else if (cache->protocol == VI)
    {
      update_stats(cache->stats, false, false, false, action);
      cache->lines[index][cache->lru_way[index]].state = INVALID;
    }
  }
  else //Full cache is checked but no tag match, action isn't a snoop
  {
    cache->lines[index][cache->lru_way[index]].tag = tag;
    update_stats(cache->stats, false, cache->lines[index][cache->lru_way[index]].dirty_f, false, action);
    if (action == STORE)
    {
      if (cache->protocol == NONE)
      {
        cache->lines[index][cache->lru_way[index]].dirty_f = 1;
        cache->lines[index][cache->lru_way[index]].state = VALID;
      }
      else if (cache->protocol == VI)
      {
        cache->lines[index][cache->lru_way[index]].dirty_f = 1;
        cache->lines[index][cache->lru_way[index]].state = VALID;
      }
    }
    else if (action == LOAD)
    {
      if (cache->protocol == NONE)
      {
        cache->lines[index][cache->lru_way[index]].dirty_f = 0;
      }
      else if (cache->protocol == VI)
      {
        cache->lines[index][cache->lru_way[index]].dirty_f = 0;
        cache->lines[index][cache->lru_way[index]].state = VALID;
      }
    }
    update_lru(cache, index, cache->lru_way[index]);
  }
  return false;
}
