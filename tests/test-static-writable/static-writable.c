/* Inspection-only: initialized data is subsequently modified in linear memory. */
static volatile unsigned char bytes[] = { 0x11u, 0x22u, 0x33u, 0x44u };

int main( void )
{
    bytes[ 2 ] = 0xAAu;
    return bytes[ 0 ] + bytes[ 2 ];
}
