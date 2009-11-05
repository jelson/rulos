#ifndef _SIM_H
#define _SIM_H

struct s_board_layout;
typedef struct s_board_layout BoardLayout;

extern BoardLayout *tree0, *tree1, *wallclock_tree;
void sim_configure_tree(BoardLayout *tree);

#endif // _SIM_H
