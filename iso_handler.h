#ifndef ISO_HANDLER_H
#define ISO_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define MAX_NUM_FILE_ENTRIES 2048

typedef enum {
    ISO_HANDLER_SUCCESS,
    ISO_HANDLER_ERROR,
    ISO_HANDLER_ISO_OPEN_ERROR,
    ISO_HANDLER_ENTRY_NOT_FOUND_ERROR,
    ISO_HANDLER_OUTPUT_ISO_OPEN_ERROR,
    ISO_HANDLER_FAST_REBUILD_COPY_LIMIT_ERROR,
} ISO_HANDLER_RETURN_CODE;


// typedef struct 
// {
//     /* data */
// } system_file_t;

typedef struct FILE_ENTRY
{
    char                name[32];
    char                file_path[64];
    uint16_t            file_index;
    bool                is_dir;
    struct FILE_ENTRY*  parent;
    
    // Directory
    uint16_t            parent_fst_index;
    uint16_t            next_fst_index;

    // File
    uint32_t            file_data_offset;
    uint32_t            file_size;

} file_entry_t;

typedef struct 
{
    char                file_to_replace[32];
    uint32_t            file_size;
    uint8_t             *file_buffer;
    uint32_t            index_of_file_to_replace;

} replacement_file_t;



typedef struct 
{
    FILE            *file_ptr;
    char            filepath[255];
    uint32_t        iso_size;
    uint32_t        filesystem_table_offset;
    uint16_t        filesystem_table_size;
    file_entry_t    file_entries[MAX_NUM_FILE_ENTRIES];
    uint16_t        file_entries_size;

} iso_handler_t;

void init_replacement_file(replacement_file_t *skin_file, const char *file_to_replace);
ISO_HANDLER_RETURN_CODE init_iso_handler(iso_handler_t *iso_handler, char* iso_filepath);
ISO_HANDLER_RETURN_CODE rebuild_iso_and_replace_file(iso_handler_t *iso_handler, replacement_file_t *replacement_skin_file, char *output_iso_filepath);
ISO_HANDLER_RETURN_CODE replace_file_in_iso(iso_handler_t *iso_handler, replacement_file_t *replacement_skin_file);
ISO_HANDLER_RETURN_CODE get_file_entry_size(iso_handler_t* iso_handler, uint32_t *file_entry_size, char* file_entry_name);
ISO_HANDLER_RETURN_CODE get_file_entry_binary_data(iso_handler_t* iso_handler, uint8_t *file_entry_data, char* file_entry_name);




#endif