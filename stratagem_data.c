#include "stratagem_data.h"
#include <string.h>
#include <storage/storage.h>
#include <toolbox/stream/stream.h>
#include <toolbox/stream/file_stream.h>
#include <furi.h>

/* No built-in stratagems: list is loaded from stratagems.txt only. */
static const StratagemDef builtin_list[] = { { NULL, NULL, NULL, 0 } };
static const uint8_t builtin_count = 0;

static StratagemDef loaded_list[STRATAGEM_MAX_COUNT];
static StratagemDir loaded_seqs[STRATAGEM_MAX_COUNT][STRATAGEM_MAX_SEQ];
static char loaded_names[STRATAGEM_MAX_COUNT][STRATAGEM_NAME_SIZE];
static char loaded_categories[STRATAGEM_MAX_COUNT][STRATAGEM_CATEGORY_SIZE];

const StratagemDef* stratagem_list = builtin_list;
uint8_t stratagem_count = builtin_count;
int8_t stratagem_keybind[KEYBIND_KEY_COUNT];

const char* stratagem_key_names[KEYBIND_KEY_COUNT] = {
    "Up", "Down", "Right", "Left", "OK", "Back"
};

static void keybind_clear(void) {
    for(int i = 0; i < KEYBIND_KEY_COUNT; i++) stratagem_keybind[i] = -1;
}

static int key_name_to_index(const char* key) {
    if(strcmp(key, "UP") == 0) return 0;
    if(strcmp(key, "DOWN") == 0) return 1;
    if(strcmp(key, "RIGHT") == 0) return 2;
    if(strcmp(key, "LEFT") == 0) return 3;
    if(strcmp(key, "OK") == 0) return 4;
    if(strcmp(key, "BACK") == 0) return 5;
    return -1;
}

int stratagem_index_by_name(const char* name) {
    for(uint8_t i = 0; i < stratagem_count; i++) {
        if(strcmp(stratagem_list[i].name, name) == 0) return (int)i;
    }
    return -1;
}

static StratagemDir char_to_dir(char c) {
    switch((unsigned char)c) {
    case 'U': return StratDirUp;
    case 'D': return StratDirDown;
    case 'L': return StratDirLeft;
    case 'R': return StratDirRight;
    default:  return StratDirCount;
    }
}

static bool parse_seq(const char* s, StratagemDir* out, uint8_t max_len, uint8_t* len) {
    *len = 0;
    while(*s && *len < max_len) {
        StratagemDir d = char_to_dir(*s);
        if(d == StratDirCount) {
            s++;
            continue;
        }
        out[(*len)++] = d;
        s++;
        while(*s == ',' || *s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
    }
    return *len > 0;
}

#define STRATAGEM_FILE_PATH EXT_PATH("apps_data/helldivers_stratagem/stratagems.txt")
#define LINE_BUF_SIZE 256

void stratagem_load_from_file(const char* path) {
    stratagem_list = builtin_list;
    stratagem_count = builtin_count;
    keybind_clear();

    if(!path) path = STRATAGEM_FILE_PATH;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, EXT_PATH("apps_data"));
    storage_common_mkdir(storage, EXT_PATH("apps_data/helldivers_stratagem"));
    Stream* stream = file_stream_alloc(storage);
    if(!stream || !file_stream_open(stream, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(stream) {
            file_stream_close(stream);
            stream_free(stream);
        }
        furi_record_close(RECORD_STORAGE);
        return;
    }

    FuriString* line = furi_string_alloc();
    char buf[LINE_BUF_SIZE];
    char current_category[STRATAGEM_CATEGORY_SIZE];
    strncpy(current_category, "General", STRATAGEM_CATEGORY_SIZE - 1);
    current_category[STRATAGEM_CATEGORY_SIZE - 1] = '\0';
    uint8_t loaded = 0;

    while(loaded < STRATAGEM_MAX_COUNT && stream_read_line(stream, line)) {
        furi_string_trim(line);
        size_t n = furi_string_size(line);
        if(n == 0 || n >= LINE_BUF_SIZE) continue;
        memcpy(buf, furi_string_get_cstr(line), n + 1);
        buf[n] = '\0';
        for(size_t k = 0; k < n; k++)
            if(buf[k] == '\r' || buf[k] == '\n') buf[k] = '\0';
        char* s = buf;
        if(*s == '#') continue;

        if(strchr(s, '=') && !strchr(s, '|')) continue;

        if(strncmp(s, "Category:", 9) == 0) {
            s += 9;
            while(*s == ' ' || *s == '\t') s++;
            strncpy(current_category, s, STRATAGEM_CATEGORY_SIZE - 1);
            current_category[STRATAGEM_CATEGORY_SIZE - 1] = '\0';
            continue;
        }

        char* pipe = strchr(s, '|');
        if(!pipe) continue;
        *pipe = '\0';
        char* name = s;
        char* seq_str = pipe + 1;
        while(*name == ' ' || *name == '\t') name++;
        size_t name_len = strlen(name);
        if(name_len >= STRATAGEM_NAME_SIZE || name_len == 0) continue;

        uint8_t seq_len;
        if(!parse_seq(seq_str, loaded_seqs[loaded], STRATAGEM_MAX_SEQ, &seq_len)) continue;

        strncpy(loaded_names[loaded], name, STRATAGEM_NAME_SIZE - 1);
        loaded_names[loaded][STRATAGEM_NAME_SIZE - 1] = '\0';
        strncpy(loaded_categories[loaded], current_category, STRATAGEM_CATEGORY_SIZE - 1);
        loaded_categories[loaded][STRATAGEM_CATEGORY_SIZE - 1] = '\0';
        loaded_list[loaded].name = loaded_names[loaded];
        loaded_list[loaded].category = loaded_categories[loaded];
        loaded_list[loaded].sequence = loaded_seqs[loaded];
        loaded_list[loaded].length = seq_len;
        loaded++;
    }

    if(loaded > 0) {
        stratagem_list = loaded_list;
        stratagem_count = loaded;
    }

    stream_rewind(stream);
    while(stream_read_line(stream, line)) {
        furi_string_trim(line);
        size_t n = furi_string_size(line);
        if(n == 0 || n >= LINE_BUF_SIZE) continue;
        memcpy(buf, furi_string_get_cstr(line), n + 1);
        buf[n] = '\0';
        for(size_t k = 0; k < n; k++)
            if(buf[k] == '\r' || buf[k] == '\n') buf[k] = '\0';
        char* s = buf;
        if(*s == '#' || !strchr(s, '=')) continue;

        char* eq = strchr(s, '=');
        *eq = '\0';
        char* key_part = s;
        char* name_part = eq + 1;
        while(*name_part == ' ' || *name_part == '\t') name_part++;
        int ki = key_name_to_index(key_part);
        if(ki >= 0) {
            int idx = stratagem_index_by_name(name_part);
            if(idx >= 0) stratagem_keybind[ki] = (int8_t)idx;
        }
    }

    file_stream_close(stream);
    stream_free(stream);
    furi_string_free(line);
    furi_record_close(RECORD_STORAGE);
}
