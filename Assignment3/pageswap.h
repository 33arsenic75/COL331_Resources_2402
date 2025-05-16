// pageswap.h
#ifndef PAGESWAP_H
#define PAGESWAP_H

void swapInit(void);
void swapOut(void);
void swapIn(char *memory);
void freeSwapSlot(int pid);

#endif