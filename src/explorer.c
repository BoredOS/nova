// Copyright (c) 2026 Lluciocc (https://github.com/lluciocc)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

// BOREDOS_APP_DESC: Native NTK file manager for BoredOS
// BOREDOS_APP_ICONS: /Library/Icons/serenityicons/32x32/app-file-manager.png

#include <ntk.h>
#include <novaproto.h>
#include <syscall.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>

// CONSTANTS
#define FILE_MANAGER_PATH_MAX 1024
#define FILE_MANAGER_HISTORY_MAX 128
#define DIRECTORY_PAGE_SIZE 128
#define FILE_COPY_BUFFER_SIZE (64 * 1024)
#define FILE_OPERATION_MAX_DEPTH 128
#define ICON_CACHE_MAX 96

// hardcoded for now
#define SERENITY_16 "/Library/Icons/serenityicons/16x16/"
#define SERENITY_32 "/Library/Icons/serenityicons/32x32/"

// TYPES
typedef enum {
    VIEW_LARGE_ICONS,
    VIEW_SMALL_ICONS,
    VIEW_LIST,
    VIEW_DETAILS,
} FileViewMode;

typedef enum {
    CLIPBOARD_NONE,
    CLIPBOARD_COPY,
    CLIPBOARD_CUT,
} FileClipboardMode;

typedef enum {
    NAVIGATION_NORMAL,
    NAVIGATION_BACK,
    NAVIGATION_FORWARD,
    NAVIGATION_REFRESH,
} NavigationReason;

typedef struct {
    char *name;
    char *path;
    FAT32_FileInfo info;
    bool selected;
} FileEntry;

typedef struct {
    FileClipboardMode mode;
    char **paths;
    size_t count;
} FileClipboard;

typedef struct {
    char path[192];
    NtkPixmap *pixmap;
} IconCacheEntry;

typedef struct {
    NtkTreeNode *node;
    char *path;
    bool children_loaded;
} DirectoryTreeItem;

typedef struct FileManager FileManager;

typedef struct {
    FileManager *manager;
    uint64_t last_click_time;
    int last_click_index;
} FileViewData;

// main struct
struct FileManager {
    NtkApp *app;
    NtkWidget *window;
    NtkWidget *location_entry;
    NtkWidget *splitter;
    NtkWidget *directory_tree;
    NtkWidget *tree_scroll;
    NtkWidget *file_view;
    NtkWidget *file_scroll;
    NtkWidget *statusbar;

    NtkWidget *back_button;
    NtkWidget *forward_button;
    NtkWidget *up_button;
    NtkWidget *paste_button;

    NtkWidget *open_item;
    NtkWidget *rename_item;
    NtkWidget *delete_item;
    NtkWidget *properties_item;
    NtkWidget *paste_item;
    NtkWidget *show_hidden_item;
    NtkWidget *view_items[4];

    char current_path[FILE_MANAGER_PATH_MAX];
    char home_path[FILE_MANAGER_PATH_MAX];

    char *back_history[FILE_MANAGER_HISTORY_MAX];
    size_t back_count;
    char *forward_history[FILE_MANAGER_HISTORY_MAX];
    size_t forward_count;

    FileEntry *entries;
    size_t entry_count;
    FileClipboard clipboard;
    FileViewMode view_mode;
    bool show_hidden;
    bool archive_mode;
    char archive_path[FILE_MANAGER_PATH_MAX];
    char archive_inner_path[FILE_MANAGER_PATH_MAX];

    DirectoryTreeItem **tree_items;
    size_t tree_item_count;
    size_t tree_item_capacity;

    IconCacheEntry icon_cache[ICON_CACHE_MAX];
    size_t icon_cache_count;
};

static FileManager *g_file_manager;

// Function proto
static bool file_manager_navigate(FileManager *manager,
                                  const char *target,
                                  NavigationReason reason);
static void file_manager_refresh(FileManager *manager);
static void file_manager_navigate_up(FileManager *manager);
static void file_manager_open_selection(FileManager *manager);
static void file_manager_update_actions(FileManager *manager);
static void file_manager_update_status(FileManager *manager);
static void file_manager_show_context_menu(FileManager *manager,
                                           int global_x,
                                           int global_y,
                                           bool background);
static void action_copy(FileManager *manager, FileClipboardMode mode);
static void action_paste(FileManager *manager);
static void action_delete(FileManager *manager);
static void action_rename(FileManager *manager);
static void action_properties(FileManager *manager);
static void action_create_archive(FileManager *manager);
static void action_extract_archive(FileManager *manager);
static bool extension_matches(const char *path, const char *extension);

// Normalize a path by resolving "." and ".." components, and ensuring it is absolute
static bool path_normalize(char *output,
                           size_t output_size,
                           const char *current_directory,
                           const char *input) {
    if (!output || output_size < 2 || !input) return false;

    char combined[FILE_MANAGER_PATH_MAX];
    int length;
    if (input[0] == '/') {
        length = snprintf(combined, sizeof(combined), "%s", input);
    } else {
        const char *base = current_directory && current_directory[0]
                               ? current_directory
                               : "/";
        length = snprintf(combined, sizeof(combined), "%s%s%s", base,
                          strcmp(base, "/") == 0 ? "" : "/", input);
    }
    if (length < 0 || (size_t)length >= sizeof(combined)) return false;

    // Normalize the path by resolving "." and ".." components.
    char normalized[FILE_MANAGER_PATH_MAX] = "/";
    size_t normalized_length = 1;
    char *save_pointer = NULL;
    char *component = strtok_r(combined, "/", &save_pointer);

    while (component) {
        // handle .
        if (strcmp(component, ".") == 0) {
            component = strtok_r(NULL, "/", &save_pointer);
            continue;
        }

        // handle ..
        if (strcmp(component, "..") == 0) {
            if (normalized_length > 1) {
                while (normalized_length > 1 &&
                       normalized[normalized_length - 1] != '/') {
                    --normalized_length;
                }
                if (normalized_length > 1) --normalized_length;
                normalized[normalized_length] = '\0';
            }
            component = strtok_r(NULL, "/", &save_pointer);
            continue;
        }

        size_t component_length = strlen(component);
        size_t separator_length = normalized_length > 1 ? 1 : 0;
        if (normalized_length + separator_length + component_length >= output_size ||
            normalized_length + separator_length + component_length >=
                sizeof(normalized)) {
            return false;
        }

        // add the component to the normalized path
        if (separator_length) normalized[normalized_length++] = '/';
        memcpy(normalized + normalized_length, component, component_length + 1);
        normalized_length += component_length;
        component = strtok_r(NULL, "/", &save_pointer);
    }

    memcpy(output, normalized, normalized_length + 1);
    return true;
}

// join dir and name into output
static bool path_join(char *output,
                      size_t output_size,
                      const char *directory,
                      const char *name) {
    if (!output || !directory || !name || strchr(name, '/')) return false;
    int length = snprintf(output, output_size, "%s%s%s", directory,
                          strcmp(directory, "/") == 0 ? "" : "/", name);
    return length >= 0 && (size_t)length < output_size;
}

// get the parent
static bool path_parent(char *output, size_t output_size, const char *path) {
    char normalized[FILE_MANAGER_PATH_MAX];
    if (!path_normalize(normalized, sizeof(normalized), "/", path)) return false;
    if (strcmp(normalized, "/") == 0) {
        return snprintf(output, output_size, "/") == 1;
    }

    char *slash = strrchr(normalized, '/');
    if (slash == normalized) slash[1] = '\0';
    else *slash = '\0';
    int length = snprintf(output, output_size, "%s", normalized);
    return length >= 0 && (size_t)length < output_size;
}

// get basename
static const char *path_basename(const char *path) {
    if (!path || strcmp(path, "/") == 0) return "/";
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

// get the path is an ancestor of another path
static bool path_is_ancestor(const char *parent, const char *child) {
    if (strcmp(parent, "/") == 0) return child[0] == '/';
    size_t parent_length = strlen(parent);
    return strncmp(parent, child, parent_length) == 0 &&
           (child[parent_length] == '\0' || child[parent_length] == '/');
}

// check if the filename is valid... pretty clear lol
static bool filename_is_valid(const char *name) {
    return name && name[0] && strcmp(name, ".") != 0 &&
           strcmp(name, "..") != 0 && strchr(name, '/') == NULL;
}

// free the file entries
static void file_entries_free(FileEntry *entries, size_t count) {
    if (!entries) return;
    for (size_t index = 0; index < count; ++index) {
        free(entries[index].name);
        free(entries[index].path);
    }
    free(entries);
}

// compare two file entries for sorting
static int file_entry_compare(const void *left_pointer, const void *right_pointer) {
    const FileEntry *left = left_pointer;
    const FileEntry *right = right_pointer;

    if (left->info.is_directory != right->info.is_directory) {
        return right->info.is_directory - left->info.is_directory;
    }

    int insensitive = strcasecmp(left->name, right->name);
    return insensitive ? insensitive : strcmp(left->name, right->name);
}

// read the directory and return the entries
static bool directory_read(const char *path,
                           bool show_hidden,
                           FileEntry **entries_out,
                           size_t *count_out,
                           char *error,
                           size_t error_size) {
    FAT32_FileInfo *page = malloc(DIRECTORY_PAGE_SIZE * sizeof(*page));
    FileEntry *entries = NULL;
    size_t count = 0;
    size_t capacity = 0;

    if (!page) {
        snprintf(error, error_size, "Out of memory while reading the folder.");
        return false;
    }

    // sys_list_offset returns at most one VFS page at a time
    for (int offset = 0;;) {
        int page_count = sys_list_offset(path, page, DIRECTORY_PAGE_SIZE, offset);
        if (page_count < 0) {
            snprintf(error, error_size, "Could not read \"%s\".", path);
            free(page);
            file_entries_free(entries, count);
            return false;
        }
        if (page_count == 0) break;

        // add the entries from the current page to the list
        for (int page_index = 0; page_index < page_count; ++page_index) {
            const char *name = page[page_index].name;
            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
            if (!show_hidden && name[0] == '.') continue;

            if (count == capacity) {
                size_t new_capacity = capacity ? capacity * 2 : 64;
                FileEntry *larger = realloc(entries,
                                            new_capacity * sizeof(*larger));
                if (!larger) {
                    snprintf(error, error_size,
                             "Out of memory while reading the folder.");
                    free(page);
                    file_entries_free(entries, count);
                    return false;
                }
                entries = larger;
                capacity = new_capacity;
            }

            FileEntry *entry = &entries[count];
            memset(entry, 0, sizeof(*entry));
            entry->name = strdup(name);

            char absolute_path[FILE_MANAGER_PATH_MAX];
            if (!entry->name ||
                !path_join(absolute_path, sizeof(absolute_path), path, name)) {
                snprintf(error, error_size, "A file path is too long.");
                free(page);
                file_entries_free(entries, count + 1);
                return false;
            }

            entry->path = strdup(absolute_path);
            if (!entry->path) {
                snprintf(error, error_size,
                         "Out of memory while reading the folder.");
                free(page);
                file_entries_free(entries, count + 1);
                return false;
            }
            entry->info = page[page_index];
            ++count;
        }
        offset += page_count;
    }

    free(page);
    qsort(entries, count, sizeof(*entries), file_entry_compare);
    *entries_out = entries;
    *count_out = count;
    return true;
}

// This must stay byte-for-byte compatible with the on-disk ustar header
// See https://www.gnu.org/software/tar/manual/html_node/Standard.html
// Still not sure at 100% if this is the best way to do it LOL
typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type;
    char link_name[100];
    char magic[6];
    char version[2];
    char user_name[32];
    char group_name[32];
    char device_major[8];
    char device_minor[8];
    char prefix[155];
    char padding[12];
} __attribute__((packed)) TarHeader;

// parse the archive location into archive path and inner path
static bool archive_location_parse(const char *location,
                                   char *archive_path,
                                   size_t archive_size,
                                   char *inner_path,
                                   size_t inner_size) {
    // keep the archive.tar:/path form 
    const char *separator = strstr(location, ".tar:");
    if (!separator) return false;

    size_t archive_length = (size_t)(separator - location) + 4;
    if (archive_length >= archive_size) return false;
    memcpy(archive_path, location, archive_length);
    archive_path[archive_length] = '\0';

    const char *inner = separator + 5;
    while (*inner == '/') ++inner;
    int length = snprintf(inner_path, inner_size, "%s", inner);
    return length >= 0 && (size_t)length < inner_size;
}

// read exactly size bytes from the descriptor into the buffer
static bool descriptor_read_exact(int descriptor, void *buffer, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        int result = sys_read(descriptor, (char *)buffer + offset,
                              (uint32_t)(size - offset));
        if (result <= 0) return false;
        offset += (size_t)result;
    }
    return true;
}

// check if the tar header is empty
static bool tar_header_is_empty(const TarHeader *header) {
    const unsigned char *bytes = (const unsigned char *)header;
    for (size_t index = 0; index < sizeof(*header); ++index) {
        if (bytes[index] != 0) return false;
    }
    return true;
}

// convert an octal string to a uint64_t value
static uint64_t tar_octal_value(const char *field, size_t size) {
    uint64_t value = 0;
    for (size_t index = 0; index < size; ++index) {
        if (field[index] == '\0' || field[index] == ' ') break;
        if (field[index] >= '0' && field[index] <= '7') {
            value = (value << 3) + (uint64_t)(field[index] - '0');
        }
    }
    return value;
}

static void tar_header_path(const TarHeader *header,
                            char *output,
                            size_t output_size) {
    if (header->prefix[0]) {
        snprintf(output, output_size, "%.*s/%.*s", 155, header->prefix, 100,
                 header->name);
    } else {
        snprintf(output, output_size, "%.*s", 100, header->name);
    }

    size_t length = strlen(output);
    while (length > 0 && output[length - 1] == '/') output[--length] = '\0';
}

// add an entry to the archive entries list
static bool archive_entry_add(FileEntry **entries,
                              size_t *count,
                              size_t *capacity,
                              const char *archive_path,
                              const char *inner_path,
                              const char *name,
                              bool directory,
                              uint32_t size) {
    for (size_t index = 0; index < *count; ++index) {
        if (strcmp((*entries)[index].name, name) == 0) {
            // a later member may turn an implicit directory into an explicit one.
            if (directory) (*entries)[index].info.is_directory = 1;
            return true;
        }
    }

    if (*count == *capacity) {
        size_t new_capacity = *capacity ? *capacity * 2 : 32;
        FileEntry *larger = realloc(*entries, new_capacity * sizeof(*larger));
        if (!larger) return false;
        *entries = larger;
        *capacity = new_capacity;
    }

    FileEntry *entry = &(*entries)[*count];
    memset(entry, 0, sizeof(*entry));
    entry->name = strdup(name);

    char virtual_path[FILE_MANAGER_PATH_MAX];
    int length = snprintf(virtual_path, sizeof(virtual_path), "%s:/%s%s%s",
                          archive_path, inner_path,
                          inner_path[0] ? "/" : "", name);
    if (!entry->name || length < 0 || (size_t)length >= sizeof(virtual_path)) {
        free(entry->name);
        memset(entry, 0, sizeof(*entry));
        return false;
    }

    entry->path = strdup(virtual_path);
    if (!entry->path) {
        free(entry->name);
        memset(entry, 0, sizeof(*entry));
        return false;
    }
    snprintf(entry->info.name, sizeof(entry->info.name), "%s", name);
    entry->info.is_directory = directory;
    entry->info.size = size;
    ++*count;
    return true;
}

// read the contents of a directory inside a tar archive
static bool archive_read_directory(const char *archive_path,
                                   const char *inner_path,
                                   FileEntry **entries_out,
                                   size_t *count_out,
                                   char *error,
                                   size_t error_size) {
    int descriptor = sys_open(archive_path, "r");
    if (descriptor < 0) {
        snprintf(error, error_size, "Could not open \"%s\".", archive_path);
        return false;
    }

    char prefix[FILE_MANAGER_PATH_MAX];
    snprintf(prefix, sizeof(prefix), "%s%s", inner_path,
             inner_path[0] ? "/" : "");
    size_t prefix_length = strlen(prefix);
    FileEntry *entries = NULL;
    size_t count = 0;
    size_t capacity = 0;
    bool success = true;

    for (;;) {
        TarHeader header;
        if (!descriptor_read_exact(descriptor, &header, sizeof(header)) ||
            tar_header_is_empty(&header)) {
            break;
        }

        uint64_t size = tar_octal_value(header.size, sizeof(header.size));
        char full_path[FILE_MANAGER_PATH_MAX];
        tar_header_path(&header, full_path, sizeof(full_path));

        if (strncmp(full_path, prefix, prefix_length) == 0) {
            const char *remaining = full_path + prefix_length;
            if (remaining[0]) {
                // Only expose the first component below the current archive path.
                const char *slash = strchr(remaining, '/');
                size_t name_length = slash ? (size_t)(slash - remaining)
                                           : strlen(remaining);
                char name[256];
                if (name_length < sizeof(name)) {
                    memcpy(name, remaining, name_length);
                    name[name_length] = '\0';
                    bool directory = slash != NULL || header.type == '5';
                    if (!archive_entry_add(&entries, &count, &capacity,
                                           archive_path, inner_path, name,
                                           directory, (uint32_t)size)) {
                        snprintf(error, error_size,
                                 "Out of memory while reading the archive.");
                        success = false;
                        break;
                    }
                }
            }
        }

        // TAR payloads are rounded up to a full 512-byte block
        uint64_t padded_size = ((size + 511) / 512) * 512;
        if (padded_size > 0 && sys_seek(descriptor, (int)padded_size, 1) != 0) {
            snprintf(error, error_size, "The archive is damaged or too large.");
            success = false;
            break;
        }
    }

    sys_close(descriptor);
    if (!success) {
        file_entries_free(entries, count);
        return false;
    }

    qsort(entries, count, sizeof(*entries), file_entry_compare);
    *entries_out = entries;
    *count_out = count;
    return true;
}


// format
static void format_size(uint64_t bytes, char *output, size_t output_size) {
    static const char *units[] = {"B", "KiB", "MiB", "GiB"};
    unsigned unit = 0;
    uint64_t whole = bytes;
    uint64_t decimal = 0;

    while (whole >= 1024 && unit < 3) {
        decimal = (whole % 1024) * 10 / 1024;
        whole /= 1024;
        ++unit;
    }

    if (unit > 0 && whole < 10) {
        snprintf(output, output_size, "%llu.%llu %s",
                 (unsigned long long)whole,
                 (unsigned long long)decimal,
                 units[unit]);
    } else {
        snprintf(output, output_size, "%llu %s",
                 (unsigned long long)whole, units[unit]);
    }
}

// date
static void format_fat_date(uint16_t date,
                            uint16_t time,
                            char *output,
                            size_t output_size) {
    if (!date) {
        snprintf(output, output_size, "-");
        return;
    }

    snprintf(output, output_size, "%04u-%02u-%02u %02u:%02u",
             1980 + ((date >> 9) & 127),
             (date >> 5) & 15,
             date & 31,
             (time >> 11) & 31,
             (time >> 5) & 63);
}

typedef struct {
    const char *extension;
    const char *icon_name;
} ExtensionIcon;

// All those icons are from serenity-icons... found them in /Library/Icons/serenityicons/32x32 (or 16x16)
static const ExtensionIcon extension_icons[] = {
    {"c", "filetype-c.png"}, {"h", "filetype-header.png"},
    {"hh", "filetype-header.png"}, {"hpp", "filetype-header.png"},
    {"hxx", "filetype-header.png"}, {"cpp", "filetype-cplusplus.png"},
    {"cc", "filetype-cplusplus.png"}, {"cxx", "filetype-cplusplus.png"},
    {"s", "filetype-asm.png"}, {"asm", "filetype-asm.png"},
    {"py", "filetype-python.png"}, {"sh", "filetype-shell.png"},
    {"bash", "filetype-shell.png"}, {"js", "filetype-javascript.png"},
    {"mjs", "filetype-javascript.png"}, {"json", "filetype-json.png"},
    {"html", "filetype-html.png"}, {"htm", "filetype-html.png"},
    {"css", "filetype-css.png"}, {"md", "filetype-markdown.png"},
    {"markdown", "filetype-markdown.png"}, {"pdf", "filetype-pdf.png"},
    {"zip", "filetype-archive.png"}, {"tar", "filetype-archive.png"},
    {"gz", "filetype-archive.png"}, {"tgz", "filetype-archive.png"},
    {"bz2", "filetype-archive.png"}, {"xz", "filetype-archive.png"},
    {"lz4", "filetype-archive.png"}, {"bup", "filetype-archive.png"},
    {"png", "filetype-image.png"}, {"jpg", "filetype-image.png"},
    {"jpeg", "filetype-image.png"}, {"gif", "filetype-image.png"},
    {"bmp", "filetype-image.png"}, {"webp", "filetype-image.png"},
    {"mp3", "filetype-music.png"}, {"ogg", "filetype-music.png"},
    {"flac", "filetype-music.png"}, {"wav", "filetype-sound.png"},
    {"aiff", "filetype-sound.png"}, {"mp4", "filetype-video.png"},
    {"mkv", "filetype-video.png"}, {"avi", "filetype-video.png"},
    {"webm", "filetype-video.png"}, {"sql", "filetype-sql.png"},
    {"db", "filetype-db.png"}, {"sqlite", "filetype-db.png"},
    {"sqlite3", "filetype-db.png"}, {"so", "filetype-library.png"},
    {"a", "filetype-library.png"}, {"o", "filetype-object.png"},
    {"obj", "filetype-object.png"}, {"elf", "filetype-executable.png"},
    {"wasm", "filetype-wasm.png"}, {"ttf", "filetype-truetype.png"},
    {"otf", "filetype-truetype.png"}, {"ini", "filetype-ini.png"},
    {"conf", "filetype-ini.png"}, {"cfg", "filetype-ini.png"},
    {"java", "filetype-java.png"}, {"php", "filetype-php.png"},
    {"rb", "filetype-ruby.png"}, {"cmake", "filetype-cmake.png"},
    {"gml", "filetype-gml.png"}, {"csv", "filetype-spreadsheet.png"},
    {"ods", "filetype-spreadsheet.png"}, {"ics", "filetype-calendar.png"},
};

// get the icon name for a file based on its path and info
static const char *file_icon_name(FileManager *manager,
                                  const char *absolute_path,
                                  const FAT32_FileInfo *info,
                                  bool expanded) {
    const char *name = path_basename(absolute_path);

    if (info->is_directory) {
        if (strcmp(absolute_path, "/") == 0) return "hard-disk.png";
        if (strcmp(absolute_path, manager->home_path) == 0) {
            return expanded ? "home-directory-open.png" : "home-directory.png";
        }
        if (strcasecmp(name, "Desktop") == 0) return "desktop.png";
        if (strcasecmp(name, "Downloads") == 0) return "downloads.png";

        char git_path[FILE_MANAGER_PATH_MAX];
        if (path_join(git_path, sizeof(git_path), absolute_path, ".git") &&
            sys_exists(git_path)) {
            return expanded ? "git-directory-open.png" : "git-directory.png";
        }
        return expanded ? "filetype-folder-open.png" : "filetype-folder.png";
    }

    if (strcasecmp(name, "CMakeLists.txt") == 0) return "filetype-cmake.png";
    if (strcasecmp(name, ".gitignore") == 0 ||
        strcasecmp(name, ".gitattributes") == 0) {
        return "filetype-git.png";
    }

    const char *dot = strrchr(name, '.');
    if (dot && dot[1]) {
        for (size_t index = 0;
             index < sizeof(extension_icons) / sizeof(extension_icons[0]);
             ++index) {
            if (strcasecmp(dot + 1, extension_icons[index].extension) == 0) {
                return extension_icons[index].icon_name;
            }
        }
    }
    return "filetype-unknown.png";
}

// load an icon from the cache or from disk
static NtkPixmap *icon_cache_load(FileManager *manager, const char *path) {
    for (size_t index = 0; index < manager->icon_cache_count; ++index) {
        if (strcmp(manager->icon_cache[index].path, path) == 0) {
            return manager->icon_cache[index].pixmap;
        }
    }

    NtkPixmap *pixmap = ntk_pixmap_new_from_file(path);
    if (!pixmap && strstr(path, "filetype-unknown.png") == NULL) {
        // Not every Serenity set ships every file type icon.
        pixmap = ntk_pixmap_new_from_file(
            SERENITY_32 "filetype-unknown.png");
    }
    if (!pixmap || manager->icon_cache_count >= ICON_CACHE_MAX) return pixmap;

    IconCacheEntry *cache_entry =
        &manager->icon_cache[manager->icon_cache_count++];
    snprintf(cache_entry->path, sizeof(cache_entry->path), "%s", path);
    cache_entry->pixmap = pixmap;
    return pixmap;
}

// get the icon for a file based on its path and info
static NtkPixmap *file_icon(FileManager *manager,
                            const char *path,
                            const FAT32_FileInfo *info,
                            bool expanded,
                            int size) {
    char icon_path[192];
    snprintf(icon_path, sizeof(icon_path), "%s%s",
             size == 16 ? SERENITY_16 : SERENITY_32,
             file_icon_name(manager, path, info, expanded));
    return icon_cache_load(manager, icon_path);
}

static NtkPixmap *toolbar_icon(FileManager *manager, const char *name) {
    char path[192];
    snprintf(path, sizeof(path), SERENITY_16 "%s.png", name);
    NtkPixmap *cached = icon_cache_load(manager, path);
    // Tool buttons own their pixmaps; the cache keeps the original alive.
    return cached ? ntk_pixmap_clone(cached) : NULL;
}

static int file_view_column_count(NtkWidget *widget, FileViewMode mode) {
    int width = ntk_widget_get_geometry(widget).width;
    int item_width = mode == VIEW_LARGE_ICONS ? 96 : 180;
    int columns = (width - 8) / item_width;
    return columns > 0 ? columns : 1;
}

static NtkRect file_view_item_rect(NtkWidget *widget, int index) {
    FileViewData *view = ntk_widget_get_instance_data(widget);
    FileViewMode mode = view->manager->view_mode;

    if (mode == VIEW_DETAILS || mode == VIEW_LIST) {
        int top = mode == VIEW_DETAILS ? 22 : 4;
        return NTK_RECT(2, top + index * 22,
                        ntk_widget_get_geometry(widget).width - 4, 22);
    }

    int item_width = mode == VIEW_LARGE_ICONS ? 96 : 180;
    int item_height = mode == VIEW_LARGE_ICONS ? 78 : 30;
    int columns = file_view_column_count(widget, mode);
    return NTK_RECT(4 + (index % columns) * item_width,
                    4 + (index / columns) * item_height,
                    item_width - 4, item_height - 4);
}

static int file_view_hit_test(NtkWidget *widget, NtkPoint point) {
    FileViewData *view = ntk_widget_get_instance_data(widget);
    for (size_t index = 0; index < view->manager->entry_count; ++index) {
        NtkRect rectangle = file_view_item_rect(widget, (int)index);
        if (point.x >= rectangle.x && point.x < rectangle.x + rectangle.width &&
            point.y >= rectangle.y && point.y < rectangle.y + rectangle.height) {
            return (int)index;
        }
    }
    return -1;
}

static void file_view_clear_selection(FileManager *manager) {
    for (size_t index = 0; index < manager->entry_count; ++index) {
        manager->entries[index].selected = false;
    }
}

static int file_view_first_selection(FileManager *manager) {
    for (size_t index = 0; index < manager->entry_count; ++index) {
        if (manager->entries[index].selected) return (int)index;
    }
    return -1;
}

static size_t file_view_selection_count(FileManager *manager) {
    size_t count = 0;
    for (size_t index = 0; index < manager->entry_count; ++index) {
        if (manager->entries[index].selected) ++count;
    }
    return count;
}

static void file_view_draw_header(NtkWidget *widget, NtkPainter *painter) {
    NtkStyle *style = ntk_widget_get_style(widget);
    NtkPoint origin = ntk_widget_map_to_global(widget, NTK_POINT(0, 0));
    int widths[] = {280, 120, 100, 160};
    const char *labels[] = {"Name", "Type", "Size", "Modified"};
    int x = origin.x + 2;

    for (int column = 0; column < 4; ++column) {
        NtkRect cell = NTK_RECT(x, origin.y + 2, widths[column], 20);
        ntk_painter_set_color(painter,
                              ntk_style_get_color(style, NTK_STYLE_ROLE_PANEL_BG));
        ntk_painter_fill_rect(painter, cell);
        ntk_painter_draw_bevel_raised(
            painter, cell,
            ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT),
            ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK));
        ntk_painter_set_color(painter,
                              ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_PRIMARY));
        ntk_painter_draw_text(painter, labels[column], x + 4, origin.y + 4);
        x += widths[column];
    }
}

static void file_view_paint(NtkWidget *widget, NtkPainter *painter) {
    FileViewData *view = ntk_widget_get_instance_data(widget);
    FileManager *manager = view->manager;
    NtkStyle *style = ntk_widget_get_style(widget);
    NtkRect geometry = ntk_widget_get_geometry(widget);
    NtkPoint origin = ntk_widget_map_to_global(widget, NTK_POINT(0, 0));
    NtkRect bounds = NTK_RECT(origin.x, origin.y, geometry.width, geometry.height);

    ntk_painter_set_color(painter,
                          ntk_style_get_color(style, NTK_STYLE_ROLE_WIDGET_BG));
    ntk_painter_fill_rect(painter, bounds);
    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
    ntk_painter_set_font(painter, font);

    if (manager->view_mode == VIEW_DETAILS) {
        file_view_draw_header(widget, painter);
    }

    for (size_t index = 0; index < manager->entry_count; ++index) {
        FileEntry *entry = &manager->entries[index];
        NtkRect local = file_view_item_rect(widget, (int)index);
        NtkRect item = NTK_RECT(origin.x + local.x, origin.y + local.y,
                                local.width, local.height);

        if (entry->selected) {
            ntk_painter_set_color(
                painter, ntk_style_get_color(style, NTK_STYLE_ROLE_SELECTION_BG));
            ntk_painter_fill_rect(painter, item);
            ntk_painter_set_color(
                painter, ntk_style_get_color(style, NTK_STYLE_ROLE_SELECTION_TEXT));
        } else {
            ntk_painter_set_color(
                painter, ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_PRIMARY));
        }

        if (manager->view_mode == VIEW_LARGE_ICONS) {
            NtkPixmap *icon = file_icon(manager, entry->path, &entry->info, false, 32);
            if (icon) ntk_painter_draw_pixmap(painter, icon, item.x + 30, item.y + 4);
            NtkSize text_size = ntk_font_measure_text(font, entry->name);
            int text_x = item.x + (item.width - text_size.width) / 2;
            if (text_x < item.x + 2) text_x = item.x + 2;
            ntk_painter_draw_text(painter, entry->name, text_x, item.y + 42);
        } else {
            NtkPixmap *icon = file_icon(manager, entry->path, &entry->info, false, 16);
            if (icon) ntk_painter_draw_pixmap(painter, icon, item.x + 4, item.y + 3);
            ntk_painter_draw_text(painter, entry->name, item.x + 24, item.y + 3);

            if (manager->view_mode == VIEW_DETAILS) {
                char size_text[32];
                char date_text[32];
                format_size(entry->info.size, size_text, sizeof(size_text));
                format_fat_date(entry->info.write_date, entry->info.write_time,
                                date_text, sizeof(date_text));
                ntk_painter_draw_text(painter,
                                      entry->info.is_directory ? "Folder" : "File",
                                      item.x + 284, item.y + 3);
                ntk_painter_draw_text(painter,
                                      entry->info.is_directory ? "-" : size_text,
                                      item.x + 404, item.y + 3);
                ntk_painter_draw_text(painter, date_text,
                                      item.x + 504, item.y + 3);
            }
        }
    }

    if (manager->entry_count == 0) {
        ntk_painter_set_color(painter,
                              ntk_style_get_color(style, NTK_STYLE_ROLE_TEXT_SECONDARY));
        ntk_painter_draw_text(painter, "This folder is empty.", origin.x + 12,
                              origin.y + 12);
    }
}

// calculate the preferred size of the file view widget
static NtkSize file_view_preferred_size(NtkWidget *widget) {
    FileViewData *view = ntk_widget_get_instance_data(widget);
    FileManager *manager = view->manager;
    int width = ntk_widget_get_geometry(widget).width;
    if (width < 560) width = 560;

    if (manager->view_mode == VIEW_DETAILS || manager->view_mode == VIEW_LIST) {
        int top = manager->view_mode == VIEW_DETAILS ? 22 : 4;
        return NTK_SIZE(width, top + (int)manager->entry_count * 22 + 4);
    }

    int columns = file_view_column_count(widget, manager->view_mode);
    int item_height = manager->view_mode == VIEW_LARGE_ICONS ? 78 : 30;
    int rows = ((int)manager->entry_count + columns - 1) / columns;
    return NTK_SIZE(width, rows * item_height + 8);
}

// get the time loool
static uint64_t current_time_milliseconds(void) {
    struct timeval time;
    gettimeofday(&time, NULL);
    return (uint64_t)time.tv_sec * 1000 + (uint64_t)time.tv_usec / 1000;
}

static bool file_view_handle_event(NtkWidget *widget, NtkEvent *event) {
    FileViewData *view = ntk_widget_get_instance_data(widget);
    FileManager *manager = view->manager;

    if (event->type == NTK_EVENT_MOUSE_PRESS) {
        int index = file_view_hit_test(widget, event->mouse_pos);

        if (event->mouse_button == NTK_MOUSE_BUTTON_RIGHT) {
            if (index >= 0 && !manager->entries[index].selected) {
                file_view_clear_selection(manager);
                manager->entries[index].selected = true;
            }
            file_manager_update_status(manager);
            file_manager_update_actions(manager);
            ntk_widget_repaint(widget);
            file_manager_show_context_menu(manager, event->global_pos.x,
                                           event->global_pos.y, index < 0);
            return true;
        }

        if (event->mouse_button != NTK_MOUSE_BUTTON_LEFT) return false;
        ntk_widget_set_focus(widget);

        if (index < 0) {
            file_view_clear_selection(manager);
        } else if (event->modifiers & NTK_MOD_CTRL) {
            manager->entries[index].selected = !manager->entries[index].selected;
        } else {
            file_view_clear_selection(manager);
            manager->entries[index].selected = true;
        }

        // NTK has no double-click event, so keep a small per-view click window
        uint64_t now = current_time_milliseconds();
        bool double_click = index >= 0 && index == view->last_click_index &&
                            now - view->last_click_time <= 450;
        view->last_click_index = index;
        view->last_click_time = now;

        file_manager_update_status(manager);
        file_manager_update_actions(manager);
        ntk_widget_repaint(widget);
        if (double_click) file_manager_open_selection(manager);
        return true;
    }

    if (event->type != NTK_EVENT_KEY_PRESS) return false;
    bool control = (event->modifiers & NTK_MOD_CTRL) != 0;
    bool alt = (event->modifiers & NTK_MOD_ALT) != 0;

    if (event->key_code == KEY_ENTER) {
        file_manager_open_selection(manager);
    } else if (event->key_code == KEY_DELETE) { // Delete smth
        action_delete(manager);
    } else if (event->key_code == KEY_F2) { // rename
        action_rename(manager);
    } else if (event->key_code == KEY_F5) { // refresh
        file_manager_refresh(manager);
    } else if (control && event->key_code == KEY_C) { // copy
        action_copy(manager, CLIPBOARD_COPY);
    } else if (control && event->key_code == KEY_X) { //cut
        action_copy(manager, CLIPBOARD_CUT);
    } else if (control && event->key_code == KEY_V) { //paste
        action_paste(manager);
    } else if (control && event->key_code == KEY_A) { // select all
        for (size_t index = 0; index < manager->entry_count; ++index) {
            manager->entries[index].selected = true;
        }
        file_manager_update_status(manager);
        file_manager_update_actions(manager);
        ntk_widget_repaint(widget);
    } else if (control && event->key_code == KEY_L) { // focus location bar
        ntk_widget_set_focus(manager->location_entry);
    } else if (alt && event->key_code == KEY_LEFT) { // back
        if (manager->back_count > 0) {
            file_manager_navigate(manager,
                                  manager->back_history[manager->back_count - 1],
                                  NAVIGATION_BACK);
        }
    } else if (alt && event->key_code == KEY_RIGHT) { // forward
        if (manager->forward_count > 0) {
            file_manager_navigate(
                manager, manager->forward_history[manager->forward_count - 1],
                NAVIGATION_FORWARD);
        }
    } else if (event->key_code == KEY_BACKSPACE || (alt && event->key_code == KEY_UP)) { // up
        file_manager_navigate_up(manager);
    } else {
        return false;
    }
    return true;
}

// Native icon and table views stop at 32 items, so the manager paints its own.
static const NtkWidgetClass file_view_class = {
    .type_name = "BoredOSFileView",
    .paint = file_view_paint,
    .layout = NULL,
    .preferred_size = file_view_preferred_size,
    .handle_event = file_view_handle_event,
    .destroy = NULL,
};

static NtkWidget *file_view_new(FileManager *manager, NtkWidget *parent) {
    NtkWidget *widget = ntk_widget_new_with_class(parent, &file_view_class,
                                                   sizeof(FileViewData));
    if (!widget) return NULL;

    FileViewData *view = ntk_widget_get_instance_data(widget);
    view->manager = manager;
    view->last_click_index = -1;
    return widget;
}

static bool copy_file(const char *source,
                      const char *destination,
                      char *error,
                      size_t error_size) {
    int source_fd = -1;
    int destination_fd = -1;
    char *buffer = NULL;
    bool success = false;

    source_fd = sys_open(source, "r");
    if (source_fd < 0) {
        snprintf(error, error_size, "Could not open \"%s\".", source);
        goto cleanup;
    }

    destination_fd = sys_open(destination, "w");
    if (destination_fd < 0) {
        snprintf(error, error_size, "Could not create \"%s\".", destination);
        goto cleanup;
    }

    buffer = malloc(FILE_COPY_BUFFER_SIZE);
    if (!buffer) {
        snprintf(error, error_size, "Out of memory while copying the file.");
        goto cleanup;
    }

    for (;;) {
        int bytes_read = sys_read(source_fd, buffer, FILE_COPY_BUFFER_SIZE);
        if (bytes_read < 0) {
            snprintf(error, error_size, "Could not read \"%s\".", source);
            goto cleanup;
        }
        if (bytes_read == 0) break;

        // Filesystem writes may accept less than the requested buffer.
        int written = 0;
        while (written < bytes_read) {
            int result = sys_write_fs(destination_fd, buffer + written,
                                      (uint32_t)(bytes_read - written));
            if (result <= 0) {
                snprintf(error, error_size, "Could not write \"%s\".",
                         destination);
                goto cleanup;
            }
            written += result;
        }
        ntk_app_draw_windows();
    }

    success = true;

cleanup:
    if (source_fd >= 0) sys_close(source_fd);
    if (destination_fd >= 0) sys_close(destination_fd);
    free(buffer);
    // A failed copy must never leave a destination that looks complete.
    if (!success && sys_exists(destination)) sys_delete(destination);
    return success;
}

static bool delete_path_recursive(const char *path,
                                  unsigned depth,
                                  char *error,
                                  size_t error_size);

static bool copy_path_recursive(const char *source,
                                const char *destination,
                                unsigned depth,
                                char *error,
                                size_t error_size) {
    if (depth > FILE_OPERATION_MAX_DEPTH) {
        snprintf(error, error_size, "Folder nesting is too deep.");
        return false;
    }

    FAT32_FileInfo source_info;
    if (sys_get_file_info(source, &source_info) != 0) {
        snprintf(error, error_size, "The source no longer exists: \"%s\".",
                 source);
        return false;
    }

    if (!source_info.is_directory) {
        return copy_file(source, destination, error, error_size);
    }

    // Recursing into the folder currently being copied would never terminate.
    if (path_is_ancestor(source, destination)) {
        snprintf(error, error_size,
                 "The destination is inside the source folder.");
        return false;
    }

    if (sys_mkdir(destination) != 0) {
        snprintf(error, error_size, "Could not create \"%s\".", destination);
        return false;
    }

    FAT32_FileInfo *page = malloc(DIRECTORY_PAGE_SIZE * sizeof(*page));
    if (!page) {
        snprintf(error, error_size, "Out of memory while copying the folder.");
        sys_delete(destination);
        return false;
    }

    bool success = true;
    for (int offset = 0; success;) {
        int page_count =
            sys_list_offset(source, page, DIRECTORY_PAGE_SIZE, offset);
        if (page_count < 0) {
            snprintf(error, error_size, "Could not read \"%s\".", source);
            success = false;
            break;
        }
        if (page_count == 0) break;

        for (int index = 0; index < page_count; ++index) {
            if (strcmp(page[index].name, ".") == 0 ||
                strcmp(page[index].name, "..") == 0) {
                continue;
            }

            char child_source[FILE_MANAGER_PATH_MAX];
            char child_destination[FILE_MANAGER_PATH_MAX];
            if (!path_join(child_source, sizeof(child_source), source,
                           page[index].name) ||
                !path_join(child_destination, sizeof(child_destination),
                           destination, page[index].name) ||
                !copy_path_recursive(child_source, child_destination, depth + 1,
                                     error, error_size)) {
                success = false;
                break;
            }
        }
        offset += page_count;
        ntk_app_draw_windows();
    }

    free(page);
    if (!success) {
        // Roll back the subtree built before the first failure.
        char cleanup_error[256];
        delete_path_recursive(destination, depth, cleanup_error,
                              sizeof(cleanup_error));
    }
    return success;
}

static bool delete_path_recursive(const char *path,
                                  unsigned depth,
                                  char *error,
                                  size_t error_size) {
    if (depth > FILE_OPERATION_MAX_DEPTH) {
        snprintf(error, error_size, "Folder nesting is too deep.");
        return false;
    }

    FAT32_FileInfo info;
    if (sys_get_file_info(path, &info) != 0) {
        snprintf(error, error_size, "Could not inspect \"%s\".", path);
        return false;
    }

    if (info.is_directory) {
        FAT32_FileInfo *page = malloc(DIRECTORY_PAGE_SIZE * sizeof(*page));
        if (!page) {
            snprintf(error, error_size,
                     "Out of memory while deleting the folder.");
            return false;
        }

        for (;;) {
            int page_count =
                sys_list_offset(path, page, DIRECTORY_PAGE_SIZE, 0);
            if (page_count < 0) {
                snprintf(error, error_size, "Could not read \"%s\".", path);
                free(page);
                return false;
            }
            if (page_count == 0) break;

            // Removing an entry shifts the next page, so read page zero again.
            for (int index = 0; index < page_count; ++index) {
                if (strcmp(page[index].name, ".") == 0 ||
                    strcmp(page[index].name, "..") == 0) {
                    continue;
                }
                char child[FILE_MANAGER_PATH_MAX];
                if (!path_join(child, sizeof(child), path, page[index].name) ||
                    !delete_path_recursive(child, depth + 1, error, error_size)) {
                    free(page);
                    return false;
                }
            }
            ntk_app_draw_windows();
        }
        free(page);
    }

    if (sys_delete(path) != 0) {
        snprintf(error, error_size, "Could not delete \"%s\".", path);
        return false;
    }
    return true;
}

// Check if a path is protected from deletion 
static bool protected_path(const FileManager *manager, const char *path) {
    // These are removable, but an accidental click should require confirmation.
    static const char *protected_paths[] = {
        "/bin", "/boot", "/dev", "/etc", "/Library", "/usr",
    };

    for (size_t index = 0;
         index < sizeof(protected_paths) / sizeof(protected_paths[0]); ++index) {
        if (strcmp(path, protected_paths[index]) == 0) return true;
    }
    return strcmp(manager->home_path, "/") != 0 &&
           strcmp(path, manager->home_path) == 0;
}

// clear the clip
static void clipboard_clear(FileClipboard *clipboard) {
    for (size_t index = 0; index < clipboard->count; ++index) {
        free(clipboard->paths[index]);
    }
    free(clipboard->paths);
    memset(clipboard, 0, sizeof(*clipboard));
}

// copy the selected files to the clipboard, CAN ALSO CUT
static void action_copy(FileManager *manager, FileClipboardMode mode) {
    if (manager->archive_mode) return;
    size_t selected_count = file_view_selection_count(manager);
    if (selected_count == 0) return;

    char **paths = calloc(selected_count, sizeof(*paths));
    if (!paths) {
        ntk_dialog_error(manager->window, "File Manager", "Out of memory.");
        return;
    }

    size_t output_index = 0;
    for (size_t index = 0; index < manager->entry_count; ++index) {
        if (!manager->entries[index].selected) continue;
        paths[output_index] = strdup(manager->entries[index].path);
        if (!paths[output_index]) {
            for (size_t cleanup = 0; cleanup < output_index; ++cleanup) {
                free(paths[cleanup]);
            }
            free(paths);
            ntk_dialog_error(manager->window, "File Manager", "Out of memory.");
            return;
        }
        ++output_index;
    }

    clipboard_clear(&manager->clipboard);
    manager->clipboard.paths = paths;
    manager->clipboard.count = selected_count;
    manager->clipboard.mode = mode;

    // Nova's text clipboard can mirror one path; multi-file state stays local.
    if (selected_count == 1) ntk_app_set_clipboard(paths[0]);
    file_manager_update_actions(manager);
}


static void action_paste(FileManager *manager) {
    if (manager->archive_mode) return;
    if (manager->clipboard.mode == CLIPBOARD_NONE) return;

    char error[512] = {0};
    bool complete_success = true;
    for (size_t index = 0; index < manager->clipboard.count; ++index) {
        const char *source = manager->clipboard.paths[index];
        if (!sys_exists(source)) {
            snprintf(error, sizeof(error), "The source no longer exists: \"%s\".",
                     source);
            complete_success = false;
            break;
        }

        char destination[FILE_MANAGER_PATH_MAX];
        if (!path_join(destination, sizeof(destination), manager->current_path,
                       path_basename(source))) {
            snprintf(error, sizeof(error), "The destination path is too long.");
            complete_success = false;
            break;
        }

        if (strcmp(source, destination) == 0) {
            snprintf(error, sizeof(error),
                     "Source and destination are the same item.");
            complete_success = false;
            break;
        }

        if (sys_exists(destination)) {
            FAT32_FileInfo source_info;
            FAT32_FileInfo destination_info;
            // Directory replacement would silently become a merge operation.
            if (sys_get_file_info(source, &source_info) != 0 ||
                sys_get_file_info(destination, &destination_info) != 0 ||
                source_info.is_directory || destination_info.is_directory) {
                snprintf(error, sizeof(error),
                         "A folder named \"%s\" already exists. Folders are not merged.",
                         path_basename(destination));
                complete_success = false;
                break;
            }

            char question[512];
            snprintf(question, sizeof(question),
                     "Replace the existing file \"%s\"?",
                     path_basename(destination));
            if (!ntk_dialog_question(manager->window, "Replace File", question)) {
                continue;
            }
            if (sys_delete(destination) != 0) {
                snprintf(error, sizeof(error), "Could not replace \"%s\".",
                         destination);
                complete_success = false;
                break;
            }
        }

        if (!copy_path_recursive(source, destination, 0, error, sizeof(error))) {
            complete_success = false;
            break;
        }

        // A cut is copy-then-delete because the VFS has no move primitive.
        if (manager->clipboard.mode == CLIPBOARD_CUT &&
            !delete_path_recursive(source, 0, error, sizeof(error))) {
            snprintf(error, sizeof(error),
                     "The copy succeeded, but the source could not be deleted.");
            complete_success = false;
            break;
        }
    }

    if (complete_success && manager->clipboard.mode == CLIPBOARD_CUT) {
        clipboard_clear(&manager->clipboard);
    }
    if (!complete_success) {
        ntk_dialog_error(manager->window, "Paste Failed", error);
    }
    file_manager_refresh(manager);
}

// delete the selected files and folders
static void action_delete(FileManager *manager) {
    if (manager->archive_mode) return;
    size_t count = file_view_selection_count(manager);
    if (count == 0) return;

    char question[512];
    if (count == 1) {
        int selected = file_view_first_selection(manager);
        snprintf(question, sizeof(question), "Delete \"%s\" permanently?",
                 manager->entries[selected].name);
    } else {
        snprintf(question, sizeof(question), "Delete %zu selected items permanently?",
                 count);
    }
    if (!ntk_dialog_question(manager->window, "Delete", question)) return;

    for (size_t index = 0; index < manager->entry_count; ++index) {
        if (!manager->entries[index].selected) continue;
        if (strcmp(manager->entries[index].path, "/") == 0) continue;

        if (protected_path(manager, manager->entries[index].path)) {
            char warning[512];
            snprintf(warning, sizeof(warning),
                     "\"%s\" is a critical system location. Delete it anyway?",
                     manager->entries[index].path);
            if (!ntk_dialog_question(manager->window, "Critical Location", warning)) {
                continue;
            }
        }

        char error[512];
        if (!delete_path_recursive(manager->entries[index].path, 0, error,
                                   sizeof(error))) {
            ntk_dialog_error(manager->window, "Delete Failed", error);
            break;
        }
    }
    file_manager_refresh(manager);
}

// create a new file or folder in the current directory
static void action_new_item(FileManager *manager, bool directory) {
    if (manager->archive_mode) {
        ntk_dialog_message(manager->window, "Archive",
                           "Archives are read-only.", NTK_MSG_INFO,
                           NTK_DIALOG_BUTTONS_OK);
        return;
    }
    char *name = ntk_dialog_get_text(manager->window,
                                     directory ? "New Folder" : "New File",
                                     directory ? "Folder name:" : "File name:",
                                     directory ? "New Folder" : "New File.txt");
    if (!name) return;

    if (!filename_is_valid(name)) {
        ntk_dialog_error(manager->window, "Invalid Name",
                         "Names cannot be empty, '.', '..', or contain '/'.");
        free(name);
        return;
    }

    char path[FILE_MANAGER_PATH_MAX];
    if (!path_join(path, sizeof(path), manager->current_path, name)) {
        ntk_dialog_error(manager->window, "Invalid Name", "The path is too long.");
        free(name);
        return;
    }
    if (sys_exists(path)) {
        ntk_dialog_error(manager->window, "Already Exists",
                         "An item with that name already exists.");
        free(name);
        return;
    }

    bool success;
    if (directory) {
        success = sys_mkdir(path) == 0;
    } else {
        int descriptor = sys_open(path, "w");
        success = descriptor >= 0;
        if (descriptor >= 0) sys_close(descriptor);
    }

    if (!success) {
        ntk_dialog_error(manager->window, directory ? "New Folder" : "New File",
                         "The item could not be created.");
    }
    free(name);
    file_manager_refresh(manager);
}

static void action_rename(FileManager *manager) {
    if (manager->archive_mode) return;
    if (file_view_selection_count(manager) != 1) return;
    int selected = file_view_first_selection(manager);
    FileEntry *entry = &manager->entries[selected];

    char *name = ntk_dialog_get_text(manager->window, "Rename", "New name:",
                                     entry->name);
    if (!name) return;
    if (!filename_is_valid(name)) {
        ntk_dialog_error(manager->window, "Invalid Name",
                         "Names cannot be empty, '.', '..', or contain '/'.");
        free(name);
        return;
    }

    char destination[FILE_MANAGER_PATH_MAX];
    char error[512] = {0};
    if (!path_join(destination, sizeof(destination), manager->current_path, name)) {
        ntk_dialog_error(manager->window, "Rename", "The path is too long.");
        free(name);
        return;
    }
    free(name);

    if (sys_exists(destination)) {
        ntk_dialog_error(manager->window, "Rename",
                         "An item with that name already exists.");
        return;
    }

    // User space has no rename syscall. Delete only after a complete copy.
    if (!copy_path_recursive(entry->path, destination, 0, error, sizeof(error)) ||
        !delete_path_recursive(entry->path, 0, error, sizeof(error))) {
        ntk_dialog_error(manager->window, "Rename Failed", error);
    }
    file_manager_refresh(manager);
}

static void action_properties(FileManager *manager) {
    if (file_view_selection_count(manager) != 1) return;
    int selected = file_view_first_selection(manager);
    FileEntry *entry = &manager->entries[selected];

    char size_text[32];
    char date_text[64];
    char message[1536];
    format_size(entry->info.size, size_text, sizeof(size_text));
    format_fat_date(entry->info.write_date, entry->info.write_time,
                    date_text, sizeof(date_text));
    snprintf(message, sizeof(message),
             "Name: %s\nPath: %s\nType: %s\nSize: %s\nModified: %s",
             entry->name, entry->path,
             entry->info.is_directory ? "Folder" : "File",
             entry->info.is_directory ? "Not calculated" : size_text,
             date_text);
    ntk_dialog_message(manager->window, "Properties", message, NTK_MSG_INFO,
                       NTK_DIALOG_BUTTONS_OK);
}

// Check if a path can be safely quoted for command-line use
static bool path_can_be_quoted(const char *path) {
    return path && strchr(path, '"') == NULL;
}

static void action_create_archive(FileManager *manager) {
    if (manager->archive_mode || file_view_selection_count(manager) != 1) return;
    int selected = file_view_first_selection(manager);
    FileEntry *entry = &manager->entries[selected];
    if (!entry->info.is_directory) {
        ntk_dialog_warning(manager->window, "Create Archive",
                           "Select a folder to create an archive.");
        return;
    }

    char archive_path[FILE_MANAGER_PATH_MAX];
    int length = snprintf(archive_path, sizeof(archive_path), "%s%s%s.tar",
                          manager->current_path,
                          strcmp(manager->current_path, "/") == 0 ? "" : "/",
                          entry->name);
    if (length < 0 || (size_t)length >= sizeof(archive_path) ||
        !path_can_be_quoted(entry->path) || !path_can_be_quoted(archive_path)) {
        ntk_dialog_error(manager->window, "Create Archive",
                         "The path is too long or contains unsupported quotes.");
        return;
    }
    if (sys_exists(archive_path)) {
        ntk_dialog_error(manager->window, "Create Archive",
                         "An archive with that name already exists.");
        return;
    }

    char arguments[FILE_MANAGER_PATH_MAX * 2 + 16];
    snprintf(arguments, sizeof(arguments), "-cf \"%s\" \"%s\"",
             archive_path, entry->path);
    // tar.elf already owns the archive format and reports progress in a terminal.
    if (sys_spawn("/bin/tar.elf", arguments, SPAWN_FLAG_TERMINAL, 0) < 0) {
        ntk_dialog_error(manager->window, "Create Archive",
                         "The archive tool could not be started.");
        return;
    }
    ntk_dialog_message(manager->window, "Create Archive",
                       "Archive creation was started in a terminal.", NTK_MSG_INFO,
                       NTK_DIALOG_BUTTONS_OK);
}

static void action_extract_archive(FileManager *manager) {
    if (manager->archive_mode || file_view_selection_count(manager) != 1) return;
    int selected = file_view_first_selection(manager);
    FileEntry *entry = &manager->entries[selected];
    if (entry->info.is_directory || !extension_matches(entry->path, "tar")) {
        ntk_dialog_warning(manager->window, "Extract Archive",
                           "Select a TAR archive to extract.");
        return;
    }

    char folder_name[256];
    snprintf(folder_name, sizeof(folder_name), "%s", entry->name);
    char *extension = strrchr(folder_name, '.');
    if (extension) *extension = '\0';

    char destination[FILE_MANAGER_PATH_MAX];
    if (!path_join(destination, sizeof(destination), manager->current_path,
                   folder_name) ||
        !path_can_be_quoted(entry->path) || !path_can_be_quoted(destination)) {
        ntk_dialog_error(manager->window, "Extract Archive",
                         "The destination path is invalid.");
        return;
    }
    if (sys_exists(destination) || sys_mkdir(destination) != 0) {
        ntk_dialog_error(manager->window, "Extract Archive",
                         "The extraction folder already exists or cannot be created.");
        return;
    }

    char arguments[FILE_MANAGER_PATH_MAX * 2 + 20];
    snprintf(arguments, sizeof(arguments), "-xf \"%s\" -C \"%s\"",
             entry->path, destination);
    // Keep extraction policy in tar.elf, including its unsafe-path checks
    if (sys_spawn("/bin/tar.elf", arguments, SPAWN_FLAG_TERMINAL, 0) < 0) { // Assume tar.elf is installed and works, because the file manager is part of the same system
        sys_delete(destination);
        ntk_dialog_error(manager->window, "Extract Archive",
                         "The archive tool could not be started.");
        return;
    }
    ntk_dialog_message(manager->window, "Extract Archive",
                       "Safe extraction was started in a terminal.", NTK_MSG_INFO,
                       NTK_DIALOG_BUTTONS_OK);
}

static bool extension_matches(const char *path, const char *extension) {
    const char *dot = strrchr(path, '.');
    return dot && strcasecmp(dot + 1, extension) == 0;
}

// Check if a file has an ELF header, indicating it is an executable
static bool file_has_elf_header(const char *path) {
    // Extensions are not trusted for executables
    unsigned char header[4];
    int descriptor = sys_open(path, "r");
    if (descriptor < 0) return false;
    int count = sys_read(descriptor, header, sizeof(header));
    sys_close(descriptor);
    return count == 4 && header[0] == 0x7f && header[1] == 'E' &&
           header[2] == 'L' && header[3] == 'F';
}

// Check if a file is likely to be text by sampling its first 512 bytes
static bool file_is_probably_text(const char *path) {
    // Unknown files still get a useful editor association when the sample is sane.
    unsigned char sample[512];
    int descriptor = sys_open(path, "r");
    if (descriptor < 0) return false;

    int count = sys_read(descriptor, sample, sizeof(sample));
    sys_close(descriptor);
    if (count < 0) return false;
    if (count == 0) return true;

    int text_bytes = 0;
    for (int index = 0; index < count; ++index) {
        unsigned char byte = sample[index];
        if (byte == 0) return false;
        if (byte == '\n' || byte == '\r' || byte == '\t' ||
            (byte >= 32 && byte != 127)) {
            ++text_bytes;
        }
    }
    return text_bytes * 100 >= count * 85;
}

// Launch a program with a single argument, handling quoting and errors
static bool spawn_with_single_argument(FileManager *manager,
                                       const char *program,
                                       const char *path,
                                       uint64_t flags) {
    if (!sys_exists(program)) {
        ntk_dialog_error(manager->window, "Open File",
                         "The associated application is not installed.");
        return false;
    }

    // sys_spawn takes a flat argument string and has no escaping convention.
    if (strchr(path, '"')) {
        ntk_dialog_error(manager->window, "Open File",
                         "This program launcher cannot safely pass a name containing quotes.");
        return false;
    }

    char arguments[FILE_MANAGER_PATH_MAX + 3];
    int length = snprintf(arguments, sizeof(arguments), "\"%s\"", path);
    if (length < 0 || (size_t)length >= sizeof(arguments)) {
        ntk_dialog_error(manager->window, "Open File", "The path is too long.");
        return false;
    }

    if (sys_spawn(program, arguments, flags, 0) < 0) {
        char message[512];
        snprintf(message, sizeof(message), "Could not open \"%s\".", path);
        ntk_dialog_error(manager->window, "Open File", message);
        return false;
    }
    return true;
}

// Open a file with the appropriate application based on its type or extension
// Sloppy for now
static bool file_launcher_open(FileManager *manager, const char *path) {
    if (file_has_elf_header(path)) {
        char question[512];
        snprintf(question, sizeof(question), "Run the executable \"%s\"?",
                 path_basename(path));
        if (!ntk_dialog_question(manager->window, "Run Program", question)) {
            return false;
        }
        if (sys_spawn(path, NULL, SPAWN_FLAG_BACKGROUND, 0) < 0) {
            ntk_dialog_error(manager->window, "Run Program",
                             "The executable could not be started.");
            return false;
        }
        return true;
    }

    static const char *text_extensions[] = { // most common lol
        "txt", "md", "markdown", "c", "h", "cpp", "cc", "cxx", "hpp",
        "py", "sh", "js", "json", "html", "css", "ini", "conf", "cfg",
        "cmake", "gml", "java", "php", "rb", "sql", "csv",
    };
    for (size_t index = 0;
         index < sizeof(text_extensions) / sizeof(text_extensions[0]); ++index) {
        if (extension_matches(path, text_extensions[index])) {
            return spawn_with_single_argument(manager, "/bin/kilo.elf", path,
                                              SPAWN_FLAG_TERMINAL);
        }
    }

    if (file_is_probably_text(path)) {
        return spawn_with_single_argument(manager, "/bin/kilo.elf", path,
                                          SPAWN_FLAG_TERMINAL);
    }

    ntk_dialog_message(manager->window, "Open File",
                       "No installed application is associated with this file type.",
                       NTK_MSG_WARNING, NTK_DIALOG_BUTTONS_OK);
    return false;
}

static void file_manager_open_selection(FileManager *manager) {
    if (file_view_selection_count(manager) != 1) return;
    int selected = file_view_first_selection(manager);
    FileEntry *entry = &manager->entries[selected];

    if (entry->info.is_directory) {
        file_manager_navigate(manager, entry->path, NAVIGATION_NORMAL);
    } else if (!manager->archive_mode && extension_matches(entry->path, "tar")) {
        char archive_location[FILE_MANAGER_PATH_MAX];
        int length = snprintf(archive_location, sizeof(archive_location), "%s:/",
                              entry->path);
        if (length >= 0 && (size_t)length < sizeof(archive_location)) {
            file_manager_navigate(manager, archive_location, NAVIGATION_NORMAL);
        }
    } else if (manager->archive_mode) {
        // Archive members have virtual paths, not descriptors that apps can open
        ntk_dialog_message(manager->window, "Archive",
                           "Files inside an archive are read-only. Extract the archive to open them.",
                           NTK_MSG_INFO, NTK_DIALOG_BUTTONS_OK);
    } else if (extension_matches(entry->path, "tgz") ||
               extension_matches(entry->path, "gz") ||
               extension_matches(entry->path, "bz2") ||
               extension_matches(entry->path, "xz")) {
        ntk_dialog_message(manager->window, "Compressed Archive",
                           "Compressed archives are not supported. Use an uncompressed .tar archive.",
                           NTK_MSG_WARNING, NTK_DIALOG_BUTTONS_OK);
    } else {
        file_launcher_open(manager, entry->path);
    }
}

static void directory_tree_items_clear(FileManager *manager) {
    for (size_t index = 0; index < manager->tree_item_count; ++index) {
        free(manager->tree_items[index]->path);
        free(manager->tree_items[index]);
    }
    free(manager->tree_items);
    manager->tree_items = NULL;
    manager->tree_item_count = 0;
    manager->tree_item_capacity = 0;
}

static DirectoryTreeItem *directory_tree_item_add(FileManager *manager,
                                                   NtkTreeNode *node,
                                                   const char *path) {
    if (manager->tree_item_count == manager->tree_item_capacity) {
        size_t new_capacity = manager->tree_item_capacity
                                  ? manager->tree_item_capacity * 2
                                  : 32;
        DirectoryTreeItem **larger = realloc(
            manager->tree_items, new_capacity * sizeof(*larger));
        if (!larger) return NULL;
        manager->tree_items = larger;
        manager->tree_item_capacity = new_capacity;
    }

    DirectoryTreeItem *item = calloc(1, sizeof(*item));
    if (!item) return NULL;
    item->node = node;
    item->path = strdup(path);
    if (!item->path) {
        free(item);
        return NULL;
    }
    manager->tree_items[manager->tree_item_count++] = item;
    ntk_tree_node_set_user_data(node, item);
    return item;
}

static bool directory_tree_load_children(FileManager *manager,
                                         DirectoryTreeItem *parent_item) {
    if (!parent_item || parent_item->children_loaded) return true;

    char error[512];
    FileEntry *entries = NULL;
    size_t entry_count = 0;
    if (!directory_read(parent_item->path, manager->show_hidden, &entries,
                        &entry_count, error, sizeof(error))) {
        ntk_tree_node_set_has_children(parent_item->node, false);
        return false;
    }

    size_t directory_count = 0;
    for (size_t index = 0; index < entry_count; ++index) {
        if (!entries[index].info.is_directory) continue;
        NtkTreeNode *node = ntk_tree_node_add_child(parent_item->node,
                                                    entries[index].name);
        if (!node) continue;

        DirectoryTreeItem *item = directory_tree_item_add(
            manager, node, entries[index].path);
        if (!item) continue;
        ++directory_count;
        // Assume folders can expand until their first lazy scan proves otherwise.
        ntk_tree_node_set_has_children(node, true);

        NtkPixmap *icon = file_icon(manager, entries[index].path,
                                    &entries[index].info, false, 16);
        if (icon) ntk_tree_node_set_icon(node, ntk_pixmap_clone(icon));
    }

    parent_item->children_loaded = true;
    ntk_tree_node_set_has_children(parent_item->node, directory_count > 0);
    file_entries_free(entries, entry_count);
    ntk_widget_resize_to_preferred(manager->directory_tree);
    return true;
}

static DirectoryTreeItem *directory_tree_find_path(FileManager *manager,
                                                   const char *path) {
    for (size_t index = 0; index < manager->tree_item_count; ++index) {
        if (strcmp(manager->tree_items[index]->path, path) == 0) {
            return manager->tree_items[index];
        }
    }
    return NULL;
}

static void directory_tree_reset(FileManager *manager) {
    directory_tree_items_clear(manager);
    NtkTreeNode *root = ntk_tree_view_get_root(manager->directory_tree);
    ntk_tree_node_clear_children(root);

    NtkTreeNode *filesystem_node = ntk_tree_node_add_child(root, "/");
    if (!filesystem_node) return;
    FAT32_FileInfo root_info = {0};
    root_info.is_directory = 1;
    NtkPixmap *icon = file_icon(manager, "/", &root_info, true, 16);
    if (icon) ntk_tree_node_set_icon(filesystem_node, ntk_pixmap_clone(icon));
    ntk_tree_node_set_has_children(filesystem_node, true);
    directory_tree_item_add(manager, filesystem_node, "/");
    ntk_tree_node_set_expanded(filesystem_node, true);
}

static void directory_tree_synchronize(FileManager *manager) {
    directory_tree_reset(manager);
    DirectoryTreeItem *current = directory_tree_find_path(manager, "/");
    if (!current) return;

    char partial[FILE_MANAGER_PATH_MAX] = "/";
    char path_copy[FILE_MANAGER_PATH_MAX];
    snprintf(path_copy, sizeof(path_copy), "%s", manager->current_path);
    char *save_pointer = NULL;

    directory_tree_load_children(manager, current);
    for (char *component = strtok_r(path_copy, "/", &save_pointer); component;
         component = strtok_r(NULL, "/", &save_pointer)) {
        char next_path[FILE_MANAGER_PATH_MAX];
        if (!path_join(next_path, sizeof(next_path), partial, component)) break;
        snprintf(partial, sizeof(partial), "%s", next_path);

        DirectoryTreeItem *next = directory_tree_find_path(manager, partial);
        if (!next) break;
        ntk_tree_node_set_expanded(current->node, true);
        current = next;
        directory_tree_load_children(manager, current);
    }

    ntk_tree_view_set_selected(manager->directory_tree, current->node);
    ntk_tree_node_set_expanded(current->node, true);
    ntk_widget_resize_to_preferred(manager->directory_tree);
}

static void on_tree_expanded(NtkWidget *widget, void *userdata) {
    FileManager *manager = userdata;
    NtkTreeNode *node = ntk_tree_view_get_selected(widget);
    DirectoryTreeItem *item = ntk_tree_node_get_user_data(node);
    if (!item) return;
    directory_tree_load_children(manager, item);
    FAT32_FileInfo info = {0};
    info.is_directory = 1;
    NtkPixmap *icon = file_icon(manager, item->path, &info, true, 16);
    if (icon) ntk_tree_node_set_icon(node, ntk_pixmap_clone(icon));
}

static void on_tree_collapsed(NtkWidget *widget, void *userdata) {
    FileManager *manager = userdata;
    NtkTreeNode *node = ntk_tree_view_get_selected(widget);
    DirectoryTreeItem *item = ntk_tree_node_get_user_data(node);
    if (!item) return;
    FAT32_FileInfo info = {0};
    info.is_directory = 1;
    NtkPixmap *icon = file_icon(manager, item->path, &info, false, 16);
    if (icon) ntk_tree_node_set_icon(node, ntk_pixmap_clone(icon));
}

static void on_tree_selection_changed(NtkWidget *widget, void *userdata) {
    FileManager *manager = userdata;
    NtkTreeNode *node = ntk_tree_view_get_selected(widget);
    DirectoryTreeItem *item = ntk_tree_node_get_user_data(node);
    if (item && strcmp(item->path, manager->current_path) != 0) {
        file_manager_navigate(manager, item->path, NAVIGATION_NORMAL);
    }
}

static void history_clear(char **history, size_t *count) {
    for (size_t index = 0; index < *count; ++index) free(history[index]);
    *count = 0;
}

static bool history_push(char **history, size_t *count, const char *path) {
    if (*count > 0 && strcmp(history[*count - 1], path) == 0) return true;

    if (*count == FILE_MANAGER_HISTORY_MAX) {
        free(history[0]);
        memmove(history, history + 1,
                (FILE_MANAGER_HISTORY_MAX - 1) * sizeof(*history));
        --*count;
    }

    history[*count] = strdup(path);
    if (!history[*count]) return false;
    ++*count;
    return true;
}

static void file_manager_update_status(FileManager *manager) {
    size_t selected_count = 0;
    uint64_t selected_size = 0;
    uint64_t total_size = 0;
    int single_selected = -1;

    for (size_t index = 0; index < manager->entry_count; ++index) {
        if (!manager->entries[index].info.is_directory) {
            total_size += manager->entries[index].info.size;
        }
        if (manager->entries[index].selected) {
            single_selected = (int)index;
            ++selected_count;
            if (!manager->entries[index].info.is_directory) {
                selected_size += manager->entries[index].info.size;
            }
        }
    }

    char size_text[32];
    char status[512];
    if (selected_count == 1) {
        FileEntry *entry = &manager->entries[single_selected];
        if (entry->info.is_directory) {
            snprintf(status, sizeof(status), "%s | Folder", entry->name);
        } else {
            format_size(entry->info.size, size_text, sizeof(size_text));
            snprintf(status, sizeof(status), "%s | %s", entry->name, size_text);
        }
    } else if (selected_count > 1) {
        format_size(selected_size, size_text, sizeof(size_text));
        snprintf(status, sizeof(status), "%zu items selected | %s",
                 selected_count, size_text);
    } else {
        format_size(total_size, size_text, sizeof(size_text));
        snprintf(status, sizeof(status), "%zu items | %s", manager->entry_count,
                 size_text);
    }
    ntk_statusbar_set_text(manager->statusbar, status);
}

static void file_manager_update_actions(FileManager *manager) {
    size_t selection_count = file_view_selection_count(manager);
    bool one_selected = selection_count == 1;
    bool any_selected = selection_count > 0;
    bool can_paste = manager->clipboard.mode != CLIPBOARD_NONE &&
                     manager->clipboard.count > 0 &&
                     !manager->archive_mode;

    ntk_widget_set_enabled(manager->back_button, manager->back_count > 0);
    ntk_widget_set_enabled(manager->forward_button, manager->forward_count > 0);
    ntk_widget_set_enabled(manager->up_button,
                           strcmp(manager->current_path, "/") != 0);
    ntk_widget_set_enabled(manager->paste_button, can_paste);
    ntk_widget_set_enabled(manager->paste_item, can_paste);
    ntk_widget_set_enabled(manager->open_item, one_selected);
    ntk_widget_set_enabled(manager->rename_item,
                           one_selected && !manager->archive_mode);
    ntk_widget_set_enabled(manager->delete_item,
                           any_selected && !manager->archive_mode);
    ntk_widget_set_enabled(manager->properties_item, one_selected);

    ntk_menu_item_set_checked(manager->show_hidden_item, manager->show_hidden);
    for (int mode = 0; mode < 4; ++mode) {
        ntk_menu_item_set_checked(manager->view_items[mode],
                                  manager->view_mode == (FileViewMode)mode);
    }
}

static bool file_manager_navigate(FileManager *manager,
                                  const char *target,
                                  NavigationReason reason) {
    char normalized[FILE_MANAGER_PATH_MAX];
    char archive_path[FILE_MANAGER_PATH_MAX] = {0};
    char archive_inner[FILE_MANAGER_PATH_MAX] = {0};
    bool archive_location = archive_location_parse(
        target, archive_path, sizeof(archive_path), archive_inner,
        sizeof(archive_inner));

    if (archive_location) {
        int length = snprintf(normalized, sizeof(normalized), "%s:/%s",
                              archive_path, archive_inner);
        if (length < 0 || (size_t)length >= sizeof(normalized)) {
            ntk_dialog_error(manager->window, "Invalid Location",
                             "The archive location is too long.");
            return false;
        }
    } else if (!path_normalize(normalized, sizeof(normalized),
                               manager->archive_mode ? "/" : manager->current_path,
                               target)) {
        ntk_dialog_error(manager->window, "Invalid Location",
                         "The location is invalid or too long.");
        ntk_text_entry_set_text(manager->location_entry, manager->current_path);
        return false;
    }

    FAT32_FileInfo info;
    if ((!archive_location &&
         (!sys_exists(normalized) || sys_get_file_info(normalized, &info) != 0 ||
          !info.is_directory)) ||
        (archive_location && !sys_exists(archive_path))) {
        char message[512];
        snprintf(message, sizeof(message),
                 "\"%s\" does not exist or is not a folder.", normalized);
        ntk_dialog_error(manager->window, "Invalid Location", message);
        ntk_text_entry_set_text(manager->location_entry, manager->current_path);
        return false;
    }

    char **selected_paths = NULL;
    size_t selected_count = 0;
    if (reason == NAVIGATION_REFRESH) {
        selected_count = file_view_selection_count(manager);
        selected_paths = calloc(selected_count, sizeof(*selected_paths));
        if (selected_paths) {
            size_t output = 0;
            for (size_t index = 0; index < manager->entry_count; ++index) {
                if (manager->entries[index].selected) {
                    selected_paths[output++] = strdup(manager->entries[index].path);
                }
            }
        }
    }

    char error[512];
    FileEntry *new_entries = NULL;
    size_t new_count = 0;
    bool read_success = archive_location
                            ? archive_read_directory(archive_path, archive_inner,
                                                     &new_entries, &new_count,
                                                     error, sizeof(error))
                            : directory_read(normalized, manager->show_hidden,
                                             &new_entries, &new_count, error,
                                             sizeof(error));
    if (!read_success) {
        ntk_dialog_error(manager->window, "Open Folder", error);
        if (selected_paths) {
            for (size_t index = 0; index < selected_count; ++index) {
                free(selected_paths[index]);
            }
            free(selected_paths);
        }
        return false;
    }

    char previous_path[FILE_MANAGER_PATH_MAX];
    snprintf(previous_path, sizeof(previous_path), "%s", manager->current_path);
    if (strcmp(previous_path, normalized) != 0) {
        if (reason == NAVIGATION_BACK) {
            if (manager->back_count > 0) {
                free(manager->back_history[--manager->back_count]);
            }
            history_push(manager->forward_history, &manager->forward_count,
                         previous_path);
        } else if (reason == NAVIGATION_FORWARD) {
            if (manager->forward_count > 0) {
                free(manager->forward_history[--manager->forward_count]);
            }
            history_push(manager->back_history, &manager->back_count,
                         previous_path);
        } else if (reason == NAVIGATION_NORMAL && previous_path[0]) {
            history_push(manager->back_history, &manager->back_count,
                         previous_path);
            history_clear(manager->forward_history, &manager->forward_count);
        }
    }

    file_entries_free(manager->entries, manager->entry_count);
    manager->entries = new_entries;
    manager->entry_count = new_count;
    snprintf(manager->current_path, sizeof(manager->current_path), "%s",
             normalized);
    manager->archive_mode = archive_location;
    snprintf(manager->archive_path, sizeof(manager->archive_path), "%s",
             archive_location ? archive_path : "");
    snprintf(manager->archive_inner_path,
             sizeof(manager->archive_inner_path), "%s",
             archive_location ? archive_inner : "");

    if (selected_paths) {
        for (size_t entry_index = 0; entry_index < manager->entry_count;
             ++entry_index) {
            for (size_t selected_index = 0; selected_index < selected_count;
                 ++selected_index) {
                if (selected_paths[selected_index] &&
                    strcmp(manager->entries[entry_index].path,
                           selected_paths[selected_index]) == 0) {
                    manager->entries[entry_index].selected = true;
                }
            }
        }
        for (size_t index = 0; index < selected_count; ++index) {
            free(selected_paths[index]);
        }
        free(selected_paths);
    }

    ntk_text_entry_set_text(manager->location_entry, manager->current_path);
    ntk_window_set_title(manager->window, "File Manager");
    if (manager->archive_mode) directory_tree_reset(manager);
    else directory_tree_synchronize(manager);
    ntk_widget_resize_to_preferred(manager->file_view);
    ntk_widget_repaint(manager->file_view);
    file_manager_update_status(manager);
    file_manager_update_actions(manager);
    return true;
}

static void file_manager_refresh(FileManager *manager) {
    file_manager_navigate(manager, manager->current_path, NAVIGATION_REFRESH);
}

static void file_manager_navigate_up(FileManager *manager) {
    char parent[FILE_MANAGER_PATH_MAX];
    if (!manager->archive_mode) {
        if (path_parent(parent, sizeof(parent), manager->current_path)) {
            file_manager_navigate(manager, parent, NAVIGATION_NORMAL);
        }
        return;
    }

    if (!manager->archive_inner_path[0]) {
        if (path_parent(parent, sizeof(parent), manager->archive_path)) {
            file_manager_navigate(manager, parent, NAVIGATION_NORMAL);
        }
        return;
    }

    char inner_parent[FILE_MANAGER_PATH_MAX];
    snprintf(inner_parent, sizeof(inner_parent), "%s",
             manager->archive_inner_path);
    char *slash = strrchr(inner_parent, '/');
    if (slash) *slash = '\0';
    else inner_parent[0] = '\0';

    int length = snprintf(parent, sizeof(parent), "%s:/%s",
                          manager->archive_path, inner_parent);
    if (length >= 0 && (size_t)length < sizeof(parent)) {
        file_manager_navigate(manager, parent, NAVIGATION_NORMAL);
    }
}

static void determine_home_directory(FileManager *manager) {
    const char *environment_home = getenv("HOME");
    if (environment_home && strcmp(environment_home, "/") != 0 &&
        sys_exists(environment_home)) {
        snprintf(manager->home_path, sizeof(manager->home_path), "%s",
                 environment_home);
        return;
    }

    char current[FILE_MANAGER_PATH_MAX];
    if (sys_getcwd(current, sizeof(current)) >= 0 &&
        strncmp(current, "/home/", 6) == 0) {
        const char *slash = strchr(current + 6, '/');
        size_t length = slash ? (size_t)(slash - current) : strlen(current);
        if (length < sizeof(manager->home_path)) {
            memcpy(manager->home_path, current, length);
            manager->home_path[length] = '\0';
            return;
        }
    }

    if (sys_exists("/home/anon")) {
        snprintf(manager->home_path, sizeof(manager->home_path), "/home/anon");
    } else if (sys_exists("/home")) {
        snprintf(manager->home_path, sizeof(manager->home_path), "/home");
    } else {
        snprintf(manager->home_path, sizeof(manager->home_path), "/");
    }
}

// Define the actions available
typedef enum {
    ACTION_NEW_FOLDER,
    ACTION_NEW_FILE,
    ACTION_OPEN,
    ACTION_PROPERTIES,
    ACTION_CREATE_ARCHIVE,
    ACTION_EXTRACT_ARCHIVE,
    ACTION_CLOSE,
    ACTION_CUT,
    ACTION_COPY,
    ACTION_PASTE,
    ACTION_RENAME,
    ACTION_DELETE,
    ACTION_SELECT_ALL,
    ACTION_VIEW_LARGE,
    ACTION_VIEW_SMALL,
    ACTION_VIEW_LIST,
    ACTION_VIEW_DETAILS,
    ACTION_REFRESH,
    ACTION_SHOW_HIDDEN,
    ACTION_BACK,
    ACTION_FORWARD,
    ACTION_UP,
    ACTION_HOME,
    ACTION_ROOT,
    ACTION_ABOUT,
} FileManagerAction;

// Perform the specified action
static void perform_action(FileManager *manager, FileManagerAction action) {
    char target[FILE_MANAGER_PATH_MAX];

    switch (action) {
        case ACTION_NEW_FOLDER:
            action_new_item(manager, true);
            break;
        case ACTION_NEW_FILE:
            action_new_item(manager, false);
            break;
        case ACTION_OPEN:
            file_manager_open_selection(manager);
            break;
        case ACTION_PROPERTIES:
            action_properties(manager);
            break;
        case ACTION_CREATE_ARCHIVE:
            action_create_archive(manager);
            break;
        case ACTION_EXTRACT_ARCHIVE:
            action_extract_archive(manager);
            break;
        case ACTION_CLOSE:
            ntk_window_close(manager->window);
            break;
        case ACTION_CUT:
            action_copy(manager, CLIPBOARD_CUT);
            break;
        case ACTION_COPY:
            action_copy(manager, CLIPBOARD_COPY);
            break;
        case ACTION_PASTE:
            action_paste(manager);
            break;
        case ACTION_RENAME:
            action_rename(manager);
            break;
        case ACTION_DELETE:
            action_delete(manager);
            break;
        case ACTION_SELECT_ALL:
            for (size_t index = 0; index < manager->entry_count; ++index) {
                manager->entries[index].selected = true;
            }
            file_manager_update_status(manager);
            file_manager_update_actions(manager);
            ntk_widget_repaint(manager->file_view);
            break;
        case ACTION_VIEW_LARGE:
        case ACTION_VIEW_SMALL:
        case ACTION_VIEW_LIST:
        case ACTION_VIEW_DETAILS:
            manager->view_mode = (FileViewMode)(action - ACTION_VIEW_LARGE);
            ntk_widget_resize_to_preferred(manager->file_view);
            ntk_widget_repaint(manager->file_view);
            file_manager_update_actions(manager);
            break;
        case ACTION_REFRESH:
            file_manager_refresh(manager);
            break;
        case ACTION_SHOW_HIDDEN:
            manager->show_hidden = !manager->show_hidden;
            file_manager_refresh(manager);
            break;
        case ACTION_BACK:
            if (manager->back_count > 0) {
                snprintf(target, sizeof(target), "%s",
                         manager->back_history[manager->back_count - 1]);
                file_manager_navigate(manager, target, NAVIGATION_BACK);
            }
            break;
        case ACTION_FORWARD:
            if (manager->forward_count > 0) {
                snprintf(target, sizeof(target), "%s",
                         manager->forward_history[manager->forward_count - 1]);
                file_manager_navigate(manager, target, NAVIGATION_FORWARD);
            }
            break;
        case ACTION_UP:
            file_manager_navigate_up(manager);
            break;
        case ACTION_HOME:
            file_manager_navigate(manager, manager->home_path, NAVIGATION_NORMAL);
            break;
        case ACTION_ROOT:
            file_manager_navigate(manager, "/", NAVIGATION_NORMAL);
            break;
        case ACTION_ABOUT:
            ntk_dialog_message(
                manager->window, "About File Manager",
                "BoredOS File Manager\n\n Made by Lluciocc 2026 \n Repository: nova/src/explorer.c \n\n This is a simple file manager for BoredOS, providing basic file operations and navigation.",
                NTK_MSG_INFO, NTK_DIALOG_BUTTONS_OK);
            break;
    }
}

static void on_action(NtkWidget *widget, void *userdata) {
    (void)widget;
    perform_action(g_file_manager, (FileManagerAction)(uintptr_t)userdata);
}

static NtkWidget *menu_item_add(NtkWidget *menu,
                                const char *label,
                                FileManagerAction action) {
    NtkWidget *item = ntk_menu_item_new(label);
    if (!item) return NULL;
    ntk_widget_connect(item, "triggered", on_action, (void *)(uintptr_t)action);
    ntk_menu_add_item(menu, item);
    return item;
}

static NtkWidget *toolbar_button_add(FileManager *manager,
                                     NtkWidget *toolbar,
                                     const char *tooltip,
                                     const char *icon_name,
                                     FileManagerAction action) {
    NtkWidget *button = ntk_tool_button_new("", toolbar_icon(manager, icon_name));
    if (!button) return NULL;
    ntk_widget_set_tooltip(button, tooltip);
    ntk_widget_connect(button, "clicked", on_action, (void *)(uintptr_t)action);
    ntk_toolbar_add_button(toolbar, button);
    return button;
}

// Build the menu bar with File, Edit, View, Go, and Help menus
static NtkWidget *build_menu_bar(FileManager *manager) {
    NtkWidget *menu_bar = ntk_menu_bar_new(manager->window);
    if (!menu_bar) return NULL;

    NtkWidget *file_menu = ntk_menu_new();
    menu_item_add(file_menu, "New Folder", ACTION_NEW_FOLDER);
    menu_item_add(file_menu, "New File", ACTION_NEW_FILE);
    ntk_menu_add_separator(file_menu);
    manager->open_item = menu_item_add(file_menu, "Open", ACTION_OPEN);
    manager->properties_item =
        menu_item_add(file_menu, "Properties", ACTION_PROPERTIES);
    menu_item_add(file_menu, "Create Archive", ACTION_CREATE_ARCHIVE);
    menu_item_add(file_menu, "Extract Archive", ACTION_EXTRACT_ARCHIVE);
    ntk_menu_add_separator(file_menu);
    menu_item_add(file_menu, "Close", ACTION_CLOSE);
    ntk_menu_bar_add_menu(menu_bar, file_menu, "File");

    NtkWidget *edit_menu = ntk_menu_new();
    menu_item_add(edit_menu, "Cut", ACTION_CUT);
    menu_item_add(edit_menu, "Copy", ACTION_COPY);
    manager->paste_item = menu_item_add(edit_menu, "Paste", ACTION_PASTE);
    ntk_menu_add_separator(edit_menu);
    manager->rename_item = menu_item_add(edit_menu, "Rename", ACTION_RENAME);
    manager->delete_item = menu_item_add(edit_menu, "Delete", ACTION_DELETE);
    ntk_menu_add_separator(edit_menu);
    menu_item_add(edit_menu, "Select All", ACTION_SELECT_ALL);
    ntk_menu_bar_add_menu(menu_bar, edit_menu, "Edit");

    NtkWidget *view_menu = ntk_menu_new();
    manager->view_items[VIEW_LARGE_ICONS] =
        menu_item_add(view_menu, "Large Icons", ACTION_VIEW_LARGE);
    manager->view_items[VIEW_SMALL_ICONS] =
        menu_item_add(view_menu, "Small Icons", ACTION_VIEW_SMALL);
    manager->view_items[VIEW_LIST] =
        menu_item_add(view_menu, "List", ACTION_VIEW_LIST);
    manager->view_items[VIEW_DETAILS] =
        menu_item_add(view_menu, "Details", ACTION_VIEW_DETAILS);
    ntk_menu_add_separator(view_menu);
    menu_item_add(view_menu, "Refresh", ACTION_REFRESH);
    manager->show_hidden_item =
        menu_item_add(view_menu, "Show Hidden Files", ACTION_SHOW_HIDDEN);
    ntk_menu_bar_add_menu(menu_bar, view_menu, "View");

    NtkWidget *go_menu = ntk_menu_new();
    menu_item_add(go_menu, "Back", ACTION_BACK);
    menu_item_add(go_menu, "Forward", ACTION_FORWARD);
    menu_item_add(go_menu, "Up", ACTION_UP);
    ntk_menu_add_separator(go_menu);
    menu_item_add(go_menu, "Home", ACTION_HOME);
    menu_item_add(go_menu, "Root", ACTION_ROOT);
    ntk_menu_bar_add_menu(menu_bar, go_menu, "Go");

    NtkWidget *help_menu = ntk_menu_new();
    menu_item_add(help_menu, "About File Manager", ACTION_ABOUT);
    ntk_menu_bar_add_menu(menu_bar, help_menu, "Help");
    return menu_bar;
}

static NtkWidget *build_toolbar(FileManager *manager) {
    NtkWidget *toolbar = ntk_toolbar_new(manager->window);
    if (!toolbar) return NULL;

    manager->back_button = toolbar_button_add(
        manager, toolbar, "Back", "go-back", ACTION_BACK);
    manager->forward_button = toolbar_button_add(
        manager, toolbar, "Forward", "go-forward", ACTION_FORWARD);
    manager->up_button = toolbar_button_add(
        manager, toolbar, "Up", "go-up", ACTION_UP);
    toolbar_button_add(manager, toolbar, "Home", "go-home", ACTION_HOME);
    toolbar_button_add(manager, toolbar, "Refresh", "reload", ACTION_REFRESH);
    ntk_toolbar_add_separator(toolbar);
    toolbar_button_add(manager, toolbar, "New Folder", "mkdir", ACTION_NEW_FOLDER);
    toolbar_button_add(manager, toolbar, "Cut", "edit-cut", ACTION_CUT);
    toolbar_button_add(manager, toolbar, "Copy", "edit-copy", ACTION_COPY);
    manager->paste_button = toolbar_button_add(
        manager, toolbar, "Paste", "paste", ACTION_PASTE);
    toolbar_button_add(manager, toolbar, "Delete", "delete", ACTION_DELETE);
    toolbar_button_add(manager, toolbar, "Properties", "properties",
                       ACTION_PROPERTIES);
    return toolbar;
}

static void on_location_activated(NtkWidget *widget, void *userdata) {
    FileManager *manager = userdata;
    const char *text = ntk_text_entry_get_text(widget);
    if (!file_manager_navigate(manager, text, NAVIGATION_NORMAL)) {
        ntk_text_entry_set_text(widget, manager->current_path);
    }
}

static void file_manager_show_context_menu(FileManager *manager,
                                           int global_x,
                                           int global_y,
                                           bool background) {
    (void)manager;
    NtkWidget *menu = ntk_menu_new();
    if (!menu) return;

    if (background) {
        menu_item_add(menu, "New Folder", ACTION_NEW_FOLDER);
        menu_item_add(menu, "New File", ACTION_NEW_FILE);
        menu_item_add(menu, "Paste", ACTION_PASTE);
        ntk_menu_add_separator(menu);
        menu_item_add(menu, "Refresh", ACTION_REFRESH);
    } else {
        menu_item_add(menu, "Open", ACTION_OPEN);
        ntk_menu_add_separator(menu);
        menu_item_add(menu, "Cut", ACTION_CUT);
        menu_item_add(menu, "Copy", ACTION_COPY);
        menu_item_add(menu, "Rename", ACTION_RENAME);
        menu_item_add(menu, "Delete", ACTION_DELETE);
        ntk_menu_add_separator(menu);
        menu_item_add(menu, "Properties", ACTION_PROPERTIES);
        menu_item_add(menu, "Create Archive", ACTION_CREATE_ARCHIVE);
        menu_item_add(menu, "Extract Archive", ACTION_EXTRACT_ARCHIVE);
    }
    ntk_menu_popup(menu, global_x, global_y);
}

// build the interface, menu, toolbar, status bar, location entry, and file view
static bool build_interface(FileManager *manager) {
    NtkWidget *menu_bar = build_menu_bar(manager);
    NtkWidget *toolbar = build_toolbar(manager);
    manager->statusbar = ntk_statusbar_new(manager->window);
    if (!menu_bar || !toolbar || !manager->statusbar) return false;
    ntk_window_set_menubar(manager->window, menu_bar);
    ntk_window_set_toolbar(manager->window, toolbar);
    ntk_window_set_statusbar(manager->window, manager->statusbar);

    NtkWidget *content = ntk_box_new(NTK_VERTICAL, manager->window);
    NtkWidget *location_row = ntk_box_new(NTK_HORIZONTAL, content);
    NtkWidget *location_label = ntk_label_new("Location:", location_row);
    manager->location_entry = ntk_text_entry_new(location_row);
    manager->splitter = ntk_splitter_new(NTK_HORIZONTAL, content);
    if (!content || !location_row || !location_label || !manager->location_entry ||
        !manager->splitter) {
        return false;
    }

    ntk_box_set_spacing(content, 2);
    ntk_box_set_spacing(location_row, 6);
    ntk_widget_set_margin(location_row, NTK_INSETS(3, 4, 3, 4));
    ntk_box_pack_start(location_row, location_label, false, false, 0);
    ntk_box_pack_start(location_row, manager->location_entry, true, true, 0);
    ntk_box_pack_start(content, location_row, false, true, 0);
    ntk_box_pack_start(content, manager->splitter, true, true, 0);
    ntk_widget_connect(manager->location_entry, "activated",
                       on_location_activated, manager);

    manager->tree_scroll = ntk_scroll_area_new(manager->splitter);
    manager->directory_tree = ntk_tree_view_new(manager->tree_scroll);
    manager->file_scroll = ntk_scroll_area_new(manager->splitter);
    manager->file_view = file_view_new(manager, manager->file_scroll);
    if (!manager->tree_scroll || !manager->directory_tree ||
        !manager->file_scroll || !manager->file_view) {
        return false;
    }

    ntk_scroll_area_set_policy(manager->tree_scroll, NTK_SCROLL_AUTO,
                               NTK_SCROLL_AUTO);
    ntk_scroll_area_set_content(manager->tree_scroll, manager->directory_tree);
    ntk_scroll_area_set_policy(manager->file_scroll, NTK_SCROLL_AUTO,
                               NTK_SCROLL_AUTO);
    ntk_scroll_area_set_content(manager->file_scroll, manager->file_view);
    ntk_splitter_set_widgets(manager->splitter, manager->tree_scroll,
                             manager->file_scroll);
    ntk_splitter_set_position(manager->splitter, 230);

    ntk_widget_connect(manager->directory_tree, "selection-changed",
                       on_tree_selection_changed, manager);
    ntk_widget_connect(manager->directory_tree, "expanded", on_tree_expanded,
                       manager);
    ntk_widget_connect(manager->directory_tree, "collapsed", on_tree_collapsed,
                       manager);
    ntk_window_set_content(manager->window, content);
    return true;
}

// Free all resources associated with the file manager
static void file_manager_destroy(FileManager *manager) {
    if (!manager) return;
    file_entries_free(manager->entries, manager->entry_count);
    clipboard_clear(&manager->clipboard);
    history_clear(manager->back_history, &manager->back_count);
    history_clear(manager->forward_history, &manager->forward_count);
    directory_tree_items_clear(manager);

    for (size_t index = 0; index < manager->icon_cache_count; ++index) {
        ntk_pixmap_destroy(manager->icon_cache[index].pixmap);
    }
    free(manager);
}

// MAIN
int main(int argc, char **argv) {
    NtkApp *app = ntk_app_new();
    if (!app) return 1;

    FileManager *manager = calloc(1, sizeof(*manager));
    if (!manager) {
        ntk_app_destroy(app);
        return 1;
    }
    manager->app = app;
    manager->show_hidden = true;
    manager->view_mode = VIEW_LARGE_ICONS;
    determine_home_directory(manager);

    manager->window = ntk_window_new("File Manager", 900, 600);
    if (!manager->window) {
        file_manager_destroy(manager);
        ntk_app_destroy(app);
        return 1;
    }

    g_file_manager = manager;
    ntk_window_set_resizable(manager->window, true);
    ntk_window_set_minimizable(manager->window, true);
    ntk_window_set_maximizable(manager->window, true);
    ntk_window_set_closable(manager->window, true);
    ntk_window_set_icon_path(
        manager->window,
        "/Library/Icons/serenityicons/32x32/app-file-manager.png");

    if (!build_interface(manager)) {
        ntk_dialog_error(manager->window, "File Manager",
                         "The user interface could not be created.");
        ntk_app_destroy(app);
        file_manager_destroy(manager);
        return 1;
    }

    const char *initial_path = argc > 1 ? argv[1] : manager->home_path;
    manager->current_path[0] = '\0';
    if (!file_manager_navigate(manager, initial_path, NAVIGATION_REFRESH)) {
        file_manager_navigate(manager, "/", NAVIGATION_REFRESH);
    }

    ntk_window_center_on_screen(manager->window);
    ntk_widget_show(manager->window);
    ntk_app_draw_windows();
    int result = ntk_app_run(app);

    ntk_app_destroy(app);
    file_manager_destroy(manager);
    g_file_manager = NULL;
    return result;
}
