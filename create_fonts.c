#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void reset(int *index, int *flag)
{
    *index = 0;
    *flag = 0;
}

int binary_str_to_number(char *str, int length)
{
    int index = 0;
    int sum = 0;
    while (index < length) {
        int bit = str[index] == '0' ? 0 : 1;
        sum += bit * (pow(2, (length - 1) - index));
        index++;
    }
    return sum;
}

// We represent "." as 0 "*" as 1
// such as "..***..." is '00111000' as 38h or 56d
int graph_to_int(char *str)
{
    int length = 8;

    char out[8] = {0};
    int sum = 0;
    for(int i=0; i < length; i++){
        if (str[i] == '*') {
            out[i] = '1';
        } else if (str[i]== '.') {
            out[i] = '0';
        } else {
            break;
        }
    }

    sum = binary_str_to_number(out, 8);
    return sum;
}

int main()
{
    /* int b = binary_str_to_number("00111000", 8); */
    int b = graph_to_int("..***...\n");
    printf("%d", b);

    /* int c; */
    /* int font_index = 0; */
    /* int p_flag = 0; */
    /* FILE *file; */
    /* char word[4] = {0}; */
    /* file = fopen("./hankaku.txt", "r"); */
    /* if (file) { */
    /*     while ((c = getc(file)) != EOF) { */
    /*         if (c == '\n') {  // end of a line */
    /*             reset(&font_index, &p_flag); */
    /*         } else {  // inner a line */
    /*             if (font_index >= 4) { */
    /*                 if (strcmp(word, "char") == 0) { */
    /*                     if (p_flag < 5) {  // Get bits postion of special
     * font */
    /*                         putchar(c); */
    /*                         p_flag++; */
    /*                     } else { */
    /*                         printf("\n"); */
    /*                         reset(&font_index, &p_flag); */
    /*                     } */
    /*                 } else { */
    /*                     reset(&font_index, &p_flag); */
    /*                 } */
    /*             } else { */
    /*                 word[font_index] = c; */
    /*                 font_index++; */
    /*             } */
    /*         } */
    /*     } */
    /*     fclose(file); */
    /* } */
}
