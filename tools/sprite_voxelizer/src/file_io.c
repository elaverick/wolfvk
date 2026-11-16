#include "file_io.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define WOLF_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define WOLF_MKDIR(path) mkdir((path), 0755)
#endif

FILE *wolf_open_file(const char *path, const char *mode)
{
    FILE *file = NULL;

#if defined(_MSC_VER)
    if (fopen_s(&file, path, mode) != 0) {
        return NULL;
    }
#else
    file = fopen(path, mode);
#endif
    return file;
}

bool wolf_read_entire_file(const char *path, uint8_t **data, size_t *size)
{
    FILE *file;
    long length;
    uint8_t *buffer;
    size_t bytes_read;

    *data = NULL;
    *size = 0u;
    file = wolf_open_file(path, "rb");
    if (file == NULL) {
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }

    /* Allocate at least one byte so an empty file still yields a valid pointer. */
    buffer = malloc((size_t)length + 1u);
    if (buffer == NULL) {
        fclose(file);
        return false;
    }
    bytes_read = fread(buffer, 1u, (size_t)length, file);
    if (fclose(file) != 0 || bytes_read != (size_t)length) {
        free(buffer);
        return false;
    }

    *data = buffer;
    *size = (size_t)length;
    return true;
}

bool wolf_write_entire_file(const char *path, const void *data, size_t size)
{
    FILE *file = wolf_open_file(path, "wb");
    size_t written;

    if (file == NULL) {
        return false;
    }
    written = size > 0u ? fwrite(data, 1u, size, file) : 0u;
    if (fclose(file) != 0) {
        return false;
    }
    return written == size;
}

bool wolf_make_directories(const char *path)
{
    char partial[1024];
    size_t length = strlen(path);

    if (length == 0u || length >= sizeof(partial)) {
        return false;
    }
    memcpy(partial, path, length + 1u);

    /*
     * Create each ancestor in turn. Drive prefixes such as "C:" fail with
     * EEXIST or EACCES and are skipped, so only the final result is checked.
     */
    for (size_t position = 1u; position <= length; ++position) {
        if (partial[position] == '/' || partial[position] == '\\' || partial[position] == '\0') {
            char saved = partial[position];

            partial[position] = '\0';
            if (WOLF_MKDIR(partial) != 0 && errno != EEXIST && position == length) {
                return false;
            }
            partial[position] = saved;
        }
    }
    return true;
}

void wolf_set_error(char *error_message, size_t error_message_size, const char *message)
{
    if (error_message != NULL && error_message_size > 0u) {
        (void)snprintf(error_message, error_message_size, "%s", message);
    }
}

/* Grow geometrically so assembling a large file stays linear overall. */
static bool reserve_buffer(WolfByteBuffer *buffer, size_t additional)
{
    size_t required;
    size_t new_capacity;
    uint8_t *grown;

    if (buffer->failed) {
        return false;
    }
    if (additional > SIZE_MAX - buffer->size) {
        buffer->failed = true;
        return false;
    }
    required = buffer->size + additional;
    if (required <= buffer->capacity) {
        return true;
    }
    new_capacity = buffer->capacity > 0u ? buffer->capacity : 4096u;
    while (new_capacity < required) {
        new_capacity *= 2u;
    }
    grown = realloc(buffer->data, new_capacity);
    if (grown == NULL) {
        buffer->failed = true;
        return false;
    }
    buffer->data = grown;
    buffer->capacity = new_capacity;
    return true;
}

void wolf_buffer_append(WolfByteBuffer *buffer, const void *data, size_t size)
{
    if (size == 0u || !reserve_buffer(buffer, size)) {
        return;
    }
    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;
}

void wolf_buffer_append_u8(WolfByteBuffer *buffer, uint8_t value)
{
    wolf_buffer_append(buffer, &value, 1u);
}

void wolf_buffer_append_u32_le(WolfByteBuffer *buffer, uint32_t value)
{
    uint8_t bytes[4];

    wolf_write_u32_le(bytes, value);
    wolf_buffer_append(buffer, bytes, sizeof(bytes));
}

void wolf_buffer_append_u32_be(WolfByteBuffer *buffer, uint32_t value)
{
    uint8_t bytes[4] = {
        (uint8_t)(value >> 24), (uint8_t)((value >> 16) & 0xffu),
        (uint8_t)((value >> 8) & 0xffu), (uint8_t)(value & 0xffu)
    };

    wolf_buffer_append(buffer, bytes, sizeof(bytes));
}

void wolf_buffer_patch_u32_le(WolfByteBuffer *buffer, size_t offset, uint32_t value)
{
    if (!buffer->failed && offset + 4u <= buffer->size) {
        wolf_write_u32_le(buffer->data + offset, value);
    }
}

void wolf_buffer_free(WolfByteBuffer *buffer)
{
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0u;
    buffer->capacity = 0u;
    buffer->failed = false;
}
