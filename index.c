// g++ -std=gnu99 -Wall -O1 index.c -o index

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

int main() {
    FILE* collection = fopen("collection.tsv", "r");
    if (!collection) { perror("Failed to open collection.tsv"); exit(1); }

    char* line = NULL;
    size_t lineSize = 0;

    for (int i = 0; i < 5; i++) {
        int n = getline(&line, &lineSize, collection);
        print(line);
    }

    free(line);
    return 0;
}