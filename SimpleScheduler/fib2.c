#include <stdio.h>
#include <stdlib.h>

unsigned long long fibonacci(long long int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;

    unsigned long long a = 0, b = 1, next;
    for (int i = 2; i <= n; i++) {
        next = a + b;
        a = b;
        b = next;
    }
    return b;
}

int main() {


    long long n = 400000005*5 + 40000005;
    if (n < 0) {
        printf("Please enter a non-negative integer.\n");
        return 1;
    }


    printf("%llu\n", fibonacci(n));
    return 0;
}

