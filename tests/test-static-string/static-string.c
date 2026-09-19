/* v1 static-data acceptance: byte layout includes the trailing NUL and is writable. */
static volatile unsigned char message[] = "Hello";

int main(void)
{
    message[0] = 'J';
    return message[0] + message[5];
}
