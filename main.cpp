/* 
 * File:   main.cpp
 * Author: Matt LaMeyer hu2824uo@metrostate.edu
 *
 * Created on November 3, 2015, 7:19 PM
 */

#include <stdio.h>
#include <string.h>

FILE *file_ptr;

#pragma pack(1)
struct bootblock {
 char jmp[3];
 char oem[8];
 short bytesPerSector;
 unsigned char sectorsPerCluster;
 short numberOfReservedSectors;
 unsigned char numberFat;
 short rootDirectoryEntries;
 short smallSectors;
 char mediaDescriptor;
 short sectorsPerFat;
};

#pragma pack(1)
struct directoryEntry {
 char filename[8];
 char extension[3];
 char attribute;
 char reserved[10];
 unsigned short time; 
 unsigned short date; 
 short startingCluster; 
 int filesize;
};


void bios_init(char * name);
void bois_close(void);
void bios_read(int number, char * sector);
void bois_write(int number, char * sector);
void hexDump (char *desc, void *addr, int len);
void dumpfilefromcluster(int clusterNumber, int size);
void cat(char *path);
char *strcat(char *dest, const char *src);

/*
 Boot block structure
 */
struct bootblock *bb;
int ClusterSize; //cluster size in bb struct
int FatSectors; //FatSectors size in bb struct
int RootDirectorySectorNum; //RootDirectorySectorNum size in bb struct
int NumRootDirSectors; //NumRootDirSectors size in bb struct
int DataStart; //DataStart size in bb struct
short fat[100 * 512];

//Calculates root and directories for locating files and folders in root
void rootCalculations(struct bootblock *bb)
{
    ClusterSize = bb->bytesPerSector * bb->sectorsPerCluster; //calculate cluster size
    FatSectors = bb->numberFat * bb->sectorsPerFat; //calculate FatSectors
    RootDirectorySectorNum = (bb->sectorsPerFat) * (bb->numberFat) + 1; //calculate RootDirectorySectorNum
    NumRootDirSectors = (bb->rootDirectoryEntries * 32) / bb->bytesPerSector; //calculate NumRootDirSectors
    DataStart = bb->numberOfReservedSectors + FatSectors + NumRootDirSectors; //calculate DataStart
}

// Calculate File sector starting location
int calculateFileSector(int ClusterNumber, struct bootblock *bb)
{
    int fs = DataStart + ((ClusterNumber - 2) * bb->sectorsPerCluster);//(cluster number - 2) * sectorsPerCluster
    return fs;
}

void bios_init(char * name)
{
 file_ptr = fopen(name, "rw"); 
 if (file_ptr == NULL)
 {
    printf("bios_init: failed \n");
    return;
 }
}

void bois_close(void)
{
 fclose(file_ptr);
}


/* reads 512 bytes from the disk image */
void bios_read(int number, char * sector)
{
  fseek(file_ptr, number*512,SEEK_SET);
  fread(sector, 1, 512, file_ptr); 
}



/* thank you stackoverflow */
/* http://stackoverflow.com/questions/7775991/how-to-get-hexdump-of-a-structure-data */

void hexDump (char *desc, void *addr, int len) 
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Output description if given.
    if (desc != NULL)
        printf ("%s:\n", desc);

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.
            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf ("  %s\n", buff);
}

//Print contents of sector in a readable format in place of hexdump
void printSector(unsigned char *line)
{
    //boolean to indicate if end of word has been reached to add space before next word
    int spaceIndicator; 
    
    //loop through buffer and print contents of sector
    for(int i = 0; i < 512; i++)
    {
        //if character is new line print new line
        if(line[i] == 0x0A)
        {
            printf("\n");
        }
        //if character is a standard readable character print it 
        if((line[i] > 0x20) && (line[i] < 0x7f))
        {
            printf("%c", line[i]);
            spaceIndicator = 0;
        }
        //else print a space once and do not repeat it if there are other characters
        //but not within 0x20 and 0x7F
        else
        {
            if(spaceIndicator == 0)
                printf(" ");
            spaceIndicator = 1;
        }
    }
    
    printf ("\n");
}

//Reads and prints master boot record
void dump_mbr(char *name, int sectorNumber)
{
    char buffer[512]; //buffer for sector
    bios_read(sectorNumber, buffer); // read sector
    printf ("%s:\n", name); //print name of sector
    printSector((unsigned char*)buffer); //call to print sector
    printf("End of %s. \n\n", name); //print end of sector read
}

//prints directory entry
void printDirectoryEntry(struct directoryEntry *de)
{
    printf("filename %s \n", de->filename);
    printf("extension %s \n", de->extension);
    printf("filesize %s \n", de->filesize);
}

//prints root directory contents 
void dir(char *name, int sectorNumber)
{
    char buffer[512]; //sector buffer
    char path[1024]; //path variable
    struct directoryEntry *de; //struct directory entry
    bios_read(sectorNumber, buffer); //read current sector
    int index = 0;//index position of next sector
    printf ("Contents of %s:\n", name);//print contents of current sector
    printSector((unsigned char*)buffer);//call to print current sector
    //hexDump(name, buffer, 512);
    /*
     loop through cluster and print folder and file structures
     */
    while(1)
    {
        de = (struct directoryEntry * ) &buffer[index];//get next directory entry 
        //if directory contains "." increment to next entry for correct name
        if(de->filename[0] == '.')
        {
            index = index + 32;
            continue;
        }
        
        de->filename[7] = 0;
        //if directory entry is 0x20 print Cluster size and cluster name. Also
        // concat folder and file path for function cat
        if(de->attribute == '\x20')
        {
            printf("%s\\%s Cluster: %i, Size: %i\n", name, de->filename, de->startingCluster, de ->filesize);
            strcpy(path, name);
            strcat(path, "\\");
            strcat(path, de->filename);
        }
        //if directory entry is 0x10 print directory contents
        if(de->attribute == '\x10')
        {
            int newDir;
            newDir = calculateFileSector(de->startingCluster, bb);
            printf("Contents of Directory %s, Starting Sector number: %d\n", de->filename, newDir);
            dir(de->filename, newDir);
        }
        //if directory entry is 0x00 break out of loop
        if(de->attribute == '\x00')
        {
            break;
        }
        //increment to next index position
        index = index + 32;
    }
    //print file path and contents of folder and directory using cat function
    cat(path);
    printf ("End of %s:\n\n", name);
}

void catdir(char *name, int sectorNumber, char * folder, char * filename)
{
    char buffer[512];
    struct directoryEntry *de;
    bios_read(sectorNumber, buffer);
    int index = 0;
    printf ("Contents of %s:\n", name);
    printSector((unsigned char*)buffer);
    //hexDump(name, buffer, 512);
    while(1)
    {
        de = (struct directoryEntry * ) &buffer[index];
        if(de->filename[0] == '.')
        {
            index = index + 32;
            continue;
        }

        if(de->attribute == '\x20')
        {
            int i, j;
            
            i = strncmp(de->filename, filename, strlen(filename));
            j = strncmp(name, folder, strlen(folder));
            if((i == 0) && (j == 0))
            {
                dumpfilefromcluster(de->startingCluster, de->filesize);
            }
        }

        if(de->attribute == '\x10')
        {
            int newDir;
            newDir = calculateFileSector(de->startingCluster, bb);
            printf("Contents of Directory %s, Starting Sector number: %d\n", de->filename, newDir);
            catdir(de->filename, newDir, folder, filename);
        }

        if(de->attribute == '\x00')
        {
            break;
        }
        
        index = index + 32;
    }
    
    printf ("End of %s:\n\n", name);
}

void cat(char *path)
{
    char * folder; //folder name
    char * filename; // file name
    char temp[30];

    printf("-->Path: %s \n",path); //prints path
    memcpy(temp, path, strlen(path));//copy path to temp
    printf("-->Path: %s \n",temp); //print temp
    
    //break temp down to folder and filename to pass to catdir function
    if (strchr(temp,'\\')){
    folder = strtok(temp,"\\");
    filename = strtok(NULL,"\\");
    }
    else{
    filename = temp;
    folder = "\\";
    }
    printf("Folder: %s \n",folder);
    printf("Filename: %s \n",filename);
    
    //call to catdir to print contents of files provided by folder and filename
    catdir("root", RootDirectorySectorNum, folder,filename);
}

//loops and prints through a cluster
void dumpfilefromcluster(int clusterNumber, int size)
{
    int fileSector = 0; //file sector position
    char filebuffer[512]; //file buffer
    int numSectors = size / 512; //number of sectors per cluster
    int i = 0;
    int nextCluster; //next cluster starting position
    nextCluster = clusterNumber;
    
    //loop through each cluster and print contents
    while(1)
    {
        fileSector = calculateFileSector(nextCluster, bb);//get next file sector
        
        //loop through file sector
        for(i = 0; i < 4; i++)
        {
            //read file sector
            bios_read(fileSector + i, filebuffer);
            //print file sector
            printSector((unsigned char*)filebuffer);
            //hexDump("file", filebuffer, 512);
            numSectors--;
            if(numSectors < 0) 
                break;
        }
        if(numSectors < 0) 
            break;
        nextCluster = fat[nextCluster];
    }
    
}

int main(int argc, char * argv[] )
{ 
  char buffer[512];//buffer for bios_read
  
  //read initial boot file
  bios_init("C:/Users/mlameyer/Documents/PA4ICS462file/pa4/pa4.img");
  bios_read(0, buffer);  // 0 is the sector number
  //hexDump("Sector 0", buffer, 512);  
  //get boot block and save structure for calculations
  bb = (struct bootblock *) &buffer[0];
  //with the boot block stored call rootCalculations to get calculations
  rootCalculations(bb);
  
  //call to print contents of master boot record
  dump_mbr("Master Boot Record", 0);
  
  //loop through sectors and read contents 
  for(int i = 0; i < bb->sectorsPerFat; i++)
  {
      bios_read(i + 1, (char*)&fat[i * 256]);
  }
  //start with root directory and read through entire root directory file and all contents
  dir("Root", RootDirectorySectorNum);

  return 0;
}
