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

bool upd_cache (cache_t *cache, unsigned long tag, unsigned long index, int touched_way, enum action_t action, 
  enum state_t new_state, bool hit_f, bool dirty_evict, bool upgr_miss_f)
{
  log_way(touched_way);
  if (!hit_f && action || LOAD) cache->lines[index][touched_way].tag = tag;
	update_stats(cache->stats, hit_f, false, upgr_miss_f, action);
  cache->lines[index][touched_way].state = new_state;
  if (action == STORE) {
    cache->lines[index][touched_way].dirty_f = 1;
    update_lru(cache, index, touched_way);
  }
  else if (action == LOAD){
    cache->lines[index][touched_way].dirty_f = 0;
    update_lru(cache, index, touched_way);
  }
  return hit_f;
}

bool vi_access (cache_t *cache, unsigned long addr, enum action_t action)
{
  unsigned long tag = get_cache_tag(cache, addr);
  unsigned long index = get_cache_index(cache, addr);
  log_set(index);
  for (int a = 0; a < cache->assoc; a++) {
    if (tag == cache->lines[index][a].tag) {
      if (cache->lines[index][a].state == INVALID){  //Tag match, state is INVALID
        if (action == ST_MISS || action == LD_MISS) { //STATE is INVALID, ACTION is ST_MISS or LD_MISS
            return upd_cache (cache, tag, index, cache->lru_way[index], action, INVALID, false, cache->lines[index][cache->lru_way[index]].dirty_f, false);
        }
        else { //STATE is INVALID, ACTION is LOAD or STORE
            return upd_cache (cache, tag, index, cache->lru_way[index], action, VALID, false, cache->lines[index][cache->lru_way[index]].dirty_f, false);
        }
      }
      else { //Tag match, state is VALID
        if (action == ST_MISS || action == LD_MISS) { //STATE is VALID, ACTION is ST_MISS
            return upd_cache (cache, tag, index, a, action, INVALID, true, cache->lines[index][cache->lru_way[index]].dirty_f, false);
        }
        else { //STATE is VALID, ACTION is LOAD or STORE
            return upd_cache (cache, tag, index, a, action, VALID, true, false, false);
      }
    }
  }
  }
  bool dirty_evict = cache->lines[index][cache->lru_way[index]].dirty_f;
  if (cache->lines[index][cache->lru_way[index]].state == INVALID){  //No tag match, state is INVALID
    if (action == ST_MISS || action == LD_MISS) {
        return upd_cache (cache, tag, index, cache->lru_way[index], action, INVALID, false, false, false);
    }
    else { //Action is LOAD or STORE
        return upd_cache (cache, tag, index, cache->lru_way[index], action, VALID, false, dirty_evict, false);
    }
  }
  else {  //No tag match, state is VALID
    if (action == ST_MISS || action == LD_MISS) {
        return upd_cache (cache, tag, index, cache->lru_way[index], action, VALID, false, false, false);
    }
    else { //ACTION is LOAD or STORE
        return upd_cache (cache, tag, index, cache->lru_way[index], action, VALID, false, dirty_evict, false);
    }
  }
}

bool msi_access (cache_t *cache, unsigned long addr, enum action_t action)
{
  unsigned long tag = get_cache_tag(cache, addr);
  unsigned long index = get_cache_index(cache, addr);
  log_set(index);
  for (int a = 0; a < cache->assoc; a++) {
    if (tag == cache->lines[index][a].tag) {
      if (cache->lines[index][a].state == INVALID){  //Tag match, state is INVALID
        if (action == ST_MISS || action == LD_MISS) { //STATE is INVALID, ACTION is ST_MISS or LD_MISS
          //return upd_cache (cache, tag, index, a, action, false, INVALID, false);
          return upd_cache (cache, tag, index, a, action, INVALID, false, false, false);
        }
        else if (action == STORE) { //STATE is INVALID, ACTION is STORE
          //return upd_cache (cache, tag, index, cache->lru_way[index], action, false, MODIFIED, false);
          return upd_cache (cache, tag, index, a, action, MODIFIED, false, false, false);
        }
        else if (action == LOAD) { //STATE is INVALID, ACTION is LOAD
          //return upd_cache (cache, tag, index, cache->lru_way[index], action, false, SHARED, false);
          return upd_cache (cache, tag, index, a, action, SHARED, false, false, false);
        }
      }
      else if (cache->lines[index][a].state == MODIFIED) {  //Tag match, state is MODIFIED
        if (action == ST_MISS) { //STATE is MODIFIED, ACTION is ST_MISS
          //return upd_cache (cache, tag, index, cache->lru_way[index], action, true, INVALID, false);
          return upd_cache (cache, tag, index, cache->lru_way[index], action, INVALID, true, true, false);
        }
        else if (action == LD_MISS) { //STATE is MODIFIED, ACTION is LD_MISS
          //return upd_cache (cache, tag, index, cache->lru_way[index], action, true, SHARED, false);
          return upd_cache (cache, tag, index, cache->lru_way[index], action, SHARED, true, true, false);
        }
        else if (action == STORE || action == LOAD) { //STATE is MODIFIED, ACTION is STORE or LOAD
          //return upd_cache (cache, tag, index, a, action, false, MODIFIED, false);
          return upd_cache (cache, tag, index, a, action, MODIFIED, false, false, false);
        }
      }
      else { //Tag match, state is SHARED
        if (action == ST_MISS) { //STATE is SHARED, ACTION is ST_MISS
          //return upd_cache (cache, tag, index, cache->lru_way[index], action, true, INVALID, true);
          return upd_cache (cache, tag, index, cache->lru_way[index], action, INVALID, true, false, true);
        }
        else if (action == STORE){ //STATE is SHARED, ACTION is STORE
          //return upd_cache (cache, tag, index, cache->lru_way[index], action, true, MODIFIED, true);
          return upd_cache (cache, tag, index, cache->lru_way[index], action, MODIFIED, true, false, true);
        }
        else if (action == LD_MISS || action == LOAD) { //STATE is SHARED, ACTION is LD_MISS or LOAD
          //return upd_cache (cache, tag, index, a, action, false, SHARED, false);
          return upd_cache (cache, tag, index, a, action, SHARED, false, false, false);
        }
      }
    }
  }
  log_way(cache->lru_way[index]);
  bool dirty_evict = cache->lines[index][cache->lru_way[index]].dirty_f;
  if (cache->lines[index][cache->lru_way[index]].state == INVALID){  //No tag match, state is INVALID
    if (action == ST_MISS || action == LD_MISS) {
      //return upd_cache (cache, tag, index, cache->lru_way[index], action, false, INVALID, false);
      return upd_cache (cache, tag, index, cache->lru_way[index], action, INVALID, false, dirty_evict, false);
    }
    else if (action == STORE) {
      //return upd_cache (cache, tag, index, cache->lru_way[index], action, false, MODIFIED, false);
      return upd_cache (cache, tag, index, cache->lru_way[index], action, MODIFIED, false, dirty_evict, false);
    }
    else if (action == LOAD) {
      //return upd_cache (cache, tag, index, cache->lru_way[index], action, false, SHARED, false);
      return upd_cache (cache, tag, index, cache->lru_way[index], action, SHARED, false, dirty_evict, false);
    }
  }
  else if (cache->lines[index][cache->lru_way[index]].state == MODIFIED) {  //No tag match, state is MODIFIED
    if (action == ST_MISS) {
      //return upd_cache (cache, tag, index, cache->lru_way[index], action, false, INVALID, false);
      return upd_cache (cache, tag, index, cache->lru_way[index], action, INVALID, false, dirty_evict, false);
    }
    else if (action == LD_MISS) {
      //return upd_cache (cache, tag, index, cache->lru_way[index], action, false, SHARED, false);
      return upd_cache (cache, tag, index, cache->lru_way[index], action, SHARED, false, dirty_evict, false);
    }
    else if (action == STORE || action == LOAD) {
      //return upd_cache (cache, tag, index, cache->lru_way[index], action, false, SHARED, false);
      return upd_cache (cache, tag, index, cache->lru_way[index], action, SHARED, false, dirty_evict, false);
    }
  }
  else { //No tag match, state is SHARED
    if (action == ST_MISS) {
      //return upd_cache (cache, tag, index, cache->lru_way[index], action, false, INVALID, true);
      return upd_cache (cache, tag, index, cache->lru_way[index], action, INVALID, false, dirty_evict, false);
    }
    else if (action == STORE){
      //return upd_cache (cache, tag, index, cache->lru_way[index], action, false, MODIFIED, true);
      return upd_cache (cache, tag, index, cache->lru_way[index], action, MODIFIED, false, dirty_evict, false);
    }
    else if (action == LD_MISS || action == LOAD) {
      //return upd_cache (cache, tag, index, cache->lru_way[index], action, false, SHARED, false);
      return upd_cache (cache, tag, index, cache->lru_way[index], action, SHARED, false, dirty_evict, false);
    }
  }
  return false;
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
  if (cache->protocol == MSI) {
    return msi_access (cache, addr, action);
  }
  else if (cache->protocol == VI) {
    return vi_access (cache, addr, action);
  }
  unsigned long tag = get_cache_tag(cache, addr);
  unsigned long index = get_cache_index(cache, addr);
  log_set(index);
  for (int a = 0; a < cache->assoc; a++) //Loop checking for tag matches
  {
    if (tag == cache->lines[index][a].tag)
    {
      return upd_cache (cache, tag, index, a, action, VALID, true, false, false);
    }
  }
  bool dirty_evict = cache->lines[index][cache->lru_way[index]].dirty_f;
  return upd_cache (cache, tag, index, cache->lru_way[index], action, VALID, false, dirty_evict, false);
}
