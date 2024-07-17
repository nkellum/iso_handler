#define FILESYSTEM_TABLE_OFFSET_LOC 0x424
#define FILESYSTEM_TABLE_SIZE_LOC 0x428

#define MAX_DATA_SIZE_TO_READ_AT_ONCE 64000000 // 64 MB
#define MAX_FAST_REBUILD_COPY_SIZE 200000000 // 200 MB


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


// Function to swap two elements
static void swap(file_entry_t *a, file_entry_t *b) {
    file_entry_t temp = *a;
    *a = *b;
    *b = temp;
}

// Partition function for Quick Sort
static int partition(file_entry_t file_entries[], int low, int high) {
    int pivot = file_entries[high].file_data_offset; // pivot
    int i = (low - 1); // Index of smaller element

    for (int j = low; j <= high - 1; j++) {
        // If current element is smaller than or equal to pivot
        if (file_entries[j].file_data_offset <= pivot) {
            i++; // increment index of smaller element
            swap(&file_entries[i], &file_entries[j]);
        }
    }
    swap(&file_entries[i + 1], &file_entries[high]);
    return (i + 1);
}

// Quick Sort function
static void quick_sort(file_entry_t file_entries[], int low, int high) {
    if (low < high) {
        // pi is partitioning index, arr[p] is now at right place
        int pi = partition(file_entries, low, high);

        // Separately sort elements before partition and after partition
        quick_sort(file_entries, low, pi - 1);
        quick_sort(file_entries, pi + 1, high);
    }
}

void init_replacement_file(replacement_file_t *skin_file, const char *file_to_replace)
{
    skin_file->file_buffer = NULL;
    skin_file->file_size = 0;
    strncpy(skin_file->file_to_replace, file_to_replace, 32);
}

ISO_HANDLER_RETURN_CODE open_iso(iso_handler_t* iso_handler)
{
    iso_handler->file_ptr = fopen(iso_handler->filepath, "rb+");

    if (iso_handler->file_ptr == NULL) {
        perror("Failed reading input iso: ");
        return ISO_HANDLER_ISO_OPEN_ERROR;
    }

    return ISO_HANDLER_SUCCESS;
}

void close_iso(iso_handler_t* iso_handler)
{
    fclose(iso_handler->file_ptr);
}

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

ISO_HANDLER_RETURN_CODE rebuild_iso_and_replace_file(iso_handler_t *iso_handler, replacement_file_t *replacement_skin_file, char *output_iso_filepath)
{
    clock_t total_time_start;
    clock_t total_time_end;

    total_time_start = clock();




    ISO_HANDLER_RETURN_CODE open_error = open_iso(iso_handler);
    if (open_error)
    {
        return open_error;
    }

    FILE *output_iso;
    /* create output iso file for writing */
    output_iso = fopen(output_iso_filepath, "w+b");

    if (output_iso == NULL) {
        perror("Failed creating ouput iso: ");
        return ISO_HANDLER_OUTPUT_ISO_OPEN_ERROR;
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



    for (uint16_t i = 0; i < iso_handler->file_entries_size; i++)
    {
        uint32_t current_file_start_offset = ftell(output_iso);

        

        if (iso_handler->file_entries[i].is_dir == false)
        {

            file_entry_t file_entry = iso_handler->file_entries[i];

            uint32_t file_size = file_entry.file_size;

            if (i == replacement_skin_file->index_of_file_to_replace)
            {
                // Write from copy buffer to output iso
                fwrite(replacement_skin_file->file_buffer, 1, replacement_skin_file->file_size, output_iso);
                // Set file size to replacement file size for later
                file_size = replacement_skin_file->file_size;
            }
            else
            {
                uint32_t size_remaining = file_entry.file_size;
                uint32_t offset_in_file = 0;


                while (size_remaining > 0)
                {
                    // Either write the full file size or do it in chunks if too big
                    uint32_t size_to_read = size_remaining < MAX_DATA_SIZE_TO_READ_AT_ONCE ? size_remaining : MAX_DATA_SIZE_TO_READ_AT_ONCE;

                    // Copy file/chunk from input iso to the copy buffer
                    fseek(iso_handler->file_ptr, file_entry.file_data_offset + offset_in_file, SEEK_SET);
                    fread(copy_buffer, 1, size_to_read, iso_handler->file_ptr);


                    // Write from copy buffer to output iso
                    fwrite(copy_buffer, 1, size_to_read, output_iso);
                    
                    size_remaining -= size_to_read;
                    offset_in_file += size_to_read;
                }

            }
            


            // Go to FST entry for this file and update its offset
            uint32_t file_entry_offset = iso_handler->filesystem_table_offset + file_entry.file_index * 0xC;
            fseek(output_iso, file_entry_offset + 4, SEEK_SET);
            // flip endianness
            uint32_t current_file_start_offset_big_endian = ((current_file_start_offset >> 24) & 0xFF) | ((current_file_start_offset >> 8) & 0xFF00) | ((current_file_start_offset << 8) & 0xFF0000) | ((current_file_start_offset << 24) & 0xFF000000);
            fwrite(&current_file_start_offset_big_endian, sizeof(uint32_t), 1, output_iso);

            // Go to FST entry for this file and update its size
            fseek(output_iso, file_entry_offset + 8, SEEK_SET);
            uint32_t file_size_big_endian = ((file_size >> 24) & 0xFF) | ((file_size >> 8) & 0xFF00) | ((file_size << 8) & 0xFF0000) | ((file_size << 24) & 0xFF000000);
            fwrite(&file_size_big_endian, sizeof(uint32_t), 1, output_iso);

            // Set file ptr to end of current file and add padding after it
            fseek(output_iso, current_file_start_offset + file_size, SEEK_SET);
            align_output_iso_to_nearest(output_iso, 4);


            

            // printf("%s | total: %.3f ms\n", file_entry.name, total_time * 1000);
        }

    }

    total_time_end = clock();

    double total_time = ((double)(total_time_end - total_time_start)) / CLOCKS_PER_SEC;

    printf("Total time: %.3f\n", total_time);

    printf("Successfully wrote %lu bytes to %s\n", ftell(output_iso), output_iso_filepath);

    align_output_iso_to_nearest(output_iso, 2048);

    free(copy_buffer);

    fclose(output_iso);
    close_iso(iso_handler);

    return ISO_HANDLER_SUCCESS;
}


/*
    Here's the plan. Get the offset of the file you want to replace, and copy the iso contents from that offset to the end of file.
    Write the replcacement file at the correct offset, then write the rest of the file back into the iso, then update the FST.

    TODO: do some proper documentation
*/
ISO_HANDLER_RETURN_CODE fast_rebuild_iso_and_replace_file(iso_handler_t *iso_handler, replacement_file_t *replacement_skin_file)
{
    clock_t total_time_start;
    clock_t total_time_end;

    total_time_start = clock();


    ISO_HANDLER_RETURN_CODE open_error = open_iso(iso_handler);
    if (open_error)
    {
        return open_error;
    }

    file_entry_t file_entry_after_file_to_replace = iso_handler->file_entries[replacement_skin_file->index_of_file_to_replace + 1];
    uint32_t size_of_region_to_copy = iso_handler->iso_size - file_entry_after_file_to_replace.file_data_offset;

    if (size_of_region_to_copy > MAX_FAST_REBUILD_COPY_SIZE)
    {
        printf("Data section of iso to copy for fast rebuild is too big! (%.3f Mb)\n", (float) size_of_region_to_copy / 1000000);
        return ISO_HANDLER_FAST_REBUILD_COPY_LIMIT_ERROR;
    }

    uint8_t *copy_buffer = malloc(size_of_region_to_copy);

    // Copy data from offset of file right after the file to replace to the end of the iso
    fseek(iso_handler->file_ptr, file_entry_after_file_to_replace.file_data_offset, SEEK_SET);
    fread(copy_buffer, 1, size_of_region_to_copy, iso_handler->file_ptr);


    // Write replacement file data
    file_entry_t file_entry_to_replace = iso_handler->file_entries[replacement_skin_file->index_of_file_to_replace];
    fseek(iso_handler->file_ptr, file_entry_to_replace.file_data_offset, SEEK_SET);
    fwrite(replacement_skin_file->file_buffer, 1, replacement_skin_file->file_size, iso_handler->file_ptr);

    uint32_t file_entry_offset = iso_handler->filesystem_table_offset + file_entry_to_replace.file_index * 0xC;
    fseek(iso_handler->file_ptr, file_entry_offset + 8, SEEK_SET);
    uint32_t file_size_big_endian = ((replacement_skin_file->file_size >> 24) & 0xFF) | ((replacement_skin_file->file_size >> 8) & 0xFF00) | ((replacement_skin_file->file_size << 8) & 0xFF0000) | ((replacement_skin_file->file_size << 24) & 0xFF000000);
    fwrite(&file_size_big_endian, sizeof(uint32_t), 1, iso_handler->file_ptr);

    // Set file ptr to end of current file and add padding after it
    fseek(iso_handler->file_ptr, file_entry_to_replace.file_data_offset + replacement_skin_file->file_size, SEEK_SET);
    align_output_iso_to_nearest(iso_handler->file_ptr, 4);

    uint32_t offset_diff = ftell(iso_handler->file_ptr) - iso_handler->file_entries[replacement_skin_file->index_of_file_to_replace + 1].file_data_offset;

    // START BACK HERE BY WRITING CONTENTS FROM COPY BUFFER BACK INTO INPUT ISO
    fwrite(copy_buffer, 1, size_of_region_to_copy, iso_handler->file_ptr);


    // Start from file entry after replacement file index
    for (uint16_t i = replacement_skin_file->index_of_file_to_replace + 1; i < iso_handler->file_entries_size; i++)
    {

        if (iso_handler->file_entries[i].is_dir == false)
        {

            file_entry_t file_entry = iso_handler->file_entries[i];

            uint32_t current_file_start_offset = file_entry.file_data_offset + offset_diff;

            // Go to FST entry for this file and update its offset
            uint32_t file_entry_offset = iso_handler->filesystem_table_offset + file_entry.file_index * 0xC;
            fseek(iso_handler->file_ptr, file_entry_offset + 4, SEEK_SET);
            // flip endianness
            uint32_t current_file_start_offset_big_endian = ((current_file_start_offset >> 24) & 0xFF) | ((current_file_start_offset >> 8) & 0xFF00) | ((current_file_start_offset << 8) & 0xFF0000) | ((current_file_start_offset << 24) & 0xFF000000);
            fwrite(&current_file_start_offset_big_endian, sizeof(uint32_t), 1, iso_handler->file_ptr);

            // printf("%s | total: %.3f ms\n", file_entry.name, total_time * 1000);
        }

    }

    total_time_end = clock();

    double total_time = ((double)(total_time_end - total_time_start)) / CLOCKS_PER_SEC;

    printf("Total time: %.3f\n", total_time);

    free(copy_buffer);

    close_iso(iso_handler);

    return ISO_HANDLER_SUCCESS;
}


/*
    Rebuild iso analogy:

    Imagine you have a bookshelf that perfectly fits a certain number of books, each with a designated spot. 
    If you want to replace a small book with a much larger one, you can’t just swap them out easily. 
    The larger book won’t fit in the same spot without reorganizing the entire shelf.
*/
ISO_HANDLER_RETURN_CODE replace_file_in_iso(iso_handler_t *iso_handler, replacement_file_t *replacement_skin_file)
{
    ISO_HANDLER_RETURN_CODE open_error = open_iso(iso_handler);
    if (open_error)
    {
        return open_error;
    }

    int index_of_file_to_replace = -1;

    for (int i = 0; i < iso_handler->file_entries_size; i++)
    {
        if (strcmp(iso_handler->file_entries[i].name, replacement_skin_file->file_to_replace) == 0)
        {
            index_of_file_to_replace = i;
            replacement_skin_file->index_of_file_to_replace = i;
            break;
        }
    }

    if (index_of_file_to_replace == -1)
    {
        close_iso(iso_handler);
        printf("File entry %s could not be found in %s\n", replacement_skin_file->file_to_replace, iso_handler->filepath);
        return ISO_HANDLER_ENTRY_NOT_FOUND_ERROR;
    }

    file_entry_t file_entry_to_replace = iso_handler->file_entries[index_of_file_to_replace];


    // If replacement file is same or smaller size, a simple fwrite does the trick
    if (replacement_skin_file->file_size <= file_entry_to_replace.file_size)
    {

        fseek(iso_handler->file_ptr, file_entry_to_replace.file_data_offset, SEEK_SET);

        int bytes_written = fwrite(replacement_skin_file->file_buffer, replacement_skin_file->file_size, 1, iso_handler->file_ptr);
        if (bytes_written == 0)
            perror("fwrite Error");

        uint32_t file_entry_offset = iso_handler->filesystem_table_offset + file_entry_to_replace.file_index * 0xC;
        fseek(iso_handler->file_ptr, file_entry_offset + 8, SEEK_SET);
        uint32_t file_size_big_endian = ((replacement_skin_file->file_size >> 24) & 0xFF) | ((replacement_skin_file->file_size >> 8) & 0xFF00) | ((replacement_skin_file->file_size << 8) & 0xFF0000) | ((replacement_skin_file->file_size << 24) & 0xFF000000);
        fwrite(&file_size_big_endian, sizeof(uint32_t), 1, iso_handler->file_ptr);
    }
    else
    {
        printf("File is bigger than original, rebuilding...\n");
        // If replacement file is bigger than original, a rebuild is needed
        // ISO_HANDLER_RETURN_CODE err = rebuild_iso_and_replace_file(iso_handler, replacement_skin_file, "rebuilt-generated.iso");
        // if(err)
        // {
        //     printf("Error rebuilding the iso!\n");
        // }

        ISO_HANDLER_RETURN_CODE err = fast_rebuild_iso_and_replace_file(iso_handler, replacement_skin_file);
        if(err)
        {
            printf("Error rebuilding the iso!\n");
            close_iso(iso_handler);
            return err;
        }

    }

    close_iso(iso_handler);

    return ISO_HANDLER_SUCCESS;
}

ISO_HANDLER_RETURN_CODE get_file_entry_size(iso_handler_t* iso_handler, uint32_t *file_entry_size, char* file_entry_name)
{

    int index_of_file_entry = -1;

    for (int i = 0; i < iso_handler->file_entries_size; i++)
    {
        if (strcmp(iso_handler->file_entries[i].name, file_entry_name) == 0)
        {
            index_of_file_entry = i;
            break;
        }
    }

    if (index_of_file_entry == -1)
    {
        close_iso(iso_handler);
        printf("File entry %s could not be found in %s\n", file_entry_name, iso_handler->filepath);
        return ISO_HANDLER_ENTRY_NOT_FOUND_ERROR;
    }
    

    file_entry_t file_entry = iso_handler->file_entries[index_of_file_entry];

    *file_entry_size = file_entry.file_size;

    return ISO_HANDLER_SUCCESS;
}

ISO_HANDLER_RETURN_CODE get_file_entry_binary_data(iso_handler_t* iso_handler, uint8_t *file_entry_data, char* file_entry_name)
{
    
    int index_of_file_entry = -1;

    for (int i = 0; i < iso_handler->file_entries_size; i++)
    {
        if (strcmp(iso_handler->file_entries[i].name, file_entry_name) == 0)
        {
            index_of_file_entry = i;
            break;
        }
    }

    if (index_of_file_entry == -1)
    {
        close_iso(iso_handler);
        printf("File entry %s could not be found in %s\n", file_entry_name, iso_handler->filepath);
        return ISO_HANDLER_ENTRY_NOT_FOUND_ERROR;
    }

    ISO_HANDLER_RETURN_CODE open_error = open_iso(iso_handler);
    if (open_error)
    {
        return open_error;
    }

    file_entry_t file_entry = iso_handler->file_entries[index_of_file_entry];

    fseek(iso_handler->file_ptr, file_entry.file_data_offset, SEEK_SET);
    fread(file_entry_data, 1, file_entry.file_size, iso_handler->file_ptr);

    close_iso(iso_handler);
    return ISO_HANDLER_SUCCESS;

}



// TODO: make header only lib?
ISO_HANDLER_RETURN_CODE init_iso_handler(iso_handler_t* iso_handler, char* iso_filepath)
{

    // Set basic fields
    iso_handler->file_entries_size = 0;
    strncpy(iso_handler->filepath, iso_filepath, 255);

    // Open iso and store FST information
    ISO_HANDLER_RETURN_CODE open_error = open_iso(iso_handler);
    if (open_error)
    {
        return open_error;
    }

    fseek(iso_handler->file_ptr, 0, SEEK_END);  // Move file pointer to the end of the file
    iso_handler->iso_size = ftell(iso_handler->file_ptr); // Get file size
    rewind(iso_handler->file_ptr);  // Move file pointer to the beginning of the file

    iso_handler->filesystem_table_offset = file_bytes_to_u32(iso_handler->file_ptr, FILESYSTEM_TABLE_OFFSET_LOC);
    iso_handler->filesystem_table_size = file_bytes_to_u32(iso_handler->file_ptr, FILESYSTEM_TABLE_SIZE_LOC);

    // Read and store file entry information
    read_filesystem(iso_handler);

    // Sort the iso file entries by their data offset in the iso
    quick_sort(iso_handler->file_entries, 0, iso_handler->file_entries_size - 1);

    // Falco index 582
    for (int i = 0; i < iso_handler->file_entries_size; i++)
    {
        if (strcmp(iso_handler->file_entries[i].name, "PlFcNr.dat") == 0)
        {
            break;
        }
    }



    close_iso(iso_handler);

    return ISO_HANDLER_SUCCESS;
}