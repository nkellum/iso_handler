#ifndef ISO_HANDLER_H
#define ISO_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#define MAX_NUM_FILE_ENTRIES 2048


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
    FILE            *file_ptr;
    uint32_t        filesystem_table_offset;
    uint16_t        filesystem_table_size;
    file_entry_t    file_entries[MAX_NUM_FILE_ENTRIES];
    uint16_t        file_entries_size;

} iso_handler_t;


iso_handler_t* init_iso_file(char* iso_filepath);
void generate_output_iso(iso_handler_t *iso_handler, char *output_iso_filepath);

#endif