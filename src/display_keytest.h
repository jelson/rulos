
typedef struct {
	ActivationFunc f;
	BoardBuffer bbuf;	
} KeyTestActivation_t;

void display_keytest_init(KeyTestActivation_t *kta, uint8_t board);
