#ifndef DISPLAY_H
#define DISPLAY_H

#include "morse.h"

void displayInit();
void displaySplash();
void displaySelfTest();
void displayRender(const morse::Decoder& decoder);

#endif // DISPLAY_H
