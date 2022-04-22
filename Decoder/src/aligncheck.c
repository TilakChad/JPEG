#include <stdio.h>
#include <stdint.h>

void align_check(uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i)
    {
        uint32_t z = (i+3) & ~(4-1);
        printf("[%u] -> %u\n",i,z);
    }
}

int main()
{
    align_check(50);
    return 0;
}
