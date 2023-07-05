#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONT_WIDTH 8

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

void reset(int *value)
{
    *value = 0;
}

void set_value(int *value)
{
    *value = 1;
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
    for (int i = 0; i < length; i++) {
        if (str[i] == '*') {
            out[i] = '1';
        } else if (str[i] == '.') {
            out[i] = '0';
        } else {
            break;
        }
    }

    sum = binary_str_to_number(out, 8);
    return sum;
}

void map_to_bin(char** map)
{
    FILE *file;
    file = fopen("./hankaku_font.img", "w");
    if (file) {
        fwrite(map, sizeof(char) * 16 * 256, 1, file);
        fclose(file);
    }
}

int main()
{
    int c;
    int font_index = 0;
    int id_index = 0;
    int line_index = 0;
    int is_pass_head = 1;
    int map_group_id = 0;
    int map_line_id = 0;  // Index for loading value to map

    FILE *file;
    char word[4] = {0};  // Catch "char"
    char id_buffer[5] = {0};
    char line_buffer[8] = {0};

    char map[256][16] = {0};
    file = fopen("./hankaku.txt", "r");
    if (file) {
        while ((c = getc(file)) != EOF) {
            if (c == '\r')
                continue;     // skip '\r'
            if (c == '\n') {  // end of a line
                // 1. Check out string in line buffer
                if (line_index > 0) {  // There is some thing in line buffer
                    int line_value = graph_to_int(line_buffer);
                    printf("%s %02x\n", line_buffer, line_value);
                    map[map_group_id][map_line_id] = line_value;
                    map_line_id++;
                    // TODO: should reset line buffer
                }
                // 2. Check out string in id_buffer
                if (id_index > 0) {
                    printf("\n");
                    map_group_id++;
                    reset(&map_line_id);
                }
                // 3. Store those buffers
                // 4. Reset local state

                set_value(&is_pass_head);
                reset(&font_index);
                reset(&id_index);
                reset(&line_index);
            } else {  // inner a line
                // Pass body
                if (c == '.' || c == '*') {
                    is_pass_head = 0;
                    line_buffer[line_index] = c;
                    line_index++;
                }
                // Pass header "char 0x00"
                if (is_pass_head) {
                    if (font_index == 4) {
                        if (strncmp(word, "char", 4) == 0) {
                            if (id_index <
                                5) {  // Get bits postion of special font
                                id_buffer[id_index] = c;
                                id_index++;
                            } else {
                                reset(&font_index);
                                reset(&id_index);
                            }
                        } else {
                            reset(&font_index);
                            reset(&id_index);
                        }
                    } else {
                        word[font_index] = c;
                        font_index++;
                    }
                }
            }
        }
        fclose(file);
    }
    map_to_bin(map);
}
