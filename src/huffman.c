#include <stdio.h>


int main (int argc, char **argv)
{
    int count[] = {0,0,1,5,2};
    int offset[5] = {0};

    for (int i = 1 ; i < 5; ++i)
    {
        offset[i] = offset[i-1] + count[i-1];
        offset[i] <<= 1;
    }
    for (int i = 1; i < 5; ++i)
        printf("Offset of %d : %d.\n",i,offset[i]);

    return 0;
}
