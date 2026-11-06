#ifndef POLYIR_COROUTINE_H
#define POLYIR_COROUTINE_H

// A coroutine: its body runs on a thread of its own, only while a resume waits for it. `coroutineResume` gives case 0 with the
// value the body gave, or case 1 when it has finished; `coroutineGive` hands a value over and hears whether it was closed.
typedef struct Coroutine Coroutine;
typedef void (*CoroutineBody)(Coroutine* coroutine, void* env);

struct Coroutine {
  CoroutineBody body;
  void* env;
  size_t size;
  pthread_mutex_t lock;
  pthread_cond_t turned;
  bool bodysTurn;
  bool started;
  bool finished;
  bool closing;
  void* handed;
};

static Coroutine* coroutineNew(CoroutineBody body, void* env, size_t size) {
  Coroutine* coroutine = gcAlloc(sizeof(Coroutine));
  coroutine->body = body;
  coroutine->env = env;
  coroutine->size = size;
  pthread_mutex_init(&coroutine->lock, NULL);
  pthread_cond_init(&coroutine->turned, NULL);

  return coroutine;
}

// Hands the turn to the other side and waits until it comes back.
static void coroutineTurn(Coroutine* coroutine, bool toBody) {
  coroutine->bodysTurn = toBody;
  pthread_cond_broadcast(&coroutine->turned);

  while (coroutine->bodysTurn == toBody && !coroutine->finished) {
    pthread_cond_wait(&coroutine->turned, &coroutine->lock);
  }
}

static void* coroutineThread(void* raw) {
  Coroutine* coroutine = raw;
  pthread_mutex_lock(&coroutine->lock);
  coroutine->body(coroutine, coroutine->env);
  coroutine->finished = true;
  pthread_cond_broadcast(&coroutine->turned);
  pthread_mutex_unlock(&coroutine->lock);

  return NULL;
}

static void coroutineStart(Coroutine* coroutine) {
  pthread_t thread;
  coroutine->started = true;
  coroutine->bodysTurn = true;
  pthread_create(&thread, NULL, coroutineThread, coroutine);
  pthread_detach(thread);

  while (coroutine->bodysTurn && !coroutine->finished) {
    pthread_cond_wait(&coroutine->turned, &coroutine->lock);
  }
}

static Variant coroutineResume(Coroutine* coroutine) {
  if (coroutine->finished) {
    return makeVariant(1, NULL, 0);
  }

  pthread_mutex_lock(&coroutine->lock);
  coroutine->started ? coroutineTurn(coroutine, true) : coroutineStart(coroutine);
  const Variant step = coroutine->finished ? makeVariant(1, NULL, 0) : makeVariant(0, coroutine->handed, coroutine->size);
  pthread_mutex_unlock(&coroutine->lock);

  return step;
}

static bool coroutineGive(Coroutine* coroutine, const void* value) {
  coroutine->handed = (void*) value;
  coroutineTurn(coroutine, false);

  return coroutine->closing;
}

static bool coroutineDone(Coroutine* coroutine) {
  return coroutine->finished;
}

static void coroutineClose(Coroutine* coroutine) {
  if (coroutine->started && !coroutine->finished) {
    pthread_mutex_lock(&coroutine->lock);
    coroutine->closing = true;
    coroutineTurn(coroutine, true);
    pthread_mutex_unlock(&coroutine->lock);
  }

  coroutine->finished = true;
}
#endif
