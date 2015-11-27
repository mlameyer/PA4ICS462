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

void printSector(unsigned char *line)
{
    int spaceIndicator;
    
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
    
    printf ("\n");
}

void dump_mbr(char *name, int sectorNumber)
{
    char buffer[512];
    bios_read(sectorNumber, buffer);
    //hexDump(name, buffer, 512);
    printf ("%s:\n", name);
    
    printSector((unsigned char*)buffer);
    
    printf("End of %s. \n\n", name);
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
        
        de->filename[7] = 0;
        
        if(de->attribute == '\x20')
        {
            printf("%s\\%s Cluster: %i, Size: %i\n", name, de->filename, de->startingCluster, de ->filesize);
        }

        if(de->attribute == '\x10')
        {
            int newDir;
            newDir = calculateFileSector(de->startingCluster, bb);
            printf("Contents of Directory %s, Starting Sector number: %d\n", de->filename, newDir);
            dir(de->filename, newDir);
        }

        if(de->attribute == '\x00')
        {
            break;
        }
        
        index = index + 32;
    }
    
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
            //printf("%s\\%s Cluster: %i, Size: %i\n", name, de->filename, de->startingCluster, de ->filesize);
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
    //printSector((unsigned char*)path);
}

void dumpfilefromcluster(int clusterNumber, int size)
{
    int fileSector = 0;
    char filebuffer[512];
    int numSectors = size / 512;
    int i = 0;
    int nextCluster;
    nextCluster = clusterNumber;
    while(1)
    {
        fileSector = calculateFileSector(nextCluster, bb);
  
        for(i = 0; i < 4; i++)
        {
            bios_read(fileSector + i, filebuffer);
            hexDump("file", filebuffer, 512);
            //printf("File Sector %i: \n", fileSector + i);
            //cat(fileBuffer);
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
      bios_read(i + 1, (char*)&fat[i * 256]);
  }
  
  //hexDump("fat", fat, 1024);
  /*
  printf("Bytes per sector %x %d \n", bb->bytesPerSector, bb->bytesPerSector);
  printf("Sectors Per Cluster %x \n", bb->sectorsPerCluster);
  */ 
  //dir("Root", RootDirectorySectorNum);
  //fileSector = calculateFileSector(4, bb);
  catdir("root", RootDirectorySectorNum, "FOLDER1","FILE1");
  //dumpfilefromcluster(3,298142);
  return 0;
/*
  for(int i = 0; i < 4; i++)
  {
      bios_read(fileSector + i, fileBuffer);
      //hexDump("file", fileBuffer, 512);
      printf("File Sector %i: \n", fileSector + i);
      cat(fileBuffer);
  }
  
  //nextCluster = (short *)&fat[4*2];
  nextCluster = fat[4];
  fileSector = calculateFileSector(nextCluster, bb);
  
  for(int i = 0; i < 4; i++)
  {
      bios_read(fileSector + i, fileBuffer);
      //hexDump("file", fileBuffer, 512);
      printf("File Sector %i: \n", fileSector + i);
      cat(fileBuffer);
  }
  
  nextCluster = fat[nextCluster];
  fileSector = calculateFileSector(nextCluster, bb);
 */ 
}
