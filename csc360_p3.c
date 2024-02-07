#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <time.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <math.h>

struct __attribute__((__packed__)) superblock_t{
	uint8_t fs_id[8];
	uint16_t block_size;
	uint32_t file_system_block_count;
	uint32_t fat_start_block;
	uint32_t fat_block_count;
	uint32_t root_dir_start_block;
	uint32_t root_dir_block_count;
};

struct __attribute__((__packed__)) dir_entry_timedate_t{
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	
};

struct __attribute__((__packed__)) dir_entry_t{
	uint8_t status;
	uint32_t starting_block;
	uint32_t block_count;
	uint32_t size;
	struct dir_entry_timedate_t create_time;
	struct dir_entry_timedate_t modify_time;
	uint8_t filename[31];
	uint8_t unused[6];
};

void diskinfo(int argc, char* argv[]){
	if(argc != 2){
		perror("Invalid argument quantity!\n");
		exit(1);
	}

	int fd = open(argv[1], O_RDONLY);
	FILE *input = fdopen(fd, "r");
	if(input == NULL){
		perror("Invalid file argument!\n");
		exit(1);
	}

	struct superblock_t* super = malloc(sizeof(struct superblock_t));
    if (super == NULL) {
        perror("Memory allocation error!\n");
        exit(1);
    }

    fseek(input, 0, SEEK_SET);
	fread(super, sizeof(struct superblock_t), 1, input);

	int free_b = 0;
	int reserved_b = 0;
	int allocated_b = 0;

	int start = ntohl(super->fat_start_block);
	int count = ntohl(super->file_system_block_count);
	int size = htons(super->block_size);

	for(int i = 0; i < count; i++){
		fseek(input, i*sizeof(uint32_t) + start*size, SEEK_SET); // seeks the start of the current block (start*size skips to the start block, and then skips past the number of blocks already iterated through)
		
		uint32_t block;
		fread(&block, sizeof(uint32_t), 1, input);
		block = ntohl(block);

		if(block == 0){
			free_b++;
		} else if(block == 1){
			reserved_b++;
		} else{
			allocated_b++;
		}
	}

	printf("Super block information\n");
	printf("Block size: %d\n", htons(super->block_size));
	printf("Block count: %d\n", ntohl(super->file_system_block_count));
	printf("FAT starts: %d\n", ntohl(super->fat_start_block));
	printf("FAT blocks: %d\n", ntohl(super->fat_block_count));
	printf("Root directory starts: %d\n", ntohl(super->root_dir_start_block));
	printf("Root directory blocks: %d\n", ntohl(super->root_dir_block_count));
	printf("\n");
	printf("FAT information\n");
	printf("Free blocks: %d\n", free_b);
	printf("Reserved blocks: %d\n", reserved_b);
	printf("Allocated blocks: %d\n", allocated_b);

	free(super);
	return;
}

void find_dir(struct superblock_t* super, void* address, int block, int block_count, char **dir_names, int level, int levels){
	void *currpoint = address + htons(super->block_size) * block; // jumps to directory

	int num_of_entries = htons(super->block_size) / sizeof(struct dir_entry_t);
    struct dir_entry_t* entries = malloc(num_of_entries * sizeof(struct dir_entry_t)); // make legit and understand it

    for (int i = 0; i < block_count; i++) {
        memcpy(entries, currpoint, htons(super->block_size));

        for (int j = 0; j < num_of_entries; j++) {
       	    struct dir_entry_t* entry = &entries[j];

            if(level == levels){
            	if(ntohl(entry->size) != 0){
               		char* type = "F";
                	if(entry->status >= 4){
                		type = "D";
                	}
               		printf("%s %10d %30s %d/%02d/%02d %02d:%02d:%02d\n", type, ntohl(entry->size), entry->filename, htons(entry->create_time.year), entry->create_time.month, entry->create_time.day, entry->create_time.hour, entry->create_time.minute, entry->create_time.second);
            	}
            } else{
            	if(dir_names[level] != NULL && strcmp((char *)entry->filename, dir_names[level]) == 0){
            		level++;
            		find_dir(super, address, ntohl(entry->starting_block), ntohl(entry->block_count), dir_names, level, levels);
            		level = 0;
            		return;
            	}
            }
        } currpoint += htons(super->block_size);
   	}
    free(entries);
	return;
}

void disklist(int argc, char* argv[]){
	if(argc != 3){
		perror("Invalid argument quantity!\n");
		exit(1);
	}

	int fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
        perror("Error opening file");
        exit(1);
    }

	struct superblock_t* super;

	struct stat buffer;
	int status = fstat(fd, &buffer);
	if(status == -1){
		perror("Error with stat\n");
		exit(1);
	}

	void* address = mmap(NULL, buffer.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (address == MAP_FAILED) {
    	perror("Error mapping file!\n");
    	exit(1);
	}

	char* prop;
	char* dir_names[1024];
	int levels = 0;

	prop = strtok(argv[2], "/");
	while(prop != NULL){ 
		dir_names[levels] = prop;
		prop = strtok(NULL, "/");
		levels++;
	}

	super = (struct superblock_t*) address;

	int level = 0;
	find_dir(super, address, ntohl(super->root_dir_start_block), ntohl(super->root_dir_block_count), dir_names, level, levels);

	return;
}

void copy_data(struct superblock_t* super, void* address, int block, int count, FILE* fp){
	void *currpoint = address + htons(super->block_size) * block; // jumps to directory
    fseek(fp, 0, SEEK_END);
    fwrite(currpoint, htons(super->block_size), 1, fp);
	return;
}

void find_allocated_blocks(struct superblock_t* super, void* address, struct dir_entry_t *dir_entry, const char* file){
	int start = ntohl(super->fat_start_block);
	int size = htons(super->block_size);
	int count = 0;

	FILE* fp = fopen(file, "w");
	if (fp == NULL) {
        perror("Error opening file");
        exit(1);
    }

	void *currpoint = address + start * size + ntohl(dir_entry->starting_block)  * sizeof(uint32_t);
	
	copy_data(super, address, ntohl(dir_entry->starting_block) , count, fp);
	uint32_t *fat_entry = (uint32_t*)currpoint;

	while(*fat_entry != -1){
		count++;
		copy_data(super, address, ntohl(*fat_entry), count, fp);

		currpoint = address + start * size + ntohl(*fat_entry) * sizeof(uint32_t);
		fat_entry = (uint32_t*)currpoint;
	} 

	fclose(fp);
	return;
}

void find_file(struct superblock_t* super, void* address, int block, int block_count, char **root_names, int level, int levels, const char* file){
	void *currpoint = address + htons(super->block_size) * block; // jumps to directory

	int num_of_entries = htons(super->block_size) / sizeof(struct dir_entry_t);
    struct dir_entry_t* entries = malloc(num_of_entries * sizeof(struct dir_entry_t));

    for (int i = 0; i < block_count; i++) {
        memcpy(entries, currpoint, htons(super->block_size));

        for (int j = 0; j < num_of_entries; j++) {
       	    struct dir_entry_t* entry = &entries[j];

            if(level == levels-1){
         		if(root_names[level] != NULL && strcmp((char *)entry->filename, root_names[level]) == 0){
         			find_allocated_blocks(super, address, entry, file);
         			free(entries);
					return;
         		}
            } else if(root_names[level] != NULL && strcmp((char *)entry->filename, root_names[level]) == 0){
            	level++;
            	find_file(super, address, ntohl(entry->starting_block), ntohl(entry->block_count), root_names, level, levels, file);
            	level--;
            }
        } currpoint += htons(super->block_size);
   	}
   	if(level == levels-1){
   		printf("File not found.\n");
   	}
    free(entries);
	return;
}

void diskget(int argc, char* argv[]){
	if(argc != 4){
		perror("Invalid argument quantity!\n");
		exit(1);
	}

	int fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
        perror("Error opening file");
        exit(1);
    }

	struct superblock_t* super;

	struct stat buffer;
	int status = fstat(fd, &buffer);
	if(status == -1){
		perror("Error with stat\n");
		exit(1);
	}

	void* address = mmap(NULL, buffer.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (address == MAP_FAILED) {
    	perror("Error mapping file to memory");
    	exit(1);
	}

	char* prop;
	char* root_names[1024];
	int levels = 0;

	prop = strtok(argv[2], "/");
	while(prop != NULL){ 
		root_names[levels] = prop;
		prop = strtok(NULL, "/");
		levels++;
	}

	super = (struct superblock_t*) address;
	int level = 0;
	find_file(super, address, ntohl(super->root_dir_start_block), ntohl(super->root_dir_block_count), root_names, level, levels, argv[3]);

	return;
}

struct dir_entry_t create_dir(uint32_t start_block, uint32_t blocks_required, int file_size, char* file_name){
	struct dir_entry_t entry;
	struct dir_entry_timedate_t entry_time;

	time_t current_time;
	struct tm *creation_time;

	setenv("TZ", "PST+8", 1);
    tzset();

    current_time = time(NULL);
    creation_time = localtime(&current_time);

	entry_time.year = (uint16_t)(creation_time->tm_year + 1900);
	entry_time.year = htons(entry_time.year);
    entry_time.month = (uint8_t)(creation_time->tm_mon + 1);
    entry_time.day = (uint8_t)(creation_time->tm_mday);
    entry_time.hour = (uint8_t)creation_time->tm_hour;
    entry_time.minute = (uint8_t)creation_time->tm_min;
    entry_time.second = (uint8_t)creation_time->tm_sec;


	entry.status = 0b011;
	entry.starting_block = start_block;
	entry.block_count = blocks_required;
	entry.size = file_size;
	entry.size = htonl(entry.size);
	entry.create_time = entry_time;
	entry.modify_time = entry_time;
	memcpy(entry.filename, file_name, strlen(file_name)+1);

	return entry;
}

bool file_exists(struct superblock_t* super, void* address, int block, int block_count, char **root_names, char* file_name, int level, int levels){
	bool found = false;

	void *currpoint = address + htons(super->block_size) * block; // jumps to directory

	int num_of_entries = htons(super->block_size) / sizeof(struct dir_entry_t);
    struct dir_entry_t* entries = malloc(num_of_entries * sizeof(struct dir_entry_t)); 

    for (int i = 0; i < block_count; i++) {
        memcpy(entries, currpoint, htons(super->block_size));

        for (int j = 0; j < num_of_entries; j++) {
       	    struct dir_entry_t* entry = &entries[j];

            if(level == levels && strcmp(((char *)entry->filename), file_name)==0){
            	found = true;
            } else if(root_names[level] != NULL && strcmp((char *)entry->filename, root_names[level]) == 0){
            	level++;
            	if(!found){
            		found = file_exists(super, address, ntohl(entry->starting_block), ntohl(entry->block_count), root_names, file_name, level, levels);
            	}
            	level = 0;
            }
        } currpoint += htons(super->block_size);
   	}
    free(entries);
    return found;
}

struct dir_entry_t find_blocks(struct superblock_t* super, void* address, int file_size, FILE *input, char* file_name){
	uint32_t first_block = 0;
	uint32_t current_block = ntohl(super->fat_start_block);
	uint32_t prev_block = 0;

	int blocks_found = 0;
	int blocks_required = ceil((file_size) / htons(super->block_size));
	if(blocks_required == 0){
		blocks_required = 1;
	}

	int start = ntohl(super->fat_start_block);
	int size = htons(super->block_size);

	while(blocks_required > blocks_found){
		void *currpoint = address + size * start + current_block * sizeof(uint32_t);

		uint32_t *fat_entry = (uint32_t*)currpoint;

		if(ntohl(*fat_entry) == 0 && blocks_found == 0){
			first_block = current_block;

			prev_block = current_block;
			blocks_found++;
		} else if(ntohl(*fat_entry) == 0 && blocks_found > 0){
			fseek(input, prev_block*sizeof(uint32_t) + start*size, SEEK_SET);
			fwrite(currpoint, sizeof(uint32_t), 1, input);

			prev_block = current_block;
            blocks_found++;
		}
		current_block++;
	}
	uint32_t endfile = htonl(0xFFFFFFFF);
	fseek(input, prev_block*sizeof(uint32_t) + start*size, SEEK_SET);
	fwrite(&endfile, sizeof(uint32_t), 1, input);

	if (blocks_required > blocks_found) { // optimally this should come beforehand in order to stop any allocation
        perror("Error no blocks left.\n");
        exit(1);
    }

    struct dir_entry_t entry = create_dir(first_block, blocks_required, file_size, file_name);
    return entry;
}

void add_dir_entry(struct superblock_t* super, void* address, int block, int block_count, char **root_names, int level, int levels, struct dir_entry_t dir_entry, FILE *input){
	void *currpoint = address + htons(super->block_size) * block; // jumps to directory

	int num_of_entries = htons(super->block_size) / sizeof(struct dir_entry_t);
    struct dir_entry_t* entries = malloc(num_of_entries * sizeof(struct dir_entry_t));

    for (int i = 0; i < block_count; i++) {
        memcpy(entries, currpoint, htons(super->block_size));

        for (int j = 0; j < num_of_entries; j++) {
       	    struct dir_entry_t* entry = &entries[j];
           	if((level == levels) && ((entry->status == 0) || (entry->status == 2) || (entry->status == 4) || (entry->status == 6))){
               	fseek(input, j*sizeof(struct dir_entry_t) + htons(super->block_size) * block, SEEK_SET);
				fwrite(&dir_entry, sizeof(struct dir_entry_t), 1, input);
				return;
            } else{
            	if(root_names[level] != NULL && strcmp((char *)entry->filename, root_names[level]) == 0){
            		level++;
            		add_dir_entry(super, address, ntohl(entry->starting_block), ntohl(entry->block_count), root_names, level, levels, dir_entry, input);
            		level = 0;
            		return;
            	}
            }
        } currpoint += htons(super->block_size);
   	}
    free(entries);
	return;
}

void add_file(struct superblock_t* super, void* address, struct dir_entry_t dir_entry, FILE *input, FILE *put_input, int file_size){
	int blocks_required = dir_entry.block_count;

	int start = ntohl(super->fat_start_block);
	int size = htons(super->block_size);

	uint32_t current_fat = dir_entry.starting_block;
	void *currpoint = NULL;

	for(int i = 0; i < blocks_required; i++){
		if (current_fat == 0xFFFFFFFF) {
            perror("Error end of file.\n");
            exit(1);
        }

        //void *currpoint_block = address + current_fat * size;

        void *buffer = malloc(size);

		fseek(put_input, i * size, SEEK_SET);
		fread(buffer, size, 1, put_input);

		fseek(input, current_fat * size, SEEK_SET);
		fwrite(buffer, size, 1, input);

		//memcpy((uint32_t *)currpoint_block, buffer, size);

		currpoint = address + start * size + current_fat * sizeof(uint32_t);
		uint32_t *fat_entry = (uint32_t*)currpoint;
		current_fat = *fat_entry;

		free(buffer);
	}
	return;
}

void deallocate_blocks(struct superblock_t* super, void* address, struct dir_entry_t *dir_entry, const char* file, int blocks_required){
	int start = ntohl(super->fat_start_block);
	int size = htons(super->block_size);
	int count = 0;

	FILE* fp = fopen(file, "r+");
	if (fp == NULL) {
        perror("Error opening file");
        exit(1);
    }

	void *currpoint = address + start * size + ntohl(dir_entry->starting_block)  * sizeof(uint32_t);
	
	uint32_t *fat_entry = (uint32_t*)currpoint;

	while (*fat_entry != -1) {
		count++;
        uint32_t next_fat_entry = *fat_entry;
        
        if (count == blocks_required) {
        	uint32_t endfile = htonl(0xFFFFFFFF);
            fseek(fp, next_fat_entry * sizeof(uint32_t) + start * size, SEEK_SET);
            fwrite(&endfile, sizeof(uint32_t), 1, fp);
        }

        if(count > blocks_required){
        	*fat_entry = 0;
       		fseek(fp, count*sizeof(uint32_t), SEEK_SET);
        	fwrite(fat_entry, sizeof(uint32_t), 1, fp);
        }
        currpoint = address + start * size + next_fat_entry * sizeof(uint32_t);
        fat_entry = (uint32_t*)currpoint;
    }

	fclose(fp);

	return;
}

struct dir_entry_t *update_entry(struct superblock_t* super, void* address, char **root_names, struct dir_entry_t *entry, const char* file, int blocks_required, int file_size){
	struct dir_entry_timedate_t entry_time;

	time_t current_time;
	struct tm *modification_time;

	setenv("TZ", "PST+8", 1);
    tzset();

    current_time = time(NULL);
    modification_time = localtime(&current_time);

	entry_time.year = (uint16_t)(modification_time->tm_year + 1900);
	entry_time.year = htons(entry_time.year);
    entry_time.month = (uint8_t)(modification_time->tm_mon + 1);
    entry_time.day = (uint8_t)(modification_time->tm_mday);
    entry_time.hour = (uint8_t)modification_time->tm_hour;
    entry_time.minute = (uint8_t)modification_time->tm_min;
    entry_time.second = (uint8_t)modification_time->tm_sec;

	entry->block_count = blocks_required;
	entry->size = file_size;
	entry->size = htonl(entry->size);
	entry->modify_time = entry_time;

	return entry;
}

void readd_dir_entry(struct superblock_t* super, void* address, int block, int block_count, char **root_names, int level, int levels, struct dir_entry_t *dir_entry, FILE *input){
	void *currpoint = address + htons(super->block_size) * block; // jumps to directory

	int num_of_entries = htons(super->block_size) / sizeof(struct dir_entry_t);
    struct dir_entry_t* entries = malloc(num_of_entries * sizeof(struct dir_entry_t));

    for (int i = 0; i < block_count; i++) {
        memcpy(entries, currpoint, htons(super->block_size));

        for (int j = 0; j < num_of_entries; j++) {
       	    struct dir_entry_t* entry = &entries[j];
           	if((level == levels) && (strcmp((char *)dir_entry->filename, (char *)entry->filename) == 0)){
               	fseek(input, j*sizeof(struct dir_entry_t) + htons(super->block_size) * block, SEEK_SET);
				fwrite(&dir_entry, sizeof(struct dir_entry_t), 1, input);
				return;
            } else{
            	if(root_names[level] != NULL && strcmp((char *)entry->filename, root_names[level]) == 0){
            		level++;
            		readd_dir_entry(super, address, ntohl(entry->starting_block), ntohl(entry->block_count), root_names, level, levels, dir_entry, input);
            		level = 0;
            		return;
            	}
            }
        } currpoint += htons(super->block_size);
   	}
    free(entries);
	return;
}

void find_more_blocks(struct superblock_t* super, void* address, int file_size, FILE *input, const char* file_name, struct dir_entry_t *dir_entry){
	uint32_t current_block = ntohl(super->fat_start_block); // should be changed to go to end of exisitng allocated blocks
	uint32_t prev_block = 0;

	int blocks_found = 0;
	int blocks_required = ceil((file_size) / htons(super->block_size));
	if(blocks_required == 0){
		blocks_required = 1;
	} blocks_required = blocks_required - dir_entry->block_count;

	int start = ntohl(super->fat_start_block);
	int size = htons(super->block_size);

	while(blocks_required > blocks_found){
		void *currpoint = address + size * start + current_block * sizeof(uint32_t);

		uint32_t *fat_entry = (uint32_t*)currpoint;

		if(ntohl(*fat_entry) == 0 && blocks_found == 0){
			prev_block = current_block;
			blocks_found++;
		} else if(ntohl(*fat_entry) == 0 && blocks_found > 0){
			fseek(input, prev_block*sizeof(uint32_t) + start*size, SEEK_SET);
			fwrite(currpoint, sizeof(uint32_t), 1, input);

			prev_block = current_block;
            blocks_found++;
		}
		current_block++;
	}
	uint32_t endfile = htonl(0xFFFFFFFF);
	fseek(input, prev_block*sizeof(uint32_t) + start*size, SEEK_SET);
	fwrite(&endfile, sizeof(uint32_t), 1, input);

	if (blocks_required > blocks_found) { // optimally this should come beforehand in order to stop any allocation
        perror("Error no blocks left.\n");
        exit(1);
    }

    return;
} 


void find_existing_file(struct superblock_t* super, void* address, int block, int block_count, char **root_names, int level, int levels, const char* file, int file_size, FILE *input){
	void *currpoint = address + htons(super->block_size) * block; // jumps to directory

	int num_of_entries = htons(super->block_size) / sizeof(struct dir_entry_t);
    struct dir_entry_t* entries = malloc(num_of_entries * sizeof(struct dir_entry_t));

    for (int i = 0; i < block_count; i++) {
        memcpy(entries, currpoint, htons(super->block_size));

        for (int j = 0; j < num_of_entries; j++) {
       	    struct dir_entry_t* entry = &entries[j];

            if(level == levels-1){
         		if(root_names[level] != NULL && strcmp((char *)entry->filename, root_names[level]) == 0){
         			int blocks_required = ceil((file_size) / htons(super->block_size));
					if(blocks_required == 0){
						blocks_required = 1;
					}
         			if(blocks_required < entry->block_count){
         				deallocate_blocks(super, address, entry, file, blocks_required);
         			} else if(blocks_required > entry->block_count){
         				find_more_blocks(super, address, file_size, input, file, entry);
         			}
         			
         			entry = update_entry(super, address, root_names, entry, file, blocks_required, file_size);
         			readd_dir_entry(super, address, ntohl(super->root_dir_start_block), ntohl(super->root_dir_block_count), root_names, level, levels, entry, input);


         			free(entries);
					return;
         		}
            } else if(root_names[level] != NULL && strcmp((char *)entry->filename, root_names[level]) == 0){
            	level++;
            	find_existing_file(super, address, ntohl(entry->starting_block), ntohl(entry->block_count), root_names, level, levels, file, file_size, input);
            	level--;
            }
        } currpoint += htons(super->block_size);
   	}
   	if(level == levels-1){
   		printf("File not found.\n");
   	}
    free(entries);
	return;
}

void diskput(int argc, char* argv[]){
	if(argc != 4){
		perror("Invalid argument quantity!\n");
		exit(1);
	}

	int fd = open(argv[1], O_RDWR);
	if (fd == -1) {
        perror("Error opening file");
        exit(1);
    }

    FILE *input = fdopen(fd, "r+");

	struct stat buffer;
	int status = fstat(fd, &buffer);
	if(status == -1){
		perror("Error with stat\n");
		exit(1);
	}

	int put_fd = open(argv[2], O_RDONLY);
	if (put_fd == -1) {
        perror("Error opening file to put in");
        exit(1);
    }

    FILE *put_input = fdopen(put_fd, "r");

	struct stat put_buffer;
	int put_status = fstat(put_fd, &put_buffer);
	if(put_status == -1){
		perror("Error with stat\n");
		exit(1);
	}

	int put_size = put_buffer.st_size;

	void* address = mmap(NULL, buffer.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (address == MAP_FAILED) {
    	perror("Error mapping file to memory");
    	exit(1);
	}

	char* prop;
	char* root_names[1024];
	int levels = 0;

	prop = strtok(argv[3], "/");
	while(prop != NULL){ 
		root_names[levels] = prop;
		prop = strtok(NULL, "/");
		levels++;
	}

	struct superblock_t* super;
	super = (struct superblock_t*) address;

	int level = 0;
	bool exists = file_exists(super, address, ntohl(super->root_dir_start_block), ntohl(super->root_dir_block_count), root_names, argv[2], level, levels);

	struct dir_entry_t entry;
	if(exists){
		level = 0;
		find_existing_file(super, address, ntohl(super->root_dir_start_block), ntohl(super->root_dir_block_count), root_names, level, levels, argv[2], put_size, input);		
	} else{
		entry = find_blocks(super, address, put_size, input, argv[2]);

		level = 0;
		add_dir_entry(super, address, ntohl(super->root_dir_start_block), ntohl(super->root_dir_block_count), root_names, level, levels, entry, input);
	} 

	add_file(super, address, entry, input, put_input, put_size);

	fclose(input);
	fclose(put_input);
	return;
}

int main(int argc, char* argv[]){
	#if defined(PART1)
		diskinfo(argc, argv);
	#elif defined(PART2)
		disklist(argc, argv);
	#elif defined(PART3)
		diskget(argc, argv);
	#elif defined(PART4)
		diskput(argc, argv);
	#else
	#	error "PART[1234] must be defined"
	#endif
		return 0;
}