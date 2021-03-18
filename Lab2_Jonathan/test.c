#include <stdio.h>

int main (int argc, char *argv[]){
    char string1[100];
    char string2[100];
    int num1, num2;
    char test_string [100] = "0 4 102";
    int rv = sscanf(test_string, "%u%u%s%[^\n]", &num1, &num2, string1, string2);
    printf("rv=%d, %d %d %s %s\n", rv, num1, num2, string1, string2);

    // int a, b, c=6;
    // printf("a:%d b:%d c:%d\n", a,b,c);
    // int rv = scanf("%s", string1);
    // printf("%d\n", rv);
    // if (rv==1){
    //     printf("%d\n", string1[0]);
    // }
}

