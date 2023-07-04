#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void reset(int *count, int *flag)
{
    *count = 0;
    *flag = 0;
}

int main()
{
    int c;
    int count = 0;
    int p_flag = 0;
    FILE *file;
    char word[4] = {0};
    file = fopen("./hankaku.txt", "r");
    if (file) {
        while ((c = getc(file)) != EOF) {
            if (c == '\n') {  // end of a line
                reset(&count, &p_flag);
            } else {  // inner a line
                if (count >= 4) {
                    if (strcmp(word, "char") == 0) {
                        if (p_flag <= 5) {
                            putchar(c);
                            /* printf("%c", c); */
                            p_flag++;
                        } else {
                            printf("\n");
                            reset(&count, &p_flag);
                        }
                    } else {
                        reset(&count, &p_flag);
                    }
                } else {
                    word[count] = c;
                    count++;
                }
            }
        }
        fclose(file);
    }
}
