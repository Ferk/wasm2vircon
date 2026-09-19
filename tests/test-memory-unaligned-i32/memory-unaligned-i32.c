/* Inspection-only: a packed field forces an unaligned Wasm i32 access. */
struct __attribute__((packed)) record
{
    unsigned char tag;
    unsigned int value;
};

static volatile struct record saved_record;

int main( void )
{
    saved_record.value = 0x11223344u;
    return saved_record.value;
}
