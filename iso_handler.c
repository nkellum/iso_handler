#define FILESYSTEM_TABLE_OFFSET_LOC 0x424
#define FILESYSTEM_TABLE_SIZE_LOC 0x428

#define MAX_DATA_SIZE_TO_READ_AT_ONCE 64000000 // 64MB


#include "iso_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/*
    Seeks to the offset in the iso_file, then read the bytes
    incrementally using fgetc() to return a u32.
*/
uint32_t file_bytes_to_u32(FILE *f, uint32_t offset) {


    fseek(f, offset, SEEK_SET);


    // Little Endian
    // return fgetc(f) | (fgetc(f) << 8) | (fgetc(f) << 16) | (fgetc(f) << 24);


    // Big Endian
    return (fgetc(f) << 24) + (fgetc(f) << 16) + (fgetc(f) << 8) + fgetc(f);
}


/*
    Seeks to the offset in the iso_file, then read the bytes
    incrementally using fgetc() to return a u16.
*/
uint16_t file_bytes_to_u16(FILE *f, uint32_t offset) {


    fseek(f, offset, SEEK_SET);


    // Little Endian
    // return fgetc(f) | (fgetc(f) << 8);


    // Big Endian
    return (fgetc(f) << 8) + fgetc(f);
}


/*
    Seeks to the offset in the iso_file, then read the bytes
    incrementally using fgetc() to return a u8.
*/
uint8_t file_bytes_to_u8(FILE *f, uint32_t offset) {


    fseek(f, offset, SEEK_SET);


    return fgetc(f);
}


// Insert contents of skin_file_buffer into iso

    //     if (strstr(entry_name, "PlFcNr.rat") != NULL)
    //     {
    //         printf("%s (%s) %d file_offset_or_pardir_index: %lu file_size_or_next_index_not_in_dir: %lu\n", i == 0 ? "GALE01" : entry_name, is_folder ? "folder" : "iso_file", i, file_offset_or_pardir_index, file_size_or_next_index_not_in_dir);

    //         fseek(iso_file, file_offset_or_pardir_index, SEEK_SET);

    //         uint32_t output_buffer_size = file_size_or_next_index_not_in_dir;
    //         printf("outputbuffersize: %d\n", output_buffer_size);

    //         if (skin_file_buffer_size <= file_size_or_next_index_not_in_dir)
    //         {
    //             int bytes_read = fwrite(skin_file_buffer, skin_file_buffer_size, 1, iso_file);

    //             if (bytes_read == 0) {
    //                 perror("fwrite Error");
    //             }

    //             if (skin_file_buffer_size < file_size_or_next_index_not_in_dir)
    //             {
    //                 // int null_bytes_size = file_size_or_next_index_not_in_dir - skin_file_buffer_size;

    //                 // fseek(iso_file, skin_file_buffer_size, SEEK_CUR);

    //                 // int bytes_read = fwrite(skin_file_buffer, skin_file_buffer_size, 1, iso_file);

    //                 // if (bytes_read == 0) {
    //                 //     perror("fwrite Error");
    //                 // }

    //             }
    //         }
    //         else 
    //         {
    //             printf("Cannot replace file, it is bigger than the original.\n");
    //             exit(1);
    //         }
    //     }

void read_directory(iso_handler_t* iso_handler, file_entry_t* directory_file_entry, char* dir_path)
{
    uint16_t i = directory_file_entry->file_index + 1;

    while (i < directory_file_entry->next_fst_index)
    {
        file_entry_t* file_entry = &(iso_handler->file_entries[i]);

        // Set parent/children relationships
        // file_entry->parent = directory_file_entry;

        if (file_entry->is_dir)
        {
            sprintf(file_entry->file_path, "%s/%s", dir_path, file_entry->name);

            // printf("File name: %s | File path: %s | File index: %d | Is dir: %s\n",
            // file_entry->name,
            // file_entry->file_path,
            // file_entry->file_index,
            // file_entry->is_dir ? "yes" : "no");

            read_directory(iso_handler, file_entry, file_entry->file_path);
            i = file_entry->next_fst_index;
        }
        else
        {
            sprintf(file_entry->file_path, "%s/%s", dir_path, file_entry->name);

            // printf("File name: %s | File path: %s | File index: %d | Is dir: %s\n",
            // file_entry->name,
            // file_entry->file_path,
            // file_entry->file_index,
            // file_entry->is_dir ? "yes" : "no");

            i++;
        }

        
    }
}

void read_filesystem(iso_handler_t *iso_handler)
{
    uint16_t num_file_entries = file_bytes_to_u32(iso_handler->file_ptr, iso_handler->filesystem_table_offset + 8); //read_u32(self.iso_file, self.fst_offset + 8)
    uint32_t name_table_offset = iso_handler->filesystem_table_offset + num_file_entries * 0xC; // each entry is 12 bytes


    printf("filesystem_table_offset: %u\nname_table_offset: %u\nfilesystem_table_size: %u\nnum_file_entries: %d\n\n",
    iso_handler->filesystem_table_offset,
    name_table_offset,
    iso_handler->filesystem_table_size,
    num_file_entries
    );

    uint32_t biggest_file_size = 0;
    uint16_t biggest_file_index = 0;

    for (int i = 0; i < num_file_entries; i++)
    {
        uint32_t file_entry_offset = iso_handler->filesystem_table_offset + (i * 0xC);

        uint32_t is_dir_and_name_offset = file_bytes_to_u32(iso_handler->file_ptr, file_entry_offset);
        bool is_folder = ((is_dir_and_name_offset & 0xFF000000) != 0);
        uint32_t name_offset = (is_dir_and_name_offset & 0x00FFFFFF);

        uint32_t file_offset_or_parent_dir_index = file_bytes_to_u32(iso_handler->file_ptr, file_entry_offset + 4);
        uint32_t file_size_or_next_index_not_in_dir = file_bytes_to_u32(iso_handler->file_ptr, file_entry_offset + 8);

        // Find and set file entry name
        fseek(iso_handler->file_ptr, name_table_offset + name_offset, SEEK_SET);
        fgets(iso_handler->file_entries[iso_handler->file_entries_size].name, 64, iso_handler->file_ptr);
        rewind(iso_handler->file_ptr);

        iso_handler->file_entries[iso_handler->file_entries_size].file_index = i;
        iso_handler->file_entries[iso_handler->file_entries_size].is_dir = is_folder;
        if(is_folder)
        {
            iso_handler->file_entries[iso_handler->file_entries_size].parent_fst_index = file_offset_or_parent_dir_index;
            iso_handler->file_entries[iso_handler->file_entries_size].next_fst_index = file_size_or_next_index_not_in_dir;
        }
        else
        {
            iso_handler->file_entries[iso_handler->file_entries_size].file_data_offset = file_offset_or_parent_dir_index;
            iso_handler->file_entries[iso_handler->file_entries_size].file_size = file_size_or_next_index_not_in_dir;

            if (file_size_or_next_index_not_in_dir > biggest_file_size) {
                biggest_file_size = file_size_or_next_index_not_in_dir;
                biggest_file_index = i;
            }
        }

        iso_handler->file_entries_size++;
    }

    read_directory(iso_handler, &(iso_handler->file_entries[0]), "files");

    // printf("Biggest file size is %lu Mb, file index %u\n", biggest_file_size / 1000000, biggest_file_index);

}

void pad_output_iso_by(FILE* output_file, uint32_t amount)
{
    for (uint32_t i = 0; i < amount; i++)
        fputc('\0', output_file);
}

void align_output_iso_to_nearest(FILE* output_file, uint32_t size)
{
    uint32_t current_offset = ftell(output_file);
    uint32_t next_offset = current_offset + (size - current_offset % size) % size;
    uint32_t padding_needed = next_offset - current_offset;
    pad_output_iso_by(output_file, padding_needed);
}

void generate_output_iso(iso_handler_t *iso_handler, char *output_iso_filepath)
{
    FILE *output_iso;
    /* create output iso file for writing */
    output_iso = fopen(output_iso_filepath, "w+b");

    if (output_iso == NULL) {
        perror("Failed creating ouput iso: ");
        exit(1);
    }

    uint8_t *copy_buffer = malloc(MAX_DATA_SIZE_TO_READ_AT_ONCE);

    // Reset input fp to beginning of file
    rewind(iso_handler->file_ptr);

    /* Copy:
        - boot.bin
        - bi2.bin
        - apploader.img
        - main.dol
        - Game.toc (FST):
            - File entries
            - File name table

        To the copy buffer. Since the FST always comes after the system files and apploader, 
        we can just copy everything before it over to the output iso, since it stays unchanged. 
    */
    uint32_t system_files_and_fst_block_size = iso_handler->filesystem_table_offset + iso_handler->filesystem_table_size;
    fread(copy_buffer, 1, system_files_and_fst_block_size, iso_handler->file_ptr);
    fwrite(copy_buffer, 1, system_files_and_fst_block_size, output_iso);

    align_output_iso_to_nearest(output_iso, 4);

    // clock_t total_time_start;
    // clock_t total_time_end;
    // clock_t copy_to_buffer_time_start;
    // clock_t copy_to_buffer_time_end;
    // clock_t copy_and_write_time_start;
    // clock_t copy_and_write_time_end;
    // clock_t write_to_iso_time_start;
    // clock_t write_to_iso_time_end;
    // clock_t fst_time_start;
    // clock_t fst_time_end;
    // double absolute_time = 0.0;

    for (uint16_t i = 0; i < iso_handler->file_entries_size; i++)
    {
        uint32_t current_file_start_offset = ftell(output_iso);

        

        if (iso_handler->file_entries[i].is_dir == false)
        {
            // total_time_start = clock();

            file_entry_t file_entry = iso_handler->file_entries[i];

            uint32_t size_remaining = file_entry.file_size;
            uint32_t offset_in_file = 0;

            // copy_and_write_time_start = clock();

            while (size_remaining > 0)
            {
                // Either write the full file size or do it in chunks if too big
                uint32_t size_to_read = size_remaining < MAX_DATA_SIZE_TO_READ_AT_ONCE ? size_remaining : MAX_DATA_SIZE_TO_READ_AT_ONCE;

                // copy_to_buffer_time_start = clock();
                // Copy file/chunk from input iso to the copy buffer
                fseek(iso_handler->file_ptr, file_entry.file_data_offset + offset_in_file, SEEK_SET);
                fread(copy_buffer, 1, size_to_read, iso_handler->file_ptr);

                // copy_to_buffer_time_end = clock();

                // write_to_iso_time_start = clock();
                // Write from copy buffer to output iso
                fwrite(copy_buffer, 1, size_to_read, output_iso);
                // write_to_iso_time_end = clock();
                
                size_remaining -= size_to_read;
                offset_in_file += size_to_read;
            }

            // copy_and_write_time_end = clock();

            // fst_time_start = clock();

            // Go to FST entry for this file and update its offset
            uint32_t file_entry_offset = iso_handler->filesystem_table_offset + file_entry.file_index * 0xC;
            fseek(output_iso, file_entry_offset + 4, SEEK_SET);
            // flip endianness
            uint32_t current_file_start_offset_big_endian = ((current_file_start_offset >> 24) & 0xFF) | ((current_file_start_offset >> 8) & 0xFF00) | ((current_file_start_offset << 8) & 0xFF0000) | ((current_file_start_offset << 24) & 0xFF000000);
            fwrite(&current_file_start_offset_big_endian, sizeof(uint32_t), 1, output_iso);

            // fst_time_end = clock();

            // If changed file, get that size, if not use original size
            //
            // if file_entry.file_path in self.changed_files:
            //     file_size = data_len(self.changed_files[file_entry.file_path])
            // else:
            //     file_size = file_entry.file_size

            // Set file ptr to end of current file and add padding after it
            fseek(output_iso, current_file_start_offset + file_entry.file_size, SEEK_SET);
            align_output_iso_to_nearest(output_iso, 4);


            // total_time_end = clock();

            // double total_time = ((double)(total_time_end - total_time_start)) / CLOCKS_PER_SEC;
            // double copy_time = ((double)(copy_to_buffer_time_end - copy_to_buffer_time_start)) / CLOCKS_PER_SEC;
            // double copy_and_write_time = ((double)(copy_and_write_time_end - copy_and_write_time_start)) / CLOCKS_PER_SEC;
            // double write_time = ((double)(write_to_iso_time_end - write_to_iso_time_start)) / CLOCKS_PER_SEC;
            // double fst_time = ((double)(fst_time_end - fst_time_start)) / CLOCKS_PER_SEC;

            // absolute_time += total_time;

            // if (total_time * 1000 > 100)
            //     printf("%s (%lu Mb) | copy: %f ms | write: %f ms | copy/write: %f ms | total: %f ms\n", file_entry.name, file_entry.file_size / 1000000, copy_time * 1000, write_time * 1000, copy_and_write_time * 1000, total_time * 1000);
            // printf("%s | total: %.3f ms\n", file_entry.name, total_time * 1000);
        }

    }    
    // printf("Biggest file size is %lu Mb, file index %u\n", biggest_file_size / 1000000, biggest_file_index);


    // printf("Absolute time: %.3f\n", absolute_time);

    printf("Successfully wrote %lu bytes to %s\n", ftell(output_iso), output_iso_filepath);

    align_output_iso_to_nearest(output_iso, 2048);

    fclose(output_iso);
}

iso_handler_t* init_iso_file(char* iso_filepath)
{
    printf("size of iso_handler_t struct: %d\n", sizeof(iso_handler_t));

    

    iso_handler_t* iso_handler;
    iso_handler = malloc(sizeof(iso_handler_t));
    iso_handler->file_entries_size = 0;

    FILE *iso_fp;
    /* open an existing iso file for reading */
    iso_handler->file_ptr = fopen(iso_filepath, "r+b");

    if (iso_handler->file_ptr == NULL) {
        perror("Failed reading input iso: ");
        exit(1);
    }

 

    iso_handler->filesystem_table_offset = file_bytes_to_u32(iso_handler->file_ptr, FILESYSTEM_TABLE_OFFSET_LOC);
    iso_handler->filesystem_table_size = file_bytes_to_u32(iso_handler->file_ptr, FILESYSTEM_TABLE_SIZE_LOC);

    read_filesystem(iso_handler);

    generate_output_iso(iso_handler, "generated.iso");


    fclose(iso_handler->file_ptr);
    return iso_handler;

}