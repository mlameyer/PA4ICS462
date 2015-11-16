/* 
 * File:   main.cpp
 * Author: Matt LaMeyer hu2824uo@metrostate.edu
 *
 * Created on November 3, 2015, 7:19 PM
 */

#include <stdio.h>

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
void hexDump (char *desc, void *addr, int len) ;

struct bootblock *bb;
int ClusterSize;
int FatSectors;
int RootDirectorySectorNum;
int NumRootDirSectors;
int DataStart;
short fat[100 * 512];

void rootCalculations(struct bootblock *bb)
{
    ClusterSize = bb->bytesPerSector * bb->sectorsPerCluster;
    FatSectors = bb->numberFat * bb->sectorsPerFat;
    RootDirectorySectorNum = (bb->sectorsPerFat) * (bb->numberFat) + 1;
    NumRootDirSectors = (bb->rootDirectoryEntries * 32) / bb->bytesPerSector;
    DataStart = bb->numberOfReservedSectors + FatSectors + NumRootDirSectors;
    //FirstFileSector = DataStart + ((ClusterNumber - 2) * sectorsPerCluster);
}

int calculateFileSector(int ClusterNumber, struct bootblock *bb)
{
    int fs = DataStart + ((ClusterNumber - 2) * bb->sectorsPerCluster);
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


void dump_mbr(char *name, int sectorNumber)
{
    char buffer[512];
    bios_read(sectorNumber, buffer);
    hexDump(name, buffer, 512);
    int spaceIndicator;
    
    unsigned char *line = (unsigned char*)buffer;
    
    printf("Start of Master Boot Record... \n");
    
    for(int i = 0; i < 512; i++)
    {
        if(line[i] == 0x0A)
        {
            printf("\n");
        }
        if((line[i] > 0x20) && (line[i] < 0x7f))
        {
            printf("%c", line[i]);
            spaceIndicator = 0;
        }
        else
        {
            if(spaceIndicator == 0)
                printf(" ");
            spaceIndicator = 1;
        }
    }
    
    printf("\nEnd of Master Boot Record. \n\n");
}

void printDirectoryEntry(struct directoryEntry *de)
{
    printf("filename %s \n", de->filename);
    printf("extension %s \n", de->extension);
    printf("filesize %s \n", de->filesize);
}

void dir(char *name, int sectorNumber)
{
    char buffer[512];
    struct directoryEntry *de;
    bios_read(sectorNumber, buffer);
    int index = 0;
    hexDump(name, buffer, 512);
    while(1)
    {
        de = (struct directoryEntry * ) &buffer[index];
        if(de->filename[0] == '.')
        {
            index = index + 32;
            continue;
        }
        
        de->filename[7] = 0;
        
        if(de->attribute == '\x20')
        {
            printf("%s\\%s \n", name, de->filename);
        }

        if(de->attribute == '\x10')
        {
            int newDir;
            printf("a directory \n");
            newDir = calculateFileSector(de->startingCluster, bb);
            printf("New directory %s Sector %d \n", de->filename, newDir);
            dir(de->filename, newDir);
        }

        if(de->attribute == '\x00')
        {
            break;
        }
        
        index = index + 32;
    }
}

void cat(char * path)
{
  /* you do this */
  /* path is the path to the file on the file system */
}

int main(int argc, char * argv[] )
{ 
  char buffer[512];
  char fileBuffer[512];
  struct directoryEntry * dirEntry;
  int fileSector;
  int nextCluster;

  bios_init("C:/Users/mlameyer/Documents/PA4ICS462file/pa4/pa4.img");
  bios_read(0, buffer);  // 0 is the sector number
  //hexDump("Sector 0", buffer, 512);  
  bb = (struct bootblock *) &buffer[0];
  rootCalculations(bb);
  
  dump_mbr("Master Boot Record", 0);

  for(int i = 0; i < bb->sectorsPerFat; i++)
  {
      bios_read(i + 1, (char*)&fat[i * 512]);
  }
  
  hexDump("fat", fat, 512);
  /*
  printf("Bytes per sector %x %d \n", bb->bytesPerSector, bb->bytesPerSector);
  printf("Sectors Per Cluster %x \n", bb->sectorsPerCluster);
  */ 
  dir("Root", RootDirectorySectorNum);
  fileSector = calculateFileSector(4, bb);
  
  for(int i = 0; i < 4; i++)
  {
      bios_read(fileSector + i, fileBuffer);
      hexDump("file", fileBuffer, 512);
  }
  
  //nextCluster = (short *)&fat[4*2];
  nextCluster = fat[4];
  fileSector = calculateFileSector(nextCluster, bb);
  
  for(int i = 0; i < 4; i++)
  {
      bios_read(fileSector + i, fileBuffer);
      hexDump("file", fileBuffer, 512);
  }
  
  nextCluster = fat[nextCluster];
  fileSector = calculateFileSector(nextCluster, bb);
}
