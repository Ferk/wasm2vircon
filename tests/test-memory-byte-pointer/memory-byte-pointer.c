/* Inspection-only: dynamic byte pointer arithmetic and byte load/store. */
extern int sample_index( void );

static volatile unsigned char values[ 8 ];

int main( void )
{
    unsigned int index = (unsigned int) sample_index() & 3u;
    values[ index ] = 0xA5u;
    return values[ index ];
}
