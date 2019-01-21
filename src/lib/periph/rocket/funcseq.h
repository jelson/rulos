#pragma once

#include "periph/rocket/rocket.h"

typedef void (*SequencableFunc)(void *param);

typedef struct funcseq_t {
  SequencableFunc *func_array;
  void *param;
  int next_index;
} FuncSeq;

void init_funcseq(FuncSeq *fs, void *param, SequencableFunc *func_array);
void funcseq_next(FuncSeq *fs);
