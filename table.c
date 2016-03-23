#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
  char ltrs[32] = { 0, 'E', 0x0A, 'A', ' ', 'S', 'I', 'U',
		    0x0D, 'D', 'R', 'J', 'N', 'F', 'C', 'K',
		    'T', 'Z', 'L', 'W', 'H', 'Y', 'P', 'Q',
		    'O', 'B', 'G', 0, 'M', 'X', 'V', 0 };
  
  char figs[32] = { 0, '3', 0x0A, '-', ' ', 7, '8', '7',
		    0x0D, '$', '4', 0x27, ',', '!', ':', '(',
		    '5', '"', ')', '2', '#', '6', '0', '1',
		    '9', '?', '&', 0, '.', '/', ';', 0 };
  
  int i;
  int a = 128; // start of first table, LTRS first.
  
  for (i=0; i<32; i++) {
    if (i%16 == 0) printf("\r\neewrite %04x ", a);
    printf("%02x ", ltrs[i]);
    a++;
  }
  for (i=0; i<32; i++)  {
    if (i%16 == 0) printf("\r\neewrite %04x ", a);
    printf("%02x ", figs[i]);
    a++;
  }
  printf("\r\n");
  
}
