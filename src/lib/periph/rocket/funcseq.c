#include "periph/rocket/funcseq.h"

void funcseq_next(FuncSeq *fs) {
  SequencableFunc sf = fs->func_array[fs->next_index];
  if (sf == NULL) {
    return;
  }
  fs->next_index += 1;
  (sf)(fs->param);
}

void init_funcseq(FuncSeq *fs, void *param, SequencableFunc *func_array) {
  fs->func_array = func_array;
  fs->param = param;
  fs->next_index = 0;
}
