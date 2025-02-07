#include <iostream>
#include <string>
#include <sstream>
#include <istream>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <stdio.h>

#include "filesystem.h"

/*

We've got these main tables to concern about the

// Offset stuff
Boot Sector and BPB Structure (page 9)
FAT32 Structure Starting at Offset 36 (page 12)

FAT32 FSInfo Sector Structure and Backup Boot Sector (pg 21)
FAT32 Byte Directory Entry Structure (pg 23)

BPB = BIOS Parameter Block
MBR = Master boot record
*/

using namespace std;

// Globals
#define FOUT stdout 

// Prototypes
char handleCommand(int,char*[],Filesystem*);

int main (int argc, char* argv[])
{
	char input = '\0';
	
	// Checks for correct usage
	if (argc < 2)
	{
		fprintf(FOUT, "%s\n", "No image file was provided");
		fprintf(FOUT, "%s\n", "Usage: 'fatmod <image_name>'");
		exit(0);
	}
	
	Filesystem *fs = new Filesystem(argv[1]);
	cout << "FAT32 Filesystem" << endl;
	cout << "Please enter your command" << endl;
	// Keep getting input until the user exits out
	while (input != 'x')
    {
      fs->isChangeDirectory = false;
      input = handleCommand(argc, argv,fs);
    }
    
    return 0;
}

char handleCommand(int argc,char *argv[], Filesystem* fs)
{
    char type_of_command = 'a';
   	int i = 0; 
    string userCommand;
    string commandBuffer;
    cout << "[fat32.img]> ";
    getline(cin, userCommand); // Gets users input
    string userArgs[5];
    
    istringstream commandStream(userCommand);
    while( commandStream >> commandBuffer)
    {
    	userArgs[i] = commandBuffer;
    	i++;
    }
    
    // All userArgs[1] are file names, nothing, or directory names
    // so as per the writeup if they contain a / we print out a message
    size_t searchFor = userArgs[1].find("/");
  	if (searchFor != string::npos){
  		cout << "File, directory names and entry names may not contain a /" << endl;
  	}
    else if (userArgs[0].compare("fsinfo") == 0)
	{
		fs->fsinfo();
	}
	else if (userArgs[0].compare("open") == 0)
	{
	 	if (userArgs[2] == ""){
			printf("%s\n", "Wrong amount of arguments supplied");
	 	}else{
	 		fs->openFile(userArgs[1],userArgs[2]);
	 	}
		
	}
	else if (userArgs[0].compare("close") == 0)
	{
		if (userArgs[1] == ""){
			printf("%s\n", "Wrong amount of arguments supplied");
		}else{
			fs->closeFile(userArgs[1]);
		}
		
	}
	else if (userArgs[0].compare("create") == 0)
	{
		if (userArgs[1] == ""){
			printf("%s\n", "Wrong amount of arguments supplied");
		}else
		{
			fs->createFile(userArgs[1]);
		}
		
	}
	else if (userArgs[0].compare("read") == 0)
	{
		if (userArgs[3] == ""){
			printf("%s\n", "Wrong amount of arguments supplied");
		}
		else
		{
			fs->Read(userArgs[1],atoi(userArgs[2].c_str()),atoi(userArgs[3].c_str()));	
		}
		
	}
	else if (userArgs[0].compare("write") == 0)
	{
		if (userArgs[3] == ""){
			printf("%s\n", "Wrong amount of arguments supplied");
		}else
		{
			fs->Write(userArgs[1],atoi(userArgs[2].c_str()),userArgs[3]);	
		}
	}
	else if (userArgs[0].compare("rm")== 0)
	{
		if (userArgs[1] == "")
		printf("%s\n", "Wrong amount of arguments supplied");
		else
		{
			fs->removeFile(userArgs[1]);
		}
	}
	else if (userArgs[0].compare("cd") == 0)
	{
		
		if (userArgs[1] == ""){
			printf("%s\n", "Wrong amount of arguments supplied");
		}else
		{
			fs->isChangeDirectory = true;
			fs->changeDirectory(userArgs[1]);	
		}
	}
	else if (userArgs[0].compare("ls") == 0)
	{
		if (userArgs[1] == ""){
			printf("%s\n", "Wrong amount of arguments supplied");
		}else{
			// Lists out the directory NYI
			fs->listDirectory(userArgs[1]);
		}
	}
	else if (userArgs[0].compare("mkdir") == 0)
	{
		if (userArgs[1] == ""){
			printf("%s\n", "Wrong amount of arguments supplied");
		}else{
			fs->makeDirectory(userArgs[1]);
		}
	}
	else if (userArgs[0].compare("rmdir") == 0)
	{
		if (userArgs[1] == ""){
			printf("%s\n", "Wrong amount of arguments supplied");
		}else{
			fs->removeDirectory(userArgs[1]);
		}
	}

	else if (userArgs[0].compare("size") == 0)
	{
		if (userArgs[1] == ""){
			printf("%s\n", "Wrong amount of arguments supplied");
		}else
		{
			fs->entrySize(userArgs[1]);	
		}
	}
	else if (userArgs[0].compare("undelete") == 0)
	{
		if (userArgs[1] != ""){
			printf("%s\n", "Wrong amount of arguments supplied");
		}else{
			fs->Undelete();
		}
	}
    else if (userArgs[0].compare("quit") == 0)
    {
        type_of_command = 'x';
    }
    // Testing purposes
    
    else if(userArgs[0].compare("show") == 0)
    {
    	fs->displayVectorContents();
    }

    else{
    	fs->findRootDirectory();
    }
    
    return type_of_command;

}


















