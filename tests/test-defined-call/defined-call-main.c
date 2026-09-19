/* Inspection-only: linked separately so the direct call cannot be inlined. */
extern int increment( int value );
extern int sample_value( void );

int main( void )
{
    return increment( sample_value() );
}
