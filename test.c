#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#define _WINUSER_
#define _WINGDI_
#define _IMM_
#define _WINCON_
#include <windows.h>

#define B2BP "%c%c%c%c%c%c%c%c"
#define B2B(byte)  \
  ((byte) & 0x80 ? '1' : '0'), \
  ((byte) & 0x40 ? '1' : '0'), \
  ((byte) & 0x20 ? '1' : '0'), \
  ((byte) & 0x10 ? '1' : '0'), \
  ((byte) & 0x08 ? '1' : '0'), \
  ((byte) & 0x04 ? '1' : '0'), \
  ((byte) & 0x02 ? '1' : '0'), \
  ((byte) & 0x01 ? '1' : '0') 


void print_dword(char *name, DWORD dw) {
    printf("%s: " B2BP" "B2BP" "B2BP" "B2BP"!\n", name, B2B(dw >> 24), B2B(dw >> 16), B2B(dw >> 8), B2B(dw));
}

void print_byte(char *name, byte b) {
    printf("%s: " B2BP"!\n", name,  B2B(b));
}
int main(void) {
    DWORD a = 0x89ABCDEF;
    byte b = 0xFF;
    print_dword("a    ", a);
    print_dword("b    ", b);
    print_dword("a & b", a & b);
    return 0;
}
