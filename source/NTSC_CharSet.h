#pragma once

typedef unsigned char(*csbits_t)[256][8];

extern unsigned char csbits_enhanced2e[2][256][8];  // Enhanced //e (2732 4K video ROM)
extern unsigned char csbits_a2[1][256][8];          // ][ and ][+

void make_csbits(void);
csbits_t Get2e_csbits(void);
