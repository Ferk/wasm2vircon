#ifndef VIRCON_H
#define VIRCON_H

/* Declarations only: implementations are selected by the eventual runtime. */
void clear_screen( int color );
void print_at( int x, int y, const char* text );
void end_frame( void );

#endif
