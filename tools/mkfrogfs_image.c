#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if !defined(__BYTE_ORDER__) || !defined(__ORDER_LITTLE_ENDIAN__) || \
    __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "mkfrogfs_image currently requires a little-endian build host"
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

#define IMAGE_SIZE (80ULL * 1024ULL * 1024ULL)
#define SECTOR_SIZE 512U
#define PARTITION_START_LBA 2048U
#define PARTITION_SECTORS ((uint32_t) (IMAGE_SIZE / SECTOR_SIZE) - \
                           PARTITION_START_LBA)

#define FROGFS_MAGIC 0xF206U
#define FROGFS_ZONE_SIZE 1024U
#define FROGFS_SECTORS_PER_ZONE 2U
#define FROGFS_BITS_PER_ZONE (FROGFS_ZONE_SIZE * 8U)
#define FROGFS_INODES 4096U
#define FROGFS_DIRECT_ZONES 11U
#define FROGFS_INDIRECT_TABLES 4U
#define FROGFS_INDIRECT_ENTRIES (FROGFS_ZONE_SIZE / sizeof(uint32_t))
#define FROGFS_MAX_DATA_ZONES \
        (FROGFS_DIRECT_ZONES + \
         FROGFS_INDIRECT_TABLES * FROGFS_INDIRECT_ENTRIES)
#define FROGFS_MAX_FILE_SIZE (FROGFS_MAX_DATA_ZONES * FROGFS_ZONE_SIZE)
#define FROGFS_NAME_BYTES 16U
#define FROGFS_MAX_NAME (FROGFS_NAME_BYTES - 1U)
#define FROGFS_ZONE_SLOTS 15U
#define FROGFS_TYPE_DIRECTORY 3U
#define FROGFS_TYPE_REGULAR 5U
#define FROGFS_MODE_SHIFT 11U
#define MANIFEST_LINE_LIMIT 8192U
#define MANIFEST_VERSION 2U

#define ELF32_IDENT_SIZE 16U
#define ELF32_CLASS_32 1U
#define ELF32_DATA_LITTLE_ENDIAN 1U
#define ELF32_VERSION_CURRENT 1U
#define ELF32_TYPE_EXEC 2U
#define ELF32_MACHINE_386 3U
#define ELF32_PT_LOAD 1U
#define ELF32_PT_DYNAMIC 2U
#define ELF32_PT_INTERP 3U
#define ELF32_PT_TLS 7U
#define ELF32_PF_EXEC 1U
#define ELF32_PF_WRITE 2U
#define ELF32_PF_READ 4U
#define ELF32_PAGE_SIZE 4096U
#define ELF32_USER_START 0x08000000U
#define ELF32_USER_END 0x40000000U
#define ELF32_MAX_SEGMENTS 32U
#define ELF32_MAX_PAGES 4096U
#define ELF32_MAX_FILE_OFFSET 0x7fffffffU

struct frogfs_super_disk {
        uint32_t magic;
        char volume_name[16];
        uint32_t inode_count;
        uint32_t inode_size;
        uint32_t zone_count;
        uint32_t zone_size;
        uint32_t inode_bitmap_block;
        uint32_t inode_bitmap_blocks;
        uint32_t zone_bitmap_block;
        uint32_t zone_bitmap_blocks;
        uint32_t inode_table_block;
        uint32_t inode_table_blocks;
        uint32_t data_start_block;
        uint32_t root_inode;
        uint32_t dir_entry_size;
        uint32_t log_zone_size;
        uint32_t max_file_size;
        uint32_t modified_time;
        uint8_t read_only;
        uint8_t padding[427];
} __attribute__((packed));

struct frogfs_inode_disk {
        uint32_t inode_number;
        uint16_t mode;
        uint16_t mode_padding;
        uint32_t size;
        uint8_t link_count;
        uint8_t link_padding[3];
        uint32_t zones[FROGFS_ZONE_SLOTS];
        uint32_t blocks;
        uint32_t access_time;
        uint32_t change_time;
        uint32_t modified_time;
        uint16_t user_id;
        uint8_t group_id;
        uint8_t device_padding;
        uint16_t device;
        uint8_t tail_padding[2];
};

struct frogfs_dir_entry_disk {
        char name[FROGFS_NAME_BYTES];
        uint32_t inode_number;
        uint32_t file_type;
};

struct mbr_partition {
        uint8_t bootable;
        uint8_t start_head;
        uint8_t start_sector;
        uint8_t start_cylinder;
        uint8_t type;
        uint8_t end_head;
        uint8_t end_sector;
        uint8_t end_cylinder;
        uint32_t start_lba;
        uint32_t sector_count;
} __attribute__((packed));

struct mbr_disk {
        uint8_t boot_code[440];
        uint32_t disk_signature;
        uint16_t reserved;
        struct mbr_partition partitions[4];
        uint16_t signature;
} __attribute__((packed));

struct elf32_header {
        uint8_t ident[ELF32_IDENT_SIZE];
        uint16_t type;
        uint16_t machine;
        uint32_t version;
        uint32_t entry;
        uint32_t program_offset;
        uint32_t section_offset;
        uint32_t flags;
        uint16_t header_size;
        uint16_t program_entry_size;
        uint16_t program_count;
        uint16_t section_entry_size;
        uint16_t section_count;
        uint16_t section_names;
};

struct elf32_program_header {
        uint32_t type;
        uint32_t offset;
        uint32_t virtual_address;
        uint32_t physical_address;
        uint32_t file_size;
        uint32_t memory_size;
        uint32_t flags;
        uint32_t alignment;
};

struct elf32_load_segment {
        uint32_t virtual_address;
        uint32_t memory_size;
        uint32_t map_start;
        uint32_t map_end;
};

_Static_assert(sizeof(struct frogfs_super_disk) == SECTOR_SIZE,
               "FrogFS superblock layout drifted");
_Static_assert(offsetof(struct frogfs_super_disk, data_start_block) == 60,
               "FrogFS superblock field offsets drifted");
_Static_assert(sizeof(struct frogfs_inode_disk) == 100,
               "FrogFS inode layout drifted");
_Static_assert(offsetof(struct frogfs_inode_disk, size) == 8,
               "FrogFS inode size offset drifted");
_Static_assert(offsetof(struct frogfs_inode_disk, zones) == 16,
               "FrogFS inode zone offset drifted");
_Static_assert(offsetof(struct frogfs_inode_disk, device) == 96,
               "FrogFS inode device offset drifted");
_Static_assert(sizeof(struct frogfs_dir_entry_disk) == 24,
               "FrogFS directory entry layout drifted");
_Static_assert(offsetof(struct frogfs_dir_entry_disk, inode_number) == 16 &&
               offsetof(struct frogfs_dir_entry_disk, file_type) == 20,
               "FrogFS directory entry offsets drifted");
_Static_assert(sizeof(struct mbr_disk) == SECTOR_SIZE,
               "MBR layout drifted");
_Static_assert(sizeof(struct elf32_header) == 52,
               "ELF32 header layout drifted");
_Static_assert(sizeof(struct elf32_program_header) == 32,
               "ELF32 program header layout drifted");

struct sha256_state {
        uint32_t words[8];
        uint64_t bit_count;
        uint8_t block[64];
        size_t used;
};

struct manifest_entry {
        char *target;
        char *source;
        uint8_t expected_hash[32];
        uint64_t size;
        bool executable;
        bool directory;
};

struct fs_node {
        char name[FROGFS_NAME_BYTES];
        uint32_t parent;
        uint32_t inode_number;
        uint32_t type;
        const struct manifest_entry *manifest;
        struct frogfs_inode_disk inode;
        uint32_t *indirect[FROGFS_INDIRECT_TABLES];
};

struct image_builder {
        int fd;
        struct frogfs_super_disk super;
        struct fs_node *nodes;
        size_t node_count;
        size_t node_capacity;
        uint8_t *inode_bitmap;
        uint8_t *zone_bitmap;
        uint32_t next_zone;
};

static const uint32_t sha256_round_constants[64] = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

static int errorf(const char *format, ...)
{
        va_list arguments;

        fprintf(stderr, "mkfrogfs_image: ");
        va_start(arguments, format);
        vfprintf(stderr, format, arguments);
        va_end(arguments);
        fputc('\n', stderr);
        return -1;
}

static void warningf(const char *format, ...)
{
        va_list arguments;

        fprintf(stderr, "mkfrogfs_image: warning: ");
        va_start(arguments, format);
        vfprintf(stderr, format, arguments);
        va_end(arguments);
        fputc('\n', stderr);
}

static uint32_t rotate_right(uint32_t value, uint32_t count)
{
        return (value >> count) | (value << (32U - count));
}

static void sha256_transform(struct sha256_state *state,
                             const uint8_t block[64])
{
        uint32_t schedule[64];
        uint32_t a;
        uint32_t b;
        uint32_t c;
        uint32_t d;
        uint32_t e;
        uint32_t f;
        uint32_t g;
        uint32_t h;

        for (size_t index = 0; index < 16; index++) {
                size_t offset = index * 4;
                schedule[index] = (uint32_t) block[offset] << 24 |
                                  (uint32_t) block[offset + 1] << 16 |
                                  (uint32_t) block[offset + 2] << 8 |
                                  block[offset + 3];
        }
        for (size_t index = 16; index < 64; index++) {
                uint32_t first = schedule[index - 15];
                uint32_t second = schedule[index - 2];
                uint32_t sigma0 = rotate_right(first, 7) ^
                                  rotate_right(first, 18) ^ (first >> 3);
                uint32_t sigma1 = rotate_right(second, 17) ^
                                  rotate_right(second, 19) ^ (second >> 10);
                schedule[index] = schedule[index - 16] + sigma0 +
                                  schedule[index - 7] + sigma1;
        }

        a = state->words[0];
        b = state->words[1];
        c = state->words[2];
        d = state->words[3];
        e = state->words[4];
        f = state->words[5];
        g = state->words[6];
        h = state->words[7];
        for (size_t index = 0; index < 64; index++) {
                uint32_t choice = (e & f) ^ (~e & g);
                uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
                uint32_t sum0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^
                                rotate_right(a, 22);
                uint32_t sum1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^
                                rotate_right(e, 25);
                uint32_t temporary1 = h + sum1 + choice +
                                      sha256_round_constants[index] +
                                      schedule[index];
                uint32_t temporary2 = sum0 + majority;

                h = g;
                g = f;
                f = e;
                e = d + temporary1;
                d = c;
                c = b;
                b = a;
                a = temporary1 + temporary2;
        }
        state->words[0] += a;
        state->words[1] += b;
        state->words[2] += c;
        state->words[3] += d;
        state->words[4] += e;
        state->words[5] += f;
        state->words[6] += g;
        state->words[7] += h;
}

static void sha256_init(struct sha256_state *state)
{
        static const uint32_t initial[8] = {
            0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
            0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
        };

        memset(state, 0, sizeof(*state));
        memcpy(state->words, initial, sizeof(initial));
}

static void sha256_update(struct sha256_state *state,
                          const void *input, size_t length)
{
        const uint8_t *bytes = input;

        state->bit_count += (uint64_t) length * 8U;
        while (length != 0) {
                size_t available = sizeof(state->block) - state->used;
                size_t take = length < available ? length : available;

                memcpy(state->block + state->used, bytes, take);
                state->used += take;
                bytes += take;
                length -= take;
                if (state->used == sizeof(state->block)) {
                        sha256_transform(state, state->block);
                        state->used = 0;
                }
        }
}

static void sha256_finish(struct sha256_state *state, uint8_t result[32])
{
        uint64_t bit_count = state->bit_count;
        uint8_t one = 0x80;
        uint8_t zero = 0;
        uint8_t length[8];

        sha256_update(state, &one, 1);
        while (state->used != 56)
                sha256_update(state, &zero, 1);
        for (size_t index = 0; index < sizeof(length); index++)
                length[sizeof(length) - index - 1] =
                    (uint8_t) (bit_count >> (index * 8));
        sha256_update(state, length, sizeof(length));
        for (size_t index = 0; index < 8; index++) {
                result[index * 4] = (uint8_t) (state->words[index] >> 24);
                result[index * 4 + 1] =
                    (uint8_t) (state->words[index] >> 16);
                result[index * 4 + 2] =
                    (uint8_t) (state->words[index] >> 8);
                result[index * 4 + 3] = (uint8_t) state->words[index];
        }
}

static int hex_value(char value)
{
        if (value >= '0' && value <= '9')
                return value - '0';
        if (value >= 'a' && value <= 'f')
                return value - 'a' + 10;
        if (value >= 'A' && value <= 'F')
                return value - 'A' + 10;
        return -1;
}

static int parse_hash(const char *text, uint8_t result[32])
{
        if (strlen(text) != 64)
                return -1;
        for (size_t index = 0; index < 32; index++) {
                int high = hex_value(text[index * 2]);
                int low = hex_value(text[index * 2 + 1]);

                if (high < 0 || low < 0)
                        return -1;
                result[index] = (uint8_t) (high << 4 | low);
        }
        return 0;
}

static int parse_size(const char *text, uint64_t *result)
{
        char *end = NULL;

        if (!text || !text[0] || text[0] == '-')
                return -1;
        errno = 0;
        uintmax_t value = strtoumax(text, &end, 10);
        if (errno || !end || *end != '\0' || value > FROGFS_MAX_FILE_SIZE)
                return -1;
        *result = (uint64_t) value;
        return 0;
}

static int read_retry(int fd, void *buffer, size_t length)
{
        for (;;) {
                ssize_t result = read(fd, buffer, length);

                if (result < 0 && errno == EINTR)
                        continue;
                return (int) result;
        }
}

static int write_at(int fd, const void *buffer, size_t length, off_t offset)
{
        const uint8_t *next = buffer;

        if (offset < 0 || (uint64_t) offset + length > IMAGE_SIZE)
                return errorf("write range is outside the image");
        while (length != 0) {
                ssize_t result = pwrite(fd, next, length, offset);

                if (result < 0 && errno == EINTR)
                        continue;
                if (result <= 0)
                        return errorf("pwrite failed: %s", strerror(errno));
                next += result;
                length -= (size_t) result;
                offset += result;
        }
        return 0;
}

static int read_at(int fd, void *buffer, size_t length, off_t offset)
{
        uint8_t *next = buffer;

        if (offset < 0 || (uint64_t) offset + length > IMAGE_SIZE)
                return errorf("read range is outside the image");
        while (length != 0) {
                ssize_t result = pread(fd, next, length, offset);

                if (result < 0 && errno == EINTR)
                        continue;
                if (result <= 0)
                        return errorf("pread failed: %s", strerror(errno));
                next += result;
                length -= (size_t) result;
                offset += result;
        }
        return 0;
}

static int hash_open_file(int fd, uint8_t result[32])
{
        struct sha256_state state;
        uint8_t buffer[64 * 1024];

        if (lseek(fd, 0, SEEK_SET) < 0)
                return errorf("cannot seek source: %s", strerror(errno));
        sha256_init(&state);
        for (;;) {
                int count = read_retry(fd, buffer, sizeof(buffer));

                if (count < 0)
                        return errorf("cannot read source: %s", strerror(errno));
                if (count == 0)
                        break;
                sha256_update(&state, buffer, (size_t) count);
        }
        sha256_finish(&state, result);
        return 0;
}

static char *join_source_path(const char *manifest, const char *source)
{
        const char *slash;
        size_t directory_length;
        char *joined;

        slash = strrchr(manifest, '/');
        directory_length = slash ? (size_t) (slash - manifest) : 1U;
        if (strlen(source) > SIZE_MAX - directory_length - 2U)
                return NULL;
        joined = malloc(directory_length + strlen(source) + 2U);
        if (!joined)
                return NULL;
        if (slash)
                memcpy(joined, manifest, directory_length);
        else
                joined[0] = '.';
        joined[directory_length] = '/';
        strcpy(joined + directory_length + 1U, source);
        return joined;
}

static int read_source_at(int fd, void *buffer, uint32_t length,
                          uint32_t offset)
{
        uint8_t *next = buffer;
        uint32_t completed = 0;

        if (offset > ELF32_MAX_FILE_OFFSET ||
            length > ELF32_MAX_FILE_OFFSET - offset)
                return -1;
        while (completed < length) {
                ssize_t count = pread(fd, next + completed,
                                      length - completed,
                                      (off_t) offset + completed);

                if (count < 0 && errno == EINTR)
                        continue;
                if (count <= 0 || (uint32_t) count > length - completed)
                        return -1;
                completed += (uint32_t) count;
        }
        return 0;
}

static bool is_power_of_two(uint32_t value)
{
        return value != 0 && (value & (value - 1U)) == 0;
}

static bool elf_ranges_overlap(uint32_t first_start, uint32_t first_end,
                               uint32_t second_start, uint32_t second_end)
{
        return first_start < second_end && second_start < first_end;
}

static bool elf_page_in_segments(const struct elf32_load_segment *segments,
                                 uint32_t count, uint32_t page)
{
        for (uint32_t index = 0; index < count; index++) {
                if (page >= segments[index].map_start &&
                    page < segments[index].map_end)
                        return true;
        }
        return false;
}

static int validate_elf_source(int fd, uint32_t file_size, const char *path)
{
        struct elf32_header header;
        struct elf32_load_segment segments[ELF32_MAX_SEGMENTS];
        uint32_t segment_count = 0;
        uint32_t page_count = 0;
        bool entry_is_executable = false;
        uint64_t program_end;

        if (file_size < sizeof(header) ||
            read_source_at(fd, &header, sizeof(header), 0) < 0)
                goto invalid;
        if (header.ident[0] != 0x7f || header.ident[1] != 'E' ||
            header.ident[2] != 'L' || header.ident[3] != 'F' ||
            header.ident[4] != ELF32_CLASS_32 ||
            header.ident[5] != ELF32_DATA_LITTLE_ENDIAN ||
            header.ident[6] != ELF32_VERSION_CURRENT ||
            header.type != ELF32_TYPE_EXEC ||
            header.machine != ELF32_MACHINE_386 ||
            header.version != ELF32_VERSION_CURRENT ||
            header.header_size != sizeof(header) ||
            header.program_entry_size != sizeof(struct elf32_program_header) ||
            header.program_count == 0 ||
            header.program_count > ELF32_MAX_SEGMENTS ||
            header.program_offset < sizeof(header))
                goto invalid;
        program_end = (uint64_t) header.program_offset +
                      (uint64_t) header.program_count *
                          header.program_entry_size;
        if (program_end > file_size || program_end > ELF32_MAX_FILE_OFFSET)
                goto invalid;

        memset(segments, 0, sizeof(segments));
        for (uint32_t index = 0; index < header.program_count; index++) {
                struct elf32_program_header program;
                uint32_t offset = header.program_offset +
                                  index * header.program_entry_size;
                uint64_t file_end;
                uint64_t memory_end;
                uint64_t aligned_end;
                uint32_t map_start;
                uint32_t map_end;
                uint32_t new_pages = 0;

                if (read_source_at(fd, &program, sizeof(program), offset) < 0)
                        goto invalid;
                if (program.type == ELF32_PT_INTERP ||
                    program.type == ELF32_PT_DYNAMIC ||
                    program.type == ELF32_PT_TLS)
                        goto invalid;
                if (program.type != ELF32_PT_LOAD)
                        continue;
                if (segment_count == ELF32_MAX_SEGMENTS ||
                    program.file_size > program.memory_size ||
                    (program.flags & ~(ELF32_PF_READ | ELF32_PF_WRITE |
                                       ELF32_PF_EXEC)) != 0 ||
                    !(program.flags & ELF32_PF_READ))
                        goto invalid;
                if (program.alignment > 1U &&
                    (!is_power_of_two(program.alignment) ||
                     (program.virtual_address & (program.alignment - 1U)) !=
                         (program.offset & (program.alignment - 1U))))
                        goto invalid;
                file_end = (uint64_t) program.offset + program.file_size;
                if (program.offset > file_size || file_end > file_size ||
                    file_end > ELF32_MAX_FILE_OFFSET)
                        goto invalid;
                if (program.memory_size == 0)
                        continue;

                memory_end = (uint64_t) program.virtual_address +
                             program.memory_size;
                aligned_end = (memory_end + ELF32_PAGE_SIZE - 1U) &
                              ~(uint64_t) (ELF32_PAGE_SIZE - 1U);
                map_start = program.virtual_address &
                            ~(ELF32_PAGE_SIZE - 1U);
                if (program.virtual_address < ELF32_USER_START ||
                    map_start < ELF32_USER_START ||
                    memory_end > ELF32_USER_END ||
                    aligned_end > ELF32_USER_END)
                        goto invalid;
                map_end = (uint32_t) aligned_end;
                if ((map_end - map_start) / ELF32_PAGE_SIZE >
                    ELF32_MAX_PAGES)
                        goto invalid;
                for (uint32_t previous = 0; previous < segment_count;
                     previous++) {
                        uint32_t previous_end =
                            segments[previous].virtual_address +
                            segments[previous].memory_size;

                        if (elf_ranges_overlap(
                                program.virtual_address,
                                (uint32_t) memory_end,
                                segments[previous].virtual_address,
                                previous_end))
                                goto invalid;
                }
                for (uint32_t page = map_start; page < map_end;
                     page += ELF32_PAGE_SIZE) {
                        if (!elf_page_in_segments(segments, segment_count,
                                                  page))
                                new_pages++;
                }
                if (new_pages > ELF32_MAX_PAGES - page_count)
                        goto invalid;
                segments[segment_count].virtual_address =
                    program.virtual_address;
                segments[segment_count].memory_size = program.memory_size;
                segments[segment_count].map_start = map_start;
                segments[segment_count].map_end = map_end;
                segment_count++;
                page_count += new_pages;
                if ((program.flags & ELF32_PF_EXEC) &&
                    header.entry >= program.virtual_address &&
                    (uint64_t) header.entry < memory_end)
                        entry_is_executable = true;
        }
        if (segment_count == 0 || !entry_is_executable)
                goto invalid;
        return 0;

invalid:
        return errorf("ELF is not accepted by Frog exec loader: %s", path);
}

static int validate_source(struct manifest_entry *entry)
{
        struct stat metadata;
        uint8_t actual[32];
        int fd = open(entry->source, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);

        if (fd < 0)
                return errorf("cannot open source %s: %s", entry->source,
                              strerror(errno));
        if (fstat(fd, &metadata) < 0 || !S_ISREG(metadata.st_mode)) {
                close(fd);
                return errorf("source is not a regular file: %s",
                              entry->source);
        }
        if (metadata.st_size < 0 ||
            (uint64_t) metadata.st_size > FROGFS_MAX_FILE_SIZE) {
                close(fd);
                return errorf("source exceeds FrogFS file limit (%u): %s",
                              FROGFS_MAX_FILE_SIZE, entry->source);
        }
        if ((uint64_t) metadata.st_size != entry->size) {
                close(fd);
                return errorf("source size does not match manifest: %s",
                              entry->source);
        }
        if (hash_open_file(fd, actual) < 0) {
                close(fd);
                return -1;
        }
        if (memcmp(actual, entry->expected_hash, sizeof(actual)) != 0) {
                close(fd);
                return errorf("SHA-256 mismatch for %s", entry->source);
        }
        if (entry->executable &&
            validate_elf_source(fd, (uint32_t) entry->size,
                                entry->source) < 0) {
                close(fd);
                return -1;
        }
        close(fd);
        return 0;
}

static int compare_manifest_entries(const void *left, const void *right)
{
        const struct manifest_entry *first = left;
        const struct manifest_entry *second = right;

        return strcmp(first->target, second->target);
}

static bool is_volume_name_character(char character)
{
        return (character >= 'a' && character <= 'z') ||
               (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9') ||
               character == '.' || character == '_' || character == '-';
}

static int validate_volume_name(const char *volume)
{
        size_t length;

        if (!volume || !volume[0])
                return -1;
        length = strlen(volume);
        if (length > FROGFS_MAX_NAME ||
            !((volume[0] >= 'a' && volume[0] <= 'z') ||
              (volume[0] >= 'A' && volume[0] <= 'Z') ||
              (volume[0] >= '0' && volume[0] <= '9')))
                return -1;
        for (size_t index = 1; index < length; index++) {
                if (!is_volume_name_character(volume[index]))
                        return -1;
        }
        return 0;
}

static int validate_target_path(const char *target)
{
        const char *component;

        if (!target || target[0] != '/')
                return -1;
        if (target[1] == '\0')
                return -1;
        if (target[strlen(target) - 1U] == '/' || strstr(target, "//"))
                return -1;
        component = target + 1;
        while (*component) {
                const char *slash = strchr(component, '/');
                size_t length = slash ? (size_t) (slash - component) :
                                        strlen(component);

                if (length == 0 || length > FROGFS_MAX_NAME ||
                    (length == 1 && component[0] == '.') ||
                    (length == 2 && component[0] == '.' &&
                     component[1] == '.'))
                        return -1;
                if (!slash)
                        break;
                component = slash + 1;
        }
        return 0;
}

static void free_manifest(struct manifest_entry *entries, size_t count)
{
        if (!entries)
                return;
        for (size_t index = 0; index < count; index++) {
                free(entries[index].target);
                free(entries[index].source);
        }
        free(entries);
}

static int sort_and_validate_entries(struct manifest_entry *entries,
                                     size_t count)
{
        qsort(entries, count, sizeof(*entries), compare_manifest_entries);
        for (size_t index = 1; index < count; index++) {
                if (strcmp(entries[index - 1].target,
                           entries[index].target) == 0)
                        return errorf("duplicate target %s",
                                      entries[index].target);
        }
        return 0;
}

static int load_manifest(const char *path, struct manifest_entry **result,
                         size_t *result_count,
                         char volume_name[FROGFS_NAME_BYTES])
{
        FILE *stream = fopen(path, "r");
        struct manifest_entry *entries = NULL;
        size_t count = 0;
        size_t capacity = 0;
        char *line = NULL;
        size_t line_capacity = 0;
        ssize_t line_length;
        size_t line_number = 0;
        bool saw_version = false;
        bool saw_volume = false;

        if (!stream)
                return errorf("cannot open manifest %s: %s", path,
                              strerror(errno));
        while ((line_length = getline(&line, &line_capacity, stream)) >= 0) {
                char *save = NULL;
                char *kind;
                char *target;
                char *source;
                char *size;
                char *hash;
                char *extra;

                line_number++;
                if ((size_t) line_length > MANIFEST_LINE_LIMIT) {
                        errorf("manifest line %zu is too long", line_number);
                        goto fail;
                }
                kind = strtok_r(line, " \t\r\n", &save);
                if (!kind || kind[0] == '#')
                        continue;
                target = strtok_r(NULL, " \t\r\n", &save);
                source = strtok_r(NULL, " \t\r\n", &save);
                size = strtok_r(NULL, " \t\r\n", &save);
                hash = strtok_r(NULL, " \t\r\n", &save);
                extra = strtok_r(NULL, " \t\r\n", &save);
                if (strcmp(kind, "frogfs-manifest") == 0) {
                        if (saw_version || saw_volume || count != 0 ||
                            !target || source ||
                            size || hash || extra ||
                            strcmp(target, "2") != 0) {
                                errorf("invalid manifest version on line %zu",
                                       line_number);
                                goto fail;
                        }
                        saw_version = true;
                        continue;
                }
                if (strcmp(kind, "volume") == 0) {
                        if (!saw_version || saw_volume || !target || source ||
                            size || hash || extra ||
                            validate_volume_name(target) < 0) {
                                errorf("invalid volume record on line %zu",
                                       line_number);
                                goto fail;
                        }
                        memcpy(volume_name, target, strlen(target) + 1U);
                        saw_volume = true;
                        continue;
                }
                if (!saw_version ||
                    (strcmp(kind, "dir") != 0 &&
                     strcmp(kind, "file") != 0 &&
                     strcmp(kind, "elf") != 0) ||
                    !target ||
                    ((strcmp(kind, "dir") == 0 &&
                      (source || size || hash || extra)) ||
                     (strcmp(kind, "dir") != 0 &&
                      (!source || !size || !hash || extra))) ||
                    validate_target_path(target) < 0) {
                        errorf("invalid manifest line %zu", line_number);
                        goto fail;
                }
                if (strcmp(kind, "dir") != 0 && source[0] == '/') {
                        errorf("manifest source must be relative on line %zu",
                              line_number);
                        goto fail;
                }
                if (count == capacity) {
                        size_t next_capacity = capacity ? capacity * 2U : 8U;
                        void *next;

                        if (next_capacity > FROGFS_INODES) {
                                errorf("manifest has too many entries");
                                goto fail;
                        }
                        next = realloc(entries,
                                       next_capacity * sizeof(*entries));
                        if (!next) {
                                errorf("out of memory loading manifest");
                                goto fail;
                        }
                        entries = next;
                        memset(entries + capacity, 0,
                               (next_capacity - capacity) * sizeof(*entries));
                        capacity = next_capacity;
                }
                entries[count].target = strdup(target);
                entries[count].directory = strcmp(kind, "dir") == 0;
                entries[count].executable = strcmp(kind, "elf") == 0;
                if (!entries[count].directory)
                        entries[count].source = join_source_path(path, source);
                if (!entries[count].target ||
                    (!entries[count].directory &&
                     (!entries[count].source ||
                      parse_size(size, &entries[count].size) < 0 ||
                      parse_hash(hash, entries[count].expected_hash) < 0))) {
                        errorf("invalid manifest data on line %zu",
                               line_number);
                        goto fail;
                }
                if (!entries[count].directory &&
                    validate_source(&entries[count]) < 0)
                        goto fail;
                count++;
        }
        if (ferror(stream)) {
                errorf("cannot read manifest %s", path);
                goto fail;
        }
        if (!saw_version) {
                errorf("manifest is missing frogfs-manifest %u",
                       MANIFEST_VERSION);
                goto fail;
        }
        if (!saw_volume) {
                errorf("manifest is missing volume record");
                goto fail;
        }
        free(line);
        fclose(stream);
        if (sort_and_validate_entries(entries, count) < 0) {
                free_manifest(entries, count);
                return -1;
        }
        *result = entries;
        *result_count = count;
        return 0;

fail:
        free(line);
        fclose(stream);
        free_manifest(entries, capacity);
        return -1;
}

static int load_overlay(const char *path, struct manifest_entry **result,
                        size_t *result_count)
{
        FILE *stream = fopen(path, "r");
        struct manifest_entry *entries = NULL;
        size_t count = 0;
        size_t capacity = 0;
        char *line = NULL;
        size_t line_capacity = 0;
        ssize_t line_length;
        size_t line_number = 0;
        bool saw_header = false;

        if (!stream)
                return errorf("cannot open overlay %s: %s", path,
                              strerror(errno));
        while ((line_length = getline(&line, &line_capacity, stream)) >= 0) {
                char *save = NULL;
                char *kind;
                char *target;
                char *source;
                char *size;
                char *hash;
                char *extra;

                line_number++;
                if ((size_t) line_length > MANIFEST_LINE_LIMIT) {
                        errorf("overlay line %zu is too long", line_number);
                        goto fail;
                }
                kind = strtok_r(line, " \t\r\n", &save);
                if (!kind || kind[0] == '#')
                        continue;
                target = strtok_r(NULL, " \t\r\n", &save);
                source = strtok_r(NULL, " \t\r\n", &save);
                size = strtok_r(NULL, " \t\r\n", &save);
                hash = strtok_r(NULL, " \t\r\n", &save);
                extra = strtok_r(NULL, " \t\r\n", &save);
                if (strcmp(kind, "frogfs-overlay") == 0) {
                        if (saw_header || count != 0 || !target || source ||
                            size || hash || extra ||
                            strcmp(target, "1") != 0) {
                                errorf("invalid overlay header on line %zu",
                                       line_number);
                                goto fail;
                        }
                        saw_header = true;
                        continue;
                }
                if (!saw_header ||
                    (strcmp(kind, "dir") != 0 &&
                     strcmp(kind, "file") != 0 &&
                     strcmp(kind, "elf") != 0) ||
                    !target ||
                    ((strcmp(kind, "dir") == 0 &&
                      (source || size || hash || extra)) ||
                     (strcmp(kind, "dir") != 0 &&
                      (!source || !size || !hash || extra))) ||
                    validate_target_path(target) < 0) {
                        errorf("invalid overlay line %zu", line_number);
                        goto fail;
                }
                if (strcmp(kind, "dir") != 0 && source[0] == '/') {
                        errorf("overlay source must be relative on line %zu",
                               line_number);
                        goto fail;
                }
                if (count == capacity) {
                        size_t next_capacity = capacity ? capacity * 2U : 8U;
                        void *next;

                        if (next_capacity > FROGFS_INODES) {
                                errorf("overlay has too many entries");
                                goto fail;
                        }
                        next = realloc(entries,
                                       next_capacity * sizeof(*entries));
                        if (!next) {
                                errorf("out of memory loading overlay");
                                goto fail;
                        }
                        entries = next;
                        memset(entries + capacity, 0,
                               (next_capacity - capacity) * sizeof(*entries));
                        capacity = next_capacity;
                }
                entries[count].target = strdup(target);
                entries[count].directory = strcmp(kind, "dir") == 0;
                entries[count].executable = strcmp(kind, "elf") == 0;
                if (!entries[count].directory)
                        entries[count].source = join_source_path(path, source);
                if (!entries[count].target ||
                    (!entries[count].directory &&
                     (!entries[count].source ||
                      parse_size(size, &entries[count].size) < 0 ||
                      parse_hash(hash, entries[count].expected_hash) < 0))) {
                        errorf("invalid overlay data on line %zu", line_number);
                        goto fail;
                }
                if (!entries[count].directory &&
                    validate_source(&entries[count]) < 0)
                        goto fail;
                count++;
        }
        if (ferror(stream)) {
                errorf("cannot read overlay %s", path);
                goto fail;
        }
        if (!saw_header) {
                errorf("overlay is missing frogfs-overlay 1");
                goto fail;
        }
        free(line);
        fclose(stream);
        if (sort_and_validate_entries(entries, count) < 0)
                goto fail_without_stream;
        *result = entries;
        *result_count = count;
        return 0;

fail:
        free(line);
        fclose(stream);
fail_without_stream:
        free_manifest(entries, capacity);
        return -1;
}

static int merge_overlay(struct manifest_entry **base_entries,
                         size_t *base_count,
                         struct manifest_entry *overlay_entries,
                         size_t overlay_count)
{
        struct manifest_entry *merged;
        size_t total;

        if (overlay_count > SIZE_MAX - *base_count)
                return errorf("manifest and overlay are too large");
        total = *base_count + overlay_count;
        merged = realloc(*base_entries, total * sizeof(*merged));
        if (!merged && total)
                return errorf("out of memory merging overlay");
        if (overlay_count)
                memcpy(merged + *base_count, overlay_entries,
                       overlay_count * sizeof(*overlay_entries));
        free(overlay_entries);
        *base_entries = merged;
        *base_count = total;
        return sort_and_validate_entries(merged, total);
}

static int ensure_node_capacity(struct image_builder *builder)
{
        if (builder->node_count < builder->node_capacity)
                return 0;
        size_t next_capacity = builder->node_capacity ?
                               builder->node_capacity * 2U : 8U;
        if (next_capacity > FROGFS_INODES)
                next_capacity = FROGFS_INODES;
        if (next_capacity <= builder->node_capacity)
                return errorf("FrogFS inode limit exceeded");
        void *next = realloc(builder->nodes,
                             next_capacity * sizeof(*builder->nodes));
        if (!next)
                return errorf("out of memory creating namespace");
        builder->nodes = next;
        memset(builder->nodes + builder->node_capacity, 0,
               (next_capacity - builder->node_capacity) *
                   sizeof(*builder->nodes));
        builder->node_capacity = next_capacity;
        return 0;
}

static int find_child(const struct image_builder *builder, uint32_t parent,
                      const char *name)
{
        for (size_t index = 1; index < builder->node_count; index++) {
                if (builder->nodes[index].parent == parent &&
                    strcmp(builder->nodes[index].name, name) == 0)
                        return (int) index;
        }
        return -1;
}

static int add_node(struct image_builder *builder, uint32_t parent,
                    const char *name, uint32_t type,
                    const struct manifest_entry *manifest)
{
        size_t length = strlen(name);

        if (length == 0 || length > FROGFS_MAX_NAME ||
            strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                return errorf("invalid FrogFS path component: %s", name);
        if (ensure_node_capacity(builder) < 0)
                return -1;
        struct fs_node *node = &builder->nodes[builder->node_count];
        node->parent = parent;
        node->inode_number = (uint32_t) builder->node_count;
        node->type = type;
        node->manifest = manifest;
        memcpy(node->name, name, length + 1U);
        builder->node_count++;
        return (int) (builder->node_count - 1U);
}

static int add_manifest_path(struct image_builder *builder,
                             const struct manifest_entry *entry)
{
        char *copy;
        char *save = NULL;
        char *component;
        uint32_t parent = 0;

        if (validate_target_path(entry->target) < 0)
                return errorf("target must be a normalized absolute path: %s",
                              entry->target);
        copy = strdup(entry->target + 1);
        if (!copy)
                return errorf("out of memory parsing target");
        component = strtok_r(copy, "/", &save);
        while (component) {
                char *next = strtok_r(NULL, "/", &save);
                int existing = find_child(builder, parent, component);

                if (next) {
                        if (existing >= 0) {
                                if (builder->nodes[existing].type !=
                                    FROGFS_TYPE_DIRECTORY) {
                                        free(copy);
                                        return errorf("path traverses a file: %s",
                                                      entry->target);
                                }
                                parent = (uint32_t) existing;
                        } else {
                                free(copy);
                                return errorf("parent directory is not "
                                              "declared: %s", entry->target);
                        }
                } else {
                        if (existing >= 0) {
                                free(copy);
                                return errorf("duplicate namespace entry: %s",
                                              entry->target);
                        }
                        if (add_node(builder, parent, component,
                                     entry->directory ? FROGFS_TYPE_DIRECTORY :
                                                        FROGFS_TYPE_REGULAR,
                                     entry->directory ? NULL : entry) < 0) {
                                free(copy);
                                return -1;
                        }
                }
                component = next;
        }
        free(copy);
        return 0;
}

static uint32_t round_up_u32(uint32_t value, uint32_t divisor)
{
        return value / divisor + (value % divisor != 0U);
}

static void set_bitmap_bit(uint8_t *bitmap, uint32_t bit)
{
        bitmap[bit / 8U] |= (uint8_t) (1U << (bit % 8U));
}

static int initialize_layout(struct image_builder *builder,
                             const char volume_name[FROGFS_NAME_BYTES])
{
        uint32_t start_block = PARTITION_START_LBA /
                               FROGFS_SECTORS_PER_ZONE;
        uint32_t total_blocks = PARTITION_SECTORS /
                               FROGFS_SECTORS_PER_ZONE;
        uint32_t inode_bitmap_blocks =
            round_up_u32(FROGFS_INODES, FROGFS_BITS_PER_ZONE);
        uint32_t inode_table_blocks = round_up_u32(
            FROGFS_INODES * (uint32_t) sizeof(struct frogfs_inode_disk),
            FROGFS_ZONE_SIZE);
        uint32_t fixed_blocks = 1U + inode_bitmap_blocks +
                                inode_table_blocks;
        uint32_t available;
        uint32_t zone_bitmap_blocks;
        uint32_t data_blocks;

        if (PARTITION_START_LBA % FROGFS_SECTORS_PER_ZONE ||
            PARTITION_SECTORS % FROGFS_SECTORS_PER_ZONE ||
            total_blocks <= fixed_blocks + 1U)
                return errorf("invalid fixed image geometry");
        available = total_blocks - fixed_blocks;
        zone_bitmap_blocks = round_up_u32(
            available, FROGFS_BITS_PER_ZONE + 1U);
        data_blocks = available - zone_bitmap_blocks;
        if (!data_blocks || zone_bitmap_blocks <
            round_up_u32(data_blocks, FROGFS_BITS_PER_ZONE))
                return errorf("FrogFS metadata does not fit image");

        memset(&builder->super, 0, sizeof(builder->super));
        builder->super.magic = FROGFS_MAGIC;
        memcpy(builder->super.volume_name, volume_name,
               strlen(volume_name));
        builder->super.inode_count = FROGFS_INODES;
        builder->super.inode_size = sizeof(struct frogfs_inode_disk);
        builder->super.zone_count = data_blocks;
        builder->super.zone_size = FROGFS_ZONE_SIZE;
        builder->super.inode_bitmap_block = start_block + 1U;
        builder->super.inode_bitmap_blocks = inode_bitmap_blocks;
        builder->super.zone_bitmap_block =
            builder->super.inode_bitmap_block + inode_bitmap_blocks;
        builder->super.zone_bitmap_blocks = zone_bitmap_blocks;
        builder->super.inode_table_block =
            builder->super.zone_bitmap_block + zone_bitmap_blocks;
        builder->super.inode_table_blocks = inode_table_blocks;
        builder->super.data_start_block =
            builder->super.inode_table_block + inode_table_blocks;
        builder->super.root_inode = 0;
        builder->super.dir_entry_size = sizeof(struct frogfs_dir_entry_disk);
        builder->super.log_zone_size = 1;
        builder->super.max_file_size = FROGFS_MAX_FILE_SIZE;
        builder->super.read_only = 1;
        if ((uint64_t) builder->super.data_start_block + data_blocks !=
            (uint64_t) start_block + total_blocks)
                return errorf("FrogFS end offset mismatch");

        builder->inode_bitmap = calloc(inode_bitmap_blocks,
                                       FROGFS_ZONE_SIZE);
        builder->zone_bitmap = calloc(zone_bitmap_blocks,
                                      FROGFS_ZONE_SIZE);
        if (!builder->inode_bitmap || !builder->zone_bitmap)
                return errorf("out of memory allocating bitmaps");
        builder->next_zone = 1;
        set_bitmap_bit(builder->zone_bitmap, 0);
        return 0;
}

static int allocate_zone(struct image_builder *builder, uint32_t *block)
{
        if (builder->next_zone >= builder->super.zone_count)
                return errorf("FrogFS image ran out of data zones");
        set_bitmap_bit(builder->zone_bitmap, builder->next_zone);
        *block = builder->super.data_start_block + builder->next_zone;
        builder->next_zone++;
        return 0;
}

static int map_node_block(struct image_builder *builder,
                          struct fs_node *node, uint32_t file_block,
                          uint32_t *disk_block)
{
        if (file_block >= FROGFS_MAX_DATA_ZONES)
                return errorf("file block exceeds FrogFS limit");
        if (file_block < FROGFS_DIRECT_ZONES) {
                if (node->inode.zones[file_block] == 0) {
                        if (node->inode_number == 0 && file_block == 0) {
                                node->inode.zones[file_block] =
                                    builder->super.data_start_block;
                        } else if (allocate_zone(
                                       builder,
                                       &node->inode.zones[file_block]) < 0) {
                                return -1;
                        }
                        node->inode.blocks++;
                }
                *disk_block = node->inode.zones[file_block];
                return 0;
        }

        uint32_t relative = file_block - FROGFS_DIRECT_ZONES;
        uint32_t table = relative / FROGFS_INDIRECT_ENTRIES;
        uint32_t entry = relative % FROGFS_INDIRECT_ENTRIES;
        if (table >= FROGFS_INDIRECT_TABLES)
                return errorf("indirect table index exceeds FrogFS limit");
        if (!node->indirect[table]) {
                node->indirect[table] = calloc(FROGFS_INDIRECT_ENTRIES,
                                               sizeof(uint32_t));
                if (!node->indirect[table])
                        return errorf("out of memory allocating indirect table");
                if (allocate_zone(builder,
                                  &node->inode.zones[
                                      FROGFS_DIRECT_ZONES + table]) < 0)
                        return -1;
                node->inode.blocks++;
        }
        if (node->indirect[table][entry] == 0) {
                if (allocate_zone(builder,
                                  &node->indirect[table][entry]) < 0)
                        return -1;
                node->inode.blocks++;
        }
        *disk_block = node->indirect[table][entry];
        return 0;
}

static off_t block_offset(uint32_t block)
{
        return (off_t) block * FROGFS_ZONE_SIZE;
}

static size_t child_count(const struct image_builder *builder,
                          uint32_t parent, bool directories_only)
{
        size_t count = 0;

        for (size_t index = 1; index < builder->node_count; index++) {
                if (builder->nodes[index].parent == parent &&
                    (!directories_only || builder->nodes[index].type ==
                                              FROGFS_TYPE_DIRECTORY))
                        count++;
        }
        return count;
}

static void initialize_dir_entry(struct frogfs_dir_entry_disk *entry,
                                 const char *name, uint32_t inode_number,
                                 uint32_t type)
{
        memset(entry, 0, sizeof(*entry));
        memcpy(entry->name, name, strlen(name));
        entry->inode_number = inode_number;
        entry->file_type = type;
}

static int write_directory(struct image_builder *builder,
                           struct fs_node *node)
{
        size_t children = child_count(builder, node->inode_number, false);
        size_t entry_count = children + 2U;
        size_t entries_per_zone = FROGFS_ZONE_SIZE /
                                  sizeof(struct frogfs_dir_entry_disk);
        size_t cursor = 2;

        if (entry_count > FROGFS_MAX_FILE_SIZE /
                              sizeof(struct frogfs_dir_entry_disk))
                return errorf("directory is too large: inode %u",
                              node->inode_number);
        uint32_t required = (uint32_t) round_up_u32(
            (uint32_t) entry_count, (uint32_t) entries_per_zone);
        size_t storage_size = (size_t) required * FROGFS_ZONE_SIZE;
        uint8_t *storage = calloc(1, storage_size);
        if (!storage)
                return errorf("out of memory writing directory");
        struct frogfs_dir_entry_disk *entries = calloc(
            entry_count, sizeof(*entries));
        if (!entries)
                goto no_memory;
        initialize_dir_entry(&entries[0], ".", node->inode_number,
                             FROGFS_TYPE_DIRECTORY);
        initialize_dir_entry(&entries[1], "..", node->parent,
                             FROGFS_TYPE_DIRECTORY);
        for (size_t index = 1; index < builder->node_count; index++) {
                const struct fs_node *child = &builder->nodes[index];

                if (child->parent != node->inode_number)
                        continue;
                initialize_dir_entry(&entries[cursor++], child->name,
                                     child->inode_number, child->type);
        }
        size_t stored_entry_count = required == 1U ? entry_count :
                                    (size_t) required * entries_per_zone;
        node->inode.size =
            (uint32_t) (stored_entry_count * sizeof(*entries));
        for (size_t index = 0; index < entry_count; index++) {
                size_t block_index = index / entries_per_zone;
                size_t block_entry = index % entries_per_zone;

                memcpy(storage + block_index * FROGFS_ZONE_SIZE +
                           block_entry * sizeof(*entries),
                       &entries[index], sizeof(*entries));
        }
        for (uint32_t index = 0; index < required; index++) {
                uint32_t disk_block;

                if (map_node_block(builder, node, index, &disk_block) < 0 ||
                    write_at(builder->fd,
                             storage + (size_t) index * FROGFS_ZONE_SIZE,
                             FROGFS_ZONE_SIZE,
                             block_offset(disk_block)) < 0) {
                        free(storage);
                        free(entries);
                        return -1;
                }
        }
        free(storage);
        free(entries);
        return 0;

no_memory:
        free(storage);
        return errorf("out of memory writing directory");
}

static int write_regular_file(struct image_builder *builder,
                              struct fs_node *node)
{
        const struct manifest_entry *entry = node->manifest;
        struct stat metadata;
        struct sha256_state hash;
        uint64_t remaining = entry->size;
        int fd = open(entry->source, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);

        if (fd < 0)
                return errorf("cannot reopen source %s: %s", entry->source,
                              strerror(errno));
        if (fstat(fd, &metadata) < 0 || !S_ISREG(metadata.st_mode) ||
            metadata.st_size < 0 ||
            (uint64_t) metadata.st_size != entry->size) {
                close(fd);
                return errorf("source changed while building: %s",
                              entry->source);
        }
        sha256_init(&hash);
        node->inode.size = (uint32_t) entry->size;
        uint32_t required = round_up_u32(node->inode.size,
                                         FROGFS_ZONE_SIZE);
        for (uint32_t index = 0; index < required; index++) {
                uint8_t block[FROGFS_ZONE_SIZE] = {0};
                size_t take = remaining < FROGFS_ZONE_SIZE ?
                              (size_t) remaining : FROGFS_ZONE_SIZE;
                size_t received = 0;
                uint32_t disk_block;

                while (received < take) {
                        int count = read_retry(fd, block + received,
                                               take - received);
                        if (count <= 0) {
                                close(fd);
                                return errorf("short read from %s",
                                              entry->source);
                        }
                        received += (size_t) count;
                }
                sha256_update(&hash, block, take);
                if (map_node_block(builder, node, index, &disk_block) < 0 ||
                    write_at(builder->fd, block, sizeof(block),
                             block_offset(disk_block)) < 0) {
                        close(fd);
                        return -1;
                }
                remaining -= take;
        }
        uint8_t extra;
        int extra_count = read_retry(fd, &extra, 1);
        close(fd);
        if (extra_count != 0)
                return errorf("source grew while building: %s",
                              entry->source);
        uint8_t actual[32];
        sha256_finish(&hash, actual);
        if (memcmp(actual, entry->expected_hash, sizeof(actual)) != 0)
                return errorf("source hash changed while building: %s",
                              entry->source);
        return 0;
}

static int write_indirect_tables(struct image_builder *builder,
                                 struct fs_node *node)
{
        for (size_t table = 0; table < FROGFS_INDIRECT_TABLES; table++) {
                if (!node->indirect[table])
                        continue;
                uint32_t disk_block =
                    node->inode.zones[FROGFS_DIRECT_ZONES + table];
                if (write_at(builder->fd, node->indirect[table],
                             FROGFS_ZONE_SIZE,
                             block_offset(disk_block)) < 0)
                        return -1;
        }
        return 0;
}

static int write_namespace(struct image_builder *builder)
{
        for (size_t index = 0; index < builder->node_count; index++) {
                struct fs_node *node = &builder->nodes[index];
                size_t subdirectories = child_count(
                    builder, node->inode_number, true);

                memset(&node->inode, 0, sizeof(node->inode));
                node->inode.inode_number = node->inode_number;
                if (node->type == FROGFS_TYPE_DIRECTORY) {
                        if (subdirectories > UINT8_MAX - 2U)
                                return errorf("directory link count overflow");
                        node->inode.mode = (uint16_t) (
                            FROGFS_TYPE_DIRECTORY << FROGFS_MODE_SHIFT | 0755U);
                        node->inode.link_count =
                            (uint8_t) (2U + subdirectories);
                        if (write_directory(builder, node) < 0)
                                return -1;
                } else {
                        node->inode.mode = (uint16_t) (
                            FROGFS_TYPE_REGULAR << FROGFS_MODE_SHIFT | 0644U);
                        node->inode.link_count = 1;
                        if (write_regular_file(builder, node) < 0)
                                return -1;
                }
                if (write_indirect_tables(builder, node) < 0)
                        return -1;
        }
        return 0;
}

static int write_inodes_and_bitmaps(struct image_builder *builder)
{
        size_t inode_bitmap_bytes =
            builder->super.inode_bitmap_blocks * FROGFS_ZONE_SIZE;
        size_t zone_bitmap_bytes =
            builder->super.zone_bitmap_blocks * FROGFS_ZONE_SIZE;

        for (size_t index = 0; index < builder->node_count; index++) {
                struct fs_node *node = &builder->nodes[index];
                off_t offset = block_offset(builder->super.inode_table_block) +
                               (off_t) index * sizeof(node->inode);

                set_bitmap_bit(builder->inode_bitmap, (uint32_t) index);
                if (write_at(builder->fd, &node->inode,
                             sizeof(node->inode), offset) < 0)
                        return -1;
        }
        for (uint32_t bit = FROGFS_INODES;
             bit < inode_bitmap_bytes * 8U; bit++)
                set_bitmap_bit(builder->inode_bitmap, bit);
        for (uint32_t bit = builder->super.zone_count;
             bit < zone_bitmap_bytes * 8U; bit++)
                set_bitmap_bit(builder->zone_bitmap, bit);
        if (write_at(builder->fd, builder->inode_bitmap,
                     inode_bitmap_bytes,
                     block_offset(builder->super.inode_bitmap_block)) < 0 ||
            write_at(builder->fd, builder->zone_bitmap,
                     zone_bitmap_bytes,
                     block_offset(builder->super.zone_bitmap_block)) < 0)
                return -1;
        return 0;
}

static void encode_chs(uint32_t lba, uint8_t *head, uint8_t *sector,
                       uint8_t *cylinder)
{
        uint32_t cylinder_value = lba / (255U * 63U);
        uint32_t remainder = lba % (255U * 63U);

        if (cylinder_value > 1023U)
                cylinder_value = 1023U;
        *head = (uint8_t) (remainder / 63U);
        *sector = (uint8_t) ((remainder % 63U + 1U) |
                            ((cylinder_value >> 2) & 0xc0U));
        *cylinder = (uint8_t) cylinder_value;
}

static void initialize_mbr(struct mbr_disk *mbr)
{
        struct mbr_partition *partition;
        uint32_t end_lba = PARTITION_START_LBA + PARTITION_SECTORS - 1U;

        memset(mbr, 0, sizeof(*mbr));
        mbr->disk_signature = 0x46524f47U;
        partition = &mbr->partitions[0];
        partition->type = 0x83;
        partition->start_lba = PARTITION_START_LBA;
        partition->sector_count = PARTITION_SECTORS;
        encode_chs(PARTITION_START_LBA, &partition->start_head,
                   &partition->start_sector, &partition->start_cylinder);
        encode_chs(end_lba, &partition->end_head,
                   &partition->end_sector, &partition->end_cylinder);
        mbr->signature = 0xaa55U;
}

static int write_mbr(struct image_builder *builder)
{
        struct mbr_disk mbr;

        initialize_mbr(&mbr);
        return write_at(builder->fd, &mbr, sizeof(mbr), 0);
}

static bool block_is_data(const struct image_builder *builder,
                          uint32_t block)
{
        return block >= builder->super.data_start_block &&
               block - builder->super.data_start_block <
                   builder->super.zone_count;
}

static int readback_node_block(const struct image_builder *builder,
                               const struct frogfs_inode_disk *inode,
                               uint32_t file_block, uint32_t *disk_block)
{
        uint32_t block;

        if (file_block >= FROGFS_MAX_DATA_ZONES)
                return errorf("readback file block exceeds FrogFS limit");
        if (file_block < FROGFS_DIRECT_ZONES) {
                block = inode->zones[file_block];
        } else {
                uint32_t relative = file_block - FROGFS_DIRECT_ZONES;
                uint32_t table = relative / FROGFS_INDIRECT_ENTRIES;
                uint32_t entry = relative % FROGFS_INDIRECT_ENTRIES;

                if (table >= FROGFS_INDIRECT_TABLES ||
                    !block_is_data(builder,
                                   inode->zones[FROGFS_DIRECT_ZONES + table]))
                        return errorf("invalid readback indirect table");
                off_t offset = block_offset(
                                   inode->zones[FROGFS_DIRECT_ZONES + table]) +
                               (off_t) entry * sizeof(block);
                if (read_at(builder->fd, &block, sizeof(block), offset) < 0)
                        return -1;
        }
        if (!block_is_data(builder, block))
                return errorf("invalid readback data block");
        *disk_block = block;
        return 0;
}

static int expected_directory_entry(const struct image_builder *builder,
                                    const struct fs_node *node,
                                    size_t entry_index,
                                    struct frogfs_dir_entry_disk *entry)
{
        if (entry_index == 0) {
                initialize_dir_entry(entry, ".", node->inode_number,
                                     FROGFS_TYPE_DIRECTORY);
                return 0;
        }
        if (entry_index == 1) {
                initialize_dir_entry(entry, "..", node->parent,
                                     FROGFS_TYPE_DIRECTORY);
                return 0;
        }
        size_t child_index = entry_index - 2U;
        for (size_t index = 1; index < builder->node_count; index++) {
                const struct fs_node *child = &builder->nodes[index];

                if (child->parent != node->inode_number)
                        continue;
                if (child_index-- != 0)
                        continue;
                initialize_dir_entry(entry, child->name,
                                     child->inode_number, child->type);
                return 0;
        }
        return errorf("missing expected directory entry");
}

static int verify_directory_readback(const struct image_builder *builder,
                                     const struct fs_node *node,
                                     const struct frogfs_inode_disk *inode)
{
        size_t entry_size = sizeof(struct frogfs_dir_entry_disk);
        size_t entries_per_zone = FROGFS_ZONE_SIZE / entry_size;
        size_t entry_count = child_count(builder, node->inode_number, false) +
                             2U;
        size_t required = round_up_u32((uint32_t) entry_count,
                                       (uint32_t) entries_per_zone);
        size_t stored_entry_count = required == 1U ? entry_count :
                                    required * entries_per_zone;

        if (inode->size != stored_entry_count * entry_size)
                return errorf("directory size changed during readback");
        for (size_t index = 0; index < stored_entry_count; index++) {
                uint32_t file_block = (uint32_t) (index / entries_per_zone);
                uint32_t disk_block;
                struct frogfs_dir_entry_disk actual;
                struct frogfs_dir_entry_disk expected;
                off_t offset;

                memset(&expected, 0, sizeof(expected));
                if (readback_node_block(builder, inode, file_block,
                                        &disk_block) < 0)
                        return -1;
                if (index < entry_count &&
                    expected_directory_entry(builder, node, index,
                                             &expected) < 0)
                        return -1;
                offset = block_offset(disk_block) +
                         (off_t) (index % entries_per_zone) * entry_size;
                if (read_at(builder->fd, &actual, sizeof(actual), offset) < 0)
                        return -1;
                if (memcmp(&actual, &expected, sizeof(actual)) != 0)
                        return errorf("directory entry changed during readback");
        }
        return 0;
}

static int verify_regular_readback(const struct image_builder *builder,
                                   const struct fs_node *node,
                                   const struct frogfs_inode_disk *inode)
{
        uint64_t remaining = node->manifest->size;
        int source = open(node->manifest->source,
                          O_RDONLY | O_CLOEXEC | O_NOFOLLOW);

        if (source < 0)
                return errorf("cannot reopen source for readback: %s",
                              strerror(errno));
        uint32_t required = round_up_u32(inode->size, FROGFS_ZONE_SIZE);
        for (uint32_t index = 0; index < required; index++) {
                uint8_t actual[FROGFS_ZONE_SIZE];
                uint8_t expected[FROGFS_ZONE_SIZE] = {0};
                size_t take = remaining < FROGFS_ZONE_SIZE ?
                              (size_t) remaining : FROGFS_ZONE_SIZE;
                size_t received = 0;
                uint32_t disk_block;

                while (received < take) {
                        int count = read_retry(source, expected + received,
                                               take - received);
                        if (count <= 0) {
                                close(source);
                                return errorf("source changed during readback");
                        }
                        received += (size_t) count;
                }
                if (readback_node_block(builder, inode, index,
                                        &disk_block) < 0 ||
                    read_at(builder->fd, actual, sizeof(actual),
                            block_offset(disk_block)) < 0) {
                        close(source);
                        return -1;
                }
                if (memcmp(actual, expected, sizeof(actual)) != 0) {
                        close(source);
                        return errorf("file data changed during readback: %s",
                                      node->manifest->target);
                }
                remaining -= take;
        }
        uint8_t extra;
        int extra_count = read_retry(source, &extra, 1);
        close(source);
        if (extra_count != 0 || remaining != 0)
                return errorf("source size changed during readback: %s",
                              node->manifest->target);
        return 0;
}

static int verify_written_image(struct image_builder *builder)
{
        struct mbr_disk actual_mbr;
        struct mbr_disk expected_mbr;
        struct frogfs_super_disk actual_super;
        size_t inode_bitmap_bytes =
            builder->super.inode_bitmap_blocks * FROGFS_ZONE_SIZE;
        size_t zone_bitmap_bytes =
            builder->super.zone_bitmap_blocks * FROGFS_ZONE_SIZE;
        size_t bitmap_bytes = inode_bitmap_bytes > zone_bitmap_bytes ?
                              inode_bitmap_bytes : zone_bitmap_bytes;
        uint8_t *bitmap = malloc(bitmap_bytes);

        if (!bitmap)
                return errorf("out of memory verifying image");
        initialize_mbr(&expected_mbr);
        if (read_at(builder->fd, &actual_mbr, sizeof(actual_mbr), 0) < 0 ||
            memcmp(&actual_mbr, &expected_mbr, sizeof(actual_mbr)) != 0) {
                free(bitmap);
                return errorf("MBR changed during readback");
        }
        if (read_at(builder->fd, &actual_super, sizeof(actual_super),
                    (off_t) PARTITION_START_LBA * SECTOR_SIZE) < 0 ||
            memcmp(&actual_super, &builder->super,
                   sizeof(actual_super)) != 0) {
                free(bitmap);
                return errorf("superblock changed during readback");
        }
        if (read_at(builder->fd, bitmap, inode_bitmap_bytes,
                    block_offset(builder->super.inode_bitmap_block)) < 0 ||
            memcmp(bitmap, builder->inode_bitmap,
                   inode_bitmap_bytes) != 0) {
                free(bitmap);
                return errorf("inode bitmap changed during readback");
        }
        if (read_at(builder->fd, bitmap, zone_bitmap_bytes,
                    block_offset(builder->super.zone_bitmap_block)) < 0 ||
            memcmp(bitmap, builder->zone_bitmap,
                   zone_bitmap_bytes) != 0) {
                free(bitmap);
                return errorf("zone bitmap changed during readback");
        }
        free(bitmap);

        for (size_t index = 0; index < builder->node_count; index++) {
                const struct fs_node *node = &builder->nodes[index];
                struct frogfs_inode_disk inode;
                off_t inode_offset =
                    block_offset(builder->super.inode_table_block) +
                    (off_t) index * sizeof(inode);

                if (read_at(builder->fd, &inode, sizeof(inode),
                            inode_offset) < 0)
                        return -1;
                if (memcmp(&inode, &node->inode, sizeof(inode)) != 0)
                        return errorf("inode changed during readback: %zu",
                                      index);
                for (size_t table = 0; table < FROGFS_INDIRECT_TABLES;
                     table++) {
                        if (!node->indirect[table])
                                continue;
                        uint32_t table_block =
                            inode.zones[FROGFS_DIRECT_ZONES + table];
                        uint32_t entries[FROGFS_INDIRECT_ENTRIES];

                        if (!block_is_data(builder, table_block) ||
                            read_at(builder->fd, entries, sizeof(entries),
                                    block_offset(table_block)) < 0)
                                return -1;
                        if (memcmp(entries, node->indirect[table],
                                   sizeof(entries)) != 0)
                                return errorf("indirect table changed during "
                                              "readback: %zu", index);
                }
                if (node->type == FROGFS_TYPE_DIRECTORY) {
                        if (verify_directory_readback(builder, node,
                                                     &inode) < 0)
                                return -1;
                } else if (verify_regular_readback(builder, node,
                                                   &inode) < 0) {
                        return -1;
                }
        }
        return 0;
}

static void free_builder(struct image_builder *builder)
{
        if (!builder)
                return;
        for (size_t index = 0; index < builder->node_count; index++) {
                for (size_t table = 0; table < FROGFS_INDIRECT_TABLES;
                     table++)
                        free(builder->nodes[index].indirect[table]);
        }
        free(builder->nodes);
        free(builder->inode_bitmap);
        free(builder->zone_bitmap);
        memset(builder, 0, sizeof(*builder));
        builder->fd = -1;
}

static int build_image(int fd, const struct manifest_entry *entries,
                       size_t entry_count,
                       const char volume_name[FROGFS_NAME_BYTES])
{
        struct image_builder builder;
        int result = -1;

        memset(&builder, 0, sizeof(builder));
        builder.fd = fd;
        if (ftruncate(fd, (off_t) IMAGE_SIZE) < 0) {
                errorf("cannot size image: %s", strerror(errno));
                goto out;
        }
        if (ensure_node_capacity(&builder) < 0)
                goto out;
        builder.node_count = 1;
        builder.nodes[0].parent = 0;
        builder.nodes[0].inode_number = 0;
        builder.nodes[0].type = FROGFS_TYPE_DIRECTORY;
        for (size_t index = 0; index < entry_count; index++) {
                if (add_manifest_path(&builder, &entries[index]) < 0)
                        goto out;
        }
        if (initialize_layout(&builder, volume_name) < 0 ||
            write_mbr(&builder) < 0 ||
            write_namespace(&builder) < 0 ||
            write_inodes_and_bitmaps(&builder) < 0)
                goto out;
        if (fsync(fd) < 0) {
                errorf("cannot sync image data: %s", strerror(errno));
                goto out;
        }
        if (write_at(fd, &builder.super, sizeof(builder.super),
                     (off_t) PARTITION_START_LBA * SECTOR_SIZE) < 0 ||
            fsync(fd) < 0) {
                errorf("cannot publish FrogFS superblock: %s",
                       strerror(errno));
                goto out;
        }
        if (verify_written_image(&builder) < 0)
                goto out;
        result = 0;
out:
        free_builder(&builder);
        return result;
}

static int files_equal(const char *first_path, const char *second_path)
{
        uint8_t first_buffer[64 * 1024];
        uint8_t second_buffer[64 * 1024];
        struct stat first_stat;
        struct stat second_stat;
        int first = -1;
        int second = -1;
        int result = -1;

        first = open(first_path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        second = open(second_path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        if (first < 0 || second < 0)
                goto out;
        if (fstat(first, &first_stat) < 0 ||
            fstat(second, &second_stat) < 0 ||
            !S_ISREG(first_stat.st_mode) || !S_ISREG(second_stat.st_mode) ||
            first_stat.st_size != second_stat.st_size) {
                result = 0;
                goto out;
        }
        for (;;) {
                int first_count = read_retry(first, first_buffer,
                                             sizeof(first_buffer));
                int second_count = read_retry(second, second_buffer,
                                              sizeof(second_buffer));

                if (first_count < 0 || second_count < 0)
                        goto out;
                if (first_count != second_count ||
                    memcmp(first_buffer, second_buffer,
                           (size_t) first_count) != 0) {
                        result = 0;
                        goto out;
                }
                if (first_count == 0) {
                        result = 1;
                        goto out;
                }
        }
out:
        if (first >= 0)
                close(first);
        if (second >= 0)
                close(second);
        return result;
}

static char *temporary_path(const char *output)
{
        size_t length = strlen(output) + 64U;
        char *path = malloc(length);

        if (!path)
                return NULL;
        for (unsigned int attempt = 0; attempt < 100U; attempt++) {
                snprintf(path, length, "%s.tmp.%ld.%u", output,
                         (long) getpid(), attempt);
                if (access(path, F_OK) != 0 && errno == ENOENT)
                        return path;
        }
        free(path);
        return NULL;
}

static int sync_parent_directory(const char *path)
{
        char *copy = strdup(path);
        char *slash;
        const char *directory;
        int fd;
        int result;

        if (!copy)
                return -1;
        slash = strrchr(copy, '/');
        if (!slash)
                directory = ".";
        else if (slash == copy) {
                slash[1] = '\0';
                directory = copy;
        } else {
                *slash = '\0';
                directory = copy;
        }
        fd = open(directory, O_RDONLY | O_CLOEXEC | O_DIRECTORY);
        if (fd < 0) {
                free(copy);
                return -1;
        }
        result = fsync(fd);
        int saved_errno = errno;
        close(fd);
        free(copy);
        errno = saved_errno;
        return result < 0 ? -1 : 0;
}

static void usage(const char *program)
{
        fprintf(stderr,
                "usage: %s --manifest BASE [--overlay PATH] --output IMAGE "
                "[--verify]\n",
                program);
}

int main(int argc, char **argv)
{
        const char *manifest_path = NULL;
        const char *output_path = NULL;
        bool verify_only = false;
        struct manifest_entry *entries = NULL;
        size_t entry_count = 0;
        struct manifest_entry *overlay_entries = NULL;
        size_t overlay_count = 0;
        char volume_name[FROGFS_NAME_BYTES] = {0};
        const char *overlay_path = NULL;
        char *temp_path = NULL;
        int temp_fd = -1;
        int result = EXIT_FAILURE;

        for (int index = 1; index < argc; index++) {
                if (strcmp(argv[index], "--verify") == 0) {
                        verify_only = true;
                } else if (strcmp(argv[index], "--manifest") == 0 &&
                           index + 1 < argc) {
                        manifest_path = argv[++index];
                } else if (strcmp(argv[index], "--overlay") == 0 &&
                           !overlay_path && index + 1 < argc) {
                        overlay_path = argv[++index];
                } else if (strcmp(argv[index], "--output") == 0 &&
                           index + 1 < argc) {
                        output_path = argv[++index];
                } else {
                        usage(argv[0]);
                        return EXIT_FAILURE;
                }
        }
        if (!manifest_path || !output_path) {
                usage(argv[0]);
                return EXIT_FAILURE;
        }
        if (load_manifest(manifest_path, &entries, &entry_count,
                          volume_name) < 0)
                goto out;
        if (overlay_path &&
            (load_overlay(overlay_path, &overlay_entries, &overlay_count) < 0 ||
             merge_overlay(&entries, &entry_count, overlay_entries,
                           overlay_count) < 0)) {
                overlay_entries = NULL;
                goto out;
        }
        overlay_entries = NULL;
        temp_path = temporary_path(output_path);
        if (!temp_path) {
                errorf("cannot choose a temporary output path");
                goto out;
        }
        temp_fd = open(temp_path, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC |
                       O_NOFOLLOW, 0644);
        if (temp_fd < 0) {
                errorf("cannot create temporary image %s: %s", temp_path,
                       strerror(errno));
                goto out;
        }
        if (build_image(temp_fd, entries, entry_count, volume_name) < 0)
                goto out;
        if (close(temp_fd) < 0) {
                temp_fd = -1;
                errorf("cannot close temporary image: %s", strerror(errno));
                goto out;
        }
        temp_fd = -1;

        int equal = files_equal(output_path, temp_path);
        if (verify_only) {
                if (equal != 1) {
                        errorf("image does not match manifest: %s",
                               output_path);
                        goto out;
                }
                result = EXIT_SUCCESS;
                goto out;
        }
        if (equal == 1) {
                result = EXIT_SUCCESS;
                goto out;
        }
        if (rename(temp_path, output_path) < 0) {
                errorf("cannot atomically publish %s: %s", output_path,
                       strerror(errno));
                goto out;
        }
        free(temp_path);
        temp_path = NULL;
        result = EXIT_SUCCESS;
        if (sync_parent_directory(output_path) < 0)
                warningf("image is published but its directory could not be "
                         "synced: %s", strerror(errno));
out:
        if (temp_fd >= 0)
                close(temp_fd);
        if (temp_path) {
                unlink(temp_path);
                free(temp_path);
        }
        free_manifest(entries, entry_count);
        free_manifest(overlay_entries, overlay_count);
        return result;
}
