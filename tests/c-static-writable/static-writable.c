/* Inspection-only: initialized data is subsequently modified in linear memory. */
static volatile unsigned char bytes[] = { 0x11u, 0x22u, 0x33u, 0x44u };

int main( void )
{
    bytes[ 2 ] = 0xAAu;
    /* This targets lane 2 while all three other bytes share its target word.
     * Any non-preserving store must make this check fail at runtime. */
    return bytes[ 0 ] != 0x11u || bytes[ 1 ] != 0x22u ||
           bytes[ 2 ] != 0xAAu || bytes[ 3 ] != 0x44u;
}
