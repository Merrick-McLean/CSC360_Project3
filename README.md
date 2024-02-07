# CSC360_Project3

Project 3: Merrick McLean

Part 1: diskinfo uses fread to read the given argument file into the super structure. Since the struct already has defined sizes it automatically fits. This will provide the Superblock information by accessign the struct attributes. In order to get the FAT info, a forloop is used (looping ince for each file system block) and fseeks to the current indexed fat entry (i*4 bytes, where i is the index), the program reads the 4 bytes into a unsigned integer of size 4 bytes (uint32_t) and properly convert it (network to host long). The program then checks if it is 0, 1, or other and increment one of 3 counts respecively. Then all superblock information and the 3 counts are printed.

Part 2: In disklist, I implemented the use of a mmap to map the data of file for this and all future parts. The program puts this into a void* which can be incremented depending on what it is traversing. The program saves this address into a instance of the superblock struct as well.
The program tokenizes the subdirectories by slash so there is an array of them. A function is called which will recursively locate the proper layer and subdirectory (root directory is the first level and increases per subdirectory deep). Whenever the correct name of a subdirectory is reached, the program get its starting block and make the call again. Each time all entries are saved into an array of directory entries. Once the desiered layer is reached, it loops through the number of entires and print each one which is not empty with the desired layout.

Part 3: For diskget I follow a similar solution as I did for disklist with mmap and using a recusrive function to reach the desired level of subdirectory. This time once the program reaches the desired level and location, the names is compared to the name given in the argument until there is a match. Then a pointer starting at the top of the FAT block (and increments 4 bytes each time) gets the location of each FAT entry. Each time, the program makes a call which gives the value of the FAT entry, it goes to the block (fat_entry_value * size of block) to copy the block into the file which was created in my intialization. Each block is inserted at the end of the file and the program stops once it hits the FAT entry which 0xFFFFFFFF.

Part 4: For diskput I used multiple elements I developed in part 2 and part 3. The program makes a call first to search for the file (similar to my recursive searches in p2 and p3), if it finds a file in that location with the given name it returns true, it returns false otherwise. 
-> If this is true, the program would check whether the new file is bigger or smaller (allocating more or less if necessary) and then update certain aspects of the dir_entry (such as blocks and modification time).
-> If this is false, the program would go about a system to allocate fat entries/blocks which are currently free. The amount allocated is equal to the file_size divided by block_size. An entry with file name, size, datetime, etc. is all saved in a new entry and store that in the proper root or sub directory (using the recursive system from earlier).

After creating or updating the entry, both will pass the entry to the last call which will get the starting block and write (or overwrite) the new data into each block (traversing the fat to find each).

Any of these parts can be run by calling name of the part with a .img file and the appropriate arguments.
