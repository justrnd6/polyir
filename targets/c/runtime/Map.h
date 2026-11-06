#ifndef POLYIR_MAP_H
#define POLYIR_MAP_H

// A map from keys to values, or a set (a map whose values have no size), by open addressing. Keys compare and hash by their
// bytes, unless the map is given its key type's own functions.
typedef struct Map {
  size_t keySize;
  size_t valueSize;
  HashFn hash;
  SameFn same;
  int64_t count;
  int64_t capacity;
  char* used;
  char* keys;
  char* values;
} Map;

static Map* mapNew(size_t keySize, size_t valueSize, HashFn hash, SameFn same) {
  Map* map = gcAlloc(sizeof(Map));
  map->keySize = keySize;
  map->valueSize = valueSize;
  map->hash = hash != NULL ? hash : hashBytes;
  map->same = same != NULL ? same : sameBytes;

  return map;
}

// The slot of `key`, or of the empty slot where it would go.
static int64_t mapSlot(Map* map, const void* key) {
  int64_t at = (int64_t) (map->hash(key, map->keySize) % (uint64_t) map->capacity);

  while (map->used[at] && !map->same(map->keys + at * map->keySize, key, map->keySize)) {
    at = (at + 1) % map->capacity;
  }

  return at;
}

static void mapPut(Map* map, const void* key, const void* value);

static void mapGrow(Map* map) {
  Map old = *map;
  map->capacity = old.capacity == 0 ? 16 : old.capacity * 2;
  map->count = 0;
  map->used = gcAlloc(map->capacity);
  map->keys = gcAlloc(map->capacity * map->keySize);
  map->values = gcAlloc(map->capacity * (map->valueSize > 0 ? map->valueSize : 1));

  for (int64_t i = 0; i < old.capacity; i++) {
    if (old.used[i]) {
      mapPut(map, old.keys + i * old.keySize, old.values + i * old.valueSize);
    }
  }
}

static void mapPut(Map* map, const void* key, const void* value) {
  if ((map->count + 1) * 2 > map->capacity) {
    mapGrow(map);
  }

  const int64_t at = mapSlot(map, key);

  if (!map->used[at]) {
    map->used[at] = 1;
    map->count++;
    memcpy(map->keys + at * map->keySize, key, map->keySize);
  }

  if (map->valueSize > 0 && value != NULL) {
    memcpy(map->values + at * map->valueSize, value, map->valueSize);
  }
}

static bool mapHas(Map* map, const void* key) {
  return map->capacity > 0 && map->used[mapSlot(map, key)];
}

// The value of `key`, or `zero` when the map does not hold it.
static void* mapGetOr(Map* map, const void* key, void* zero) {
  return mapHas(map, key) ? map->values + mapSlot(map, key) * map->valueSize : zero;
}

// Takes a key out (the other entries are put back, so none is lost) and says whether it was there.
static bool mapRemove(Map* map, const void* key) {
  if (!mapHas(map, key)) {
    return false;
  }

  Map old = *map;
  map->capacity = 0;
  map->count = 0;

  for (int64_t i = 0; i < old.capacity; i++) {
    if (old.used[i] && !old.same(old.keys + i * old.keySize, key, old.keySize)) {
      mapPut(map, old.keys + i * old.keySize, old.values + i * old.valueSize);
    }
  }

  return true;
}

static List* mapKeys(Map* map) {
  List* keys = listNew(map->keySize);

  for (int64_t i = 0; i < map->capacity; i++) {
    if (map->used[i]) {
      listPush(keys, map->keys + i * map->keySize);
    }
  }

  return keys;
}

static Map* mapCopy(Map* from) {
  Map* map = mapNew(from->keySize, from->valueSize, from->hash, from->same);

  for (int64_t i = 0; i < from->capacity; i++) {
    if (from->used[i]) {
      mapPut(map, from->keys + i * from->keySize, from->values + i * from->valueSize);
    }
  }

  return map;
}

static Map* mapWith(Map* from, const void* key, const void* value) {
  Map* map = mapCopy(from);
  mapPut(map, key, value);

  return map;
}

#define MAP_GET(map, K, V, key, zero) (*(V*) mapGetOr((map), (K[]){key}, (V[]){zero}))
#endif
