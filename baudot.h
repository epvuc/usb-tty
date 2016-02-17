char baudot_to_ascii(char);
char ascii_to_baudot(char);
int tty_putchar(char, FILE *);
int tty_putchar_raw(char);


extern volatile int8_t flag_tx_rdy;
extern volatile int8_t flag_tx_busy;

#define TRUE -1
#define FALSE !TRUE

#define LTRS 0x1F   // Baudot Letters Shift
#define FIGS 0x1B   // Baudot Figures Shift


