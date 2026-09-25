/* Keep an ordinary char array so its exact initial Wasm byte layout is tested. */
__attribute__((used)) static char message[] = "Hello";

int main(void)
{
    return 0;
}
