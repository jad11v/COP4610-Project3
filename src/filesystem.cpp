#include "filesystem.h"
#include "directory.h"
#include <vector>

// FATENTRY
/*
	Pass in the cluster number and it finds the FAT entry
	
	Example implementation -
	   FatEntry FATEntry = fs->findFatEntry(2);
    	cout << FATEntry.FATOffset << endl;
    	cout << FATEntry.FATsecNum << endl;
    	
    clusterNumber 2 should be the root
*/
FatEntry Filesystem::findFatEntry(uint32_t clusterNumber )
{
	FatEntry entry;
	uint32_t fatOffset = clusterNumber * 4;

	div_t result;

	result = div(fatOffset,BPB_BytsPerSec);
	/*FATsecNum is the sector number of the FAT sector that contains the entry 
	  for cluster N in the first FAT. If you want the sector number in the second 
	  FAT, you add FATSz to ThisFATSecNum; for the third FAT, you add 2*FATSz, 
	  and so on.
	*/
	entry.FATsecNum = BPB_ResvdSecCnt + (fatOffset / BPB_BytsPerSec );
	entry.FATOffset = result.rem;

	return entry;
}


// FILESYSTEM

/*
	Constructor for filesystem
*/
Filesystem::Filesystem(const char* name)
{
	fname = name;
	init();
}

/*
	Finds the first sector based on the cluster we pass in
*/
int Filesystem::findFirstSectorOfCluster(int clusterIndex){
	// Contains the local sector we're reading in NYI
	return ((clusterIndex - 2) * BPB_SecPerClus) + FirstDataSector;
}


/* Called by constructor to set up required fields
   for Filesystem
*/
void Filesystem::init()
{
	// Get the file size first
	workingDirectory = "Root Directory";
	getFileSize();
	lastIFileLocation = 0;
	filetoRemove = "";
	int offset = 0;
	unsigned len = fileSize; // Length of the file reading
	image_fd = open(fname, O_RDWR);
	if (image_fd < 0)
	{
		cout << "error while opening the file" << endl;
		exit(EXIT_FAILURE);
	}
	fdata = (uint8_t*)mmap(0, len, PROT_READ, MAP_PRIVATE, image_fd, offset);
	
	BPB_RootClus = parseInteger<uint32_t>(fdata + 44);
	BPB_NuMFATs = parseInteger<uint8_t,2>(fdata + 16 );
	BPB_BytsPerSec = parseInteger<uint16_t, 512, 1024, 2048, 4096>(fdata+ 11);
	BPB_SecPerClus = parseInteger<uint8_t, 1, 2, 4, 8, 16, 32, 64, 128> (fdata + 13);
	BPB_FATz32 = parseInteger<uint32_t>(fdata + 36);
	BPB_TotSec32 = parseInteger<uint32_t>(fdata + 32);
	BPB_ResvdSecCnt = parseInteger<uint32_t>(fdata + 14);

	bytesPerCluster = BPB_BytsPerSec * BPB_SecPerClus;
	
	currentClusterLocation = 2;
	// Now we read in the root directory
	findRootDirectory();
	//openImage();
}

/*
	Finds the file passed in by the user in fatmod.cpp and
	stores the size in fileSize
*/
void Filesystem::getFileSize(){
  FILE * file;

  file = fopen (fname,"rb");
  // If file not found
  if (file==NULL){
  	perror ("Error opening file");
  }
  else
  {
    fseek(file, 0, SEEK_END); // Goes through the file
    fileSize = ftell(file);
    fclose (file); // Close file we don't need it anymore
  }
}

void Filesystem::findRootDirectory()
{
	FirstDataSector = BPB_ResvdSecCnt + (BPB_NuMFATs * BPB_FATz32);
	firstSectorClusterRD = findFirstSectorOfCluster(BPB_RootClus);
	//fprintf(stdout,"First Sector of First RD cluster %u ",firstSectorClusterRD);
	
	// Gets the FATEntry for the location of the next Root Directory Cluster
	FATEntryRCluster = this->findFatEntry(2);

	//The second cluster number of the root directory. 
	uint32_t secondRootCluster = parseInteger<uint32_t>(fdata + FATEntryRCluster.FATOffset + FATEntryRCluster.FATsecNum * BPB_BytsPerSec);
	//fprintf(stdout,"Next RD cluster value %x\n", secondRootCluster);
	

	//First sector of the second cluster of the root directory.
	secondSectorClusterRD = findFirstSectorOfCluster(secondRootCluster);
	// Gets the FATEntry for the location of the next Root Directory Cluster(EOC)
	//FatEntry temp = this->findFatEntry(secondRootCluster);

	//uint32_t EoC = parseInteger<uint32_t>(fdata + temp.FATOffset + temp.FATsecNum * BPB_BytsPerSec);
	//fprintf(stdout,"EOC Marker hit %x\n", EoC);
	getRootDirectoryContents(firstSectorClusterRD);
	getRootDirectoryContents(secondSectorClusterRD);

}

void Filesystem::getRootDirectoryContents(int FirstDataSector)
{

	string recordName;
	fileRecord record;
	
	record.currentFolder = "root";
	cout << "cluster sector"<<FirstDataSector << endl;
	//number of records, had to divide by 32, because we were reading too far.
	for(int i = 0; i < BPB_BytsPerSec/32; i++)
		{
			recordName="";
			// If it's a long file name, skip over the record.
			if((int)*(fdata + (FirstDataSector * BPB_BytsPerSec) + i * 32 + 11) == 15)continue;	
			//if the record is empty, then break, because we have reached the end of the sector, or cluster? does it really matter?
			if((int)*(fdata + (FirstDataSector * BPB_BytsPerSec) + i * 32 + 0) == 0)break;	
			for (int j = 0; j < 11; j++)
			{
				//fprintf(stderr,"inner counter index: %d\n", j);
				//fprintf(stderr,"name byte value: %d, at index: %d ", nameByte,j); 	
				record.name[j] = (char)parseInteger<uint8_t>(fdata + (FirstDataSector * BPB_BytsPerSec) + i * 32 + j);
				fprintf(stdout,"%c", record.name[j]);
				
			}
			cout << " ";
			//fprintf(stderr,"outer counter index: %d\n", i);
			if ((int)record.name[1] < 10 )continue;
			
			//refer to the filesystem spec instead of the dumb slides, the offsets were wrong, and were ORing time stamps.
			record.attr = parseInteger<uint8_t>(fdata + (FirstDataSector * BPB_BytsPerSec) + i * 32 + 11);
			record.highCluster = parseInteger<uint16_t>(fdata + (FirstDataSector * BPB_BytsPerSec) + i * 32 + 20);
			record.lowCluster = parseInteger<uint16_t>(fdata + (FirstDataSector * BPB_BytsPerSec) + i * 32 + 26);
			
			//we shift 16 bits for the or, because we are making room foor the bits in lowCluster for the addition(draw it out kid)
			record.highCluster <<= 16;
			record.fClusterLocation = record.highCluster | record.lowCluster;
			
			// Gets the number that we need to pass into findFirstSectorOfCluster
			//fClusterLocation = record.highCluster + record.lowCluster;
			
			// Get the sector for the contents of the file to read from it
			//int sectorToPass = findFirstSectorOfCluster(record.fClusterLocation);
			// Reads the file data
			//fileHandler.getFileData(sectorToPass, fdata);
			
			fprintf(stdout,"Entry Type: %x ", record.attr);
			fprintf(stdout,"next cluster location: %d", record.fClusterLocation);
			
			cout << endl;
			record.currentFolder = workingDirectory;
			files.push_back(record); 
			
			
		}

}


/*Pass this function a cluster number, it will find the first data sector for the cluster
as well as check to see if there is another cluster associated with it in the FAT
Call function to read from data sector in this function.
*/

void Filesystem::findDirectoriesForCluster(int clusterIndex, int toDo)
{
	
	int dataSector;
	uint32_t nextCluster;
	FirstDataSector = BPB_ResvdSecCnt + (BPB_NuMFATs * BPB_FATz32);
	while(clusterIndex != 268435455)
	{
		
		dataSector = findFirstSectorOfCluster(clusterIndex);
		// Gets the FATEntry for the location of the next directory cluster
		FATEntryRCluster = this->findFatEntry(clusterIndex);
		//The second cluster number of the root directory. 
		nextCluster = parseInteger<uint32_t>(fdata + FATEntryRCluster.FATOffset + FATEntryRCluster.FATsecNum * BPB_BytsPerSec);
		clusterIndex = nextCluster;
		//this will be called for as many additional clusters there are for a directory.
		if(toDo == 0)
		PrintCurrentDirectory(dataSector);
		if(toDo == 1)
		readFilesystemforFile(dataSector,filetoRemove);
		if(toDo == 2)
		PrintCurrentDirectory(dataSector, true);
		
	}	
	
	fprintf(stdout,"EOC Marker hit %x\n", clusterIndex);

	
}

//this function gets the cluster number for a directory from the vector, and then calls the function to get the data sector and future clusters for the sectors.
bool Filesystem::getDirectoryClusterNumber(string directory)
{
	string recordName;
	bool foundRecord = false;
	for(unsigned int i = 0; i < files.size(); i++)
		{
			// If direcory
			if((int)files[i].attr == 16 )
			{
				// Reset recordName
				recordName = "";
				
				for(int j = 0; j < 11; j++)
				{
					char readInChar = files[i].name[j];
					if(readInChar != ' ')
						recordName.push_back(readInChar);
				}	
				
				// Trims and converts to uppercase the record and directoryname
				recordName = normalizeToUppercase(recordName, ' ');
				directory = normalizeToUppercase(directory, '.');
				
				if(recordName == directory)
				{
					findDirectoriesForCluster(files[i].fClusterLocation, 0);
					foundRecord = true;
					break;
				}
			}
	}
	return foundRecord;

}
/*This function takes in the data sector from a directory cluster as an argument, and then prints out the records in that cluster
this funciton is called n amount of times, where n is the amount of clusters associated with a directory.*/
void Filesystem::PrintCurrentDirectory(int directoryDataSector, bool store)
{
	fileRecord dirRecord;
	for(int i = 0; i < BPB_BytsPerSec/32; i++)
		{
		
		// If it's a long file name, skip over the record.
			if((int)*(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 11) == 15)continue;	
			//if the record is empty, then break, because we have reached the end of the sector, or cluster? does it really matter?
			if((int)*(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 0) == 0)break;	
			for (int j = 0; j < 11; j++)
			{
				//fprintf(stderr,"inner counter index: %d\n", j);
				//fprintf(stderr,"name byte value: %d, at index: %d ", nameByte,j); 	
				dirRecord.name[j] = (char)parseInteger<uint8_t>(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + j);
				fprintf(stdout,"%c", dirRecord.name[j]);
				
			}
			cout << " ";
			//fprintf(stderr,"outer counter index: %d\n", i);
			if ((int)dirRecord.name[1] < 10 )continue;
			
			//refer to the filesystem spec instead of the dumb slides, the offsets were wrong, and were ORing time stamps.
			dirRecord.attr = parseInteger<uint8_t>(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 11);
			dirRecord.highCluster = parseInteger<uint16_t>(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 20);
			dirRecord.lowCluster = parseInteger<uint16_t>(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 26);
		 
			//we shift 16 bits for the or, because we are making room foor the bits in lowCluster for the addition(draw it out kid)
			dirRecord.highCluster <<= 16;
			dirRecord.fClusterLocation = dirRecord.highCluster | dirRecord.lowCluster;
			if(store == true)
			files.push_back(dirRecord);
			
			cout << endl;
	}
}

/*
	Checks if the directory exists in the current working directory
*/
bool Filesystem::directoryExistsAndChangeTo(string directoryName){
	bool dirFound = false;
	/*
		We must note that the directory name the user enters may just be RED 
		(no spaces) but the stored value may contain spaces so we must know when
		to ignore that.
	*/
	for(unsigned int i = 0; i < files.size(); i++)
	{
		string recordName = convertCharNameToString(i, 11);
		
		// Go through file name one character at a time
		// and add its name to the recordName
		// for(int j = 0; j < 11; j++)
		// {
		// 	char readInChar = files[i].name[j];
		// 	recordName.push_back(readInChar);
		// }	

		cout << "Record Name: " << recordName << endl;
		// cout << "Record Name: " << recordName.length() << endl;
		
		// //fprintf()
		// cout << "Directory Name: " << directoryName << endl;
		// cout << "Directory Name: " << directoryName.length() << endl;

		// Trims and converts to uppercase the record and directoryname
		recordName = normalizeToUppercase(recordName, ' ');
		directoryName = normalizeToUppercase(directoryName, '.');
		
		// If directory and recordname = directorName
		if(recordName == directoryName && files[i].attr == 16)
		{
			//currentClusterLocation = files[i].fClusterLocation;
			// Now we check if the directorys .. is the same as the current
			// directory we are in only do if not in home directory
			// int x = i;
			// while(1 && x != 0){
			// 	x--;
			// 	string fileName = convertCharNameToString(x, 11);
			// 	cout << fileName << endl;
			// 	if(fileName == ".."){
			// 		// If the directory we want to change into has the parent directory
			// 		// as the curren directory were in then were good oherwise were not
			// 		if(files[x].fClusterLocation != currentClusterLocation){
			// 			return false;
			// 		}
			// 		break;
			// 	}
			// }
			lastIFileLocation = 1;
			
			dirFound = true; // Directories found
			findDirectoriesForCluster(files[i].fClusterLocation, 2);
			break;
		}
	
	}
	return dirFound;
}

/*
    Creates a directory with the passed in directoryName,
    also checks if the directory already exists in the current
    working directory
    
    NYI
*/
void Filesystem::makeDirectory(string directoryName){
    // Check if the directory already exists
    if(directoryExists(directoryName) == -1){
    	cout << "The directory " << directoryName << " already exists." << endl;
    }
    // Otherwise we create the directory
    else{
    	
    }
}

/*
    Removes a directory with the passed in directoryName,
    also checks if the directory already exists in the current
    working directory
    
    NYI
*/
void Filesystem::removeDirectory(string directoryName){
    // Check if the directory already exists
    if(directoryExists(directoryName) == -1){
    	cout << "The directory " << directoryName << " already exists." << endl;
    }
    // Otherwise we remove the directory
    else{
    	
    }
}

/*
	Changes the directory and checks if it exists.
	
	TODO NYI - Need to check if directory would be transformed into version of what
	we are comparing it against, SEE FAT documentation.
*/
void Filesystem::changeDirectory(string directoryName)
{
	// If the directory exists (pretty self explanatory)
	if(directoryExistsAndChangeTo(directoryName))
	{
		cout << "Working directory changed to " << directoryName  << endl;
		workingDirectory = directoryName;
	}
	else
	{
		cout << "Directory " << directoryName << " was not found" << endl;
		return;
	}
	
		//cout <<"Vector Size: " <<files.size() << endl;
}

/*
	Checks if the directory exists in the current working directory
	Type is the flag to check to see if what was passed in is a file
	or a directory. 
*/
int Filesystem::directoryExists(string directoryName, int type){
	int dirPos = -1;
	
	// Will return the position of the . found in the files array if the file
	// or directory looking for isn't found will return -1
	
	// unsigned int v;
	// cout << "last IFile " << lastIFileLocation << endl;
	// // Go upwards till we find a ..
	// for(v = lastIFileLocation; convertCharNameToString(v, 11) != ".."  && v != 0; v--){
	// 	cout << convertCharNameToString(v, 11) << endl;
	// }
	// cout << "v is " << v << endl;
	/*
		We must note that the directory name the user enters may just be RED 
		(no spaces) but the stored value may contain spaces so we must know when
		to ignore that.
	*/
	for(unsigned int i = 0; i < files.size(); i++)
	{
		/*
			To check if a diretory exists we are checking our current working
			folder. The way we do that is go through all our files till we find
			the occurence of it. The one issue we have we this is it will only
			find the first occurence so if multiple folders exist with the same
			name things are going to be a bit haywire.
			
			I have tried a way around which is to take the index of the last value we 
			cd'd into and then going up to find it's . and down but this will only
			fix non cd issues.
		*/
		// Go through file name one character at a time
		// and add its name to the recordName
		string recordName = convertCharNameToString(i, 11);
		
		// Once we find a . we're done itterating through the folder
		if(recordName == "."){
			dirPos = -1;
			break;
		}

		// cout << "Record Name: " << recordName << endl;
		// cout << "Record Name: " << recordName.length() << endl;
		
		// //fprintf()
		// cout << "Directory Name: " << directoryName << endl;
		// cout << "Directory Name: " << directoryName.length() << endl;

		// Trims and converts to uppercase the record and directoryname
		recordName = normalizeToUppercase(recordName, ' ');
		directoryName = normalizeToUppercase(directoryName, '.');
		
		int propertyType;
		switch(type){
			// 0 is folder
			case 0:
				propertyType = 16;
			break;
			// If a file
			case 1:
				propertyType = 32;
			break;
			default:
				propertyType = 16;
			break;
		}
		
		// If director and recordname = directorName
		if(recordName == directoryName && (int)files[i].attr == propertyType)
		{
			// FIX ME
			dirPos = 1; // Directories found
			break;
		}
	
	}
	return dirPos;
}

/*
	Takes the index to read from and the number of values and returns it 
	as a string
*/
string Filesystem::convertCharNameToString(unsigned int index, int numValuesToAddTogether = 11){
	string recordName = "";
	for(int j = 0; j < 11; j++)
	{
		char readInChar = files[index].name[j];
		recordName.push_back(readInChar);
	}	
	return recordName;
}

/*
	Lists directories out.
*/
void Filesystem::listDirectory(string dir_name)
{
	// If we didn't find the directory
	if(!getDirectoryClusterNumber(dir_name)){
		cout << "Directory " << dir_name << " not found." << endl;
	}
}

/*
	Prints out file system information
*/
void Filesystem::fsinfo()
{
	fprintf(stdout,"%u Root Cluster\n",BPB_RootClus);
	fprintf(stdout,"%u Bytes per sector\n",BPB_BytsPerSec);
	fprintf(stdout,"%u Sectors per cluster\n",BPB_SecPerClus);
	fprintf(stdout,"%u Toal sectors\n",BPB_TotSec32);
	fprintf(stdout, "%u Number of FATs\n", BPB_NuMFATs);
	fprintf(stdout, "%u Sectors per FAT\n", BPB_FATz32);
}

void Filesystem::openFile(string file_name, string mode)
{

	fileTable[file_name] = mode;

}



void Filesystem::removeFile(string file)
{
	filetoRemove = file;
	int dotClusterIndex = directoryExists(filetoRemove,1);
	 int dotCluster = files[dotClusterIndex].fClusterLocation;
	 if(dotClusterIndex != -1)
	 {
   		for(int i = 0; 0 < files.size(); i++)
   		{
   			string compare = "";
   			compare = convertCharNameToString(i);
   			if (compare == file && files[i].currentFolder == workingDirectory)
   				files[i].name[0] = 229;
   		}
   		cout << "The file " << filetoRemove << "  exists in the current directory." << endl;
     	findDirectoriesForCluster(dotCluster,1);
	 }
    // Otherwise we didnt find the directory
    else
    {
    	cout << "File " << file << " not found" << endl;
    }
	
}


void Filesystem::readFilesystemforFile(int directoryDataSector,string filetoRemove)
{
	fileRecord dirRecord;
	
	
	for(int i = 0; i < BPB_BytsPerSec/32; i++)
		{
			string recordName = "";
		// If it's a long file name, skip over the record.
			if((int)*(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 11) == 15)continue;	
			if((int)*(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 11) == 16)continue;	
			//if the record is empty, then break, because we have reached the end of the sector, or cluster? does it really matter?
			if((int)*(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 0) == 0)break;	
			for (int j = 0; j < 11; j++)
			{
				//fprintf(stderr,"inner counter index: %d\n", j);
				//fprintf(stderr,"name byte value: %d, at index: %d ", nameByte,j); 	
				dirRecord.name[j] = (char)parseInteger<uint8_t>(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + j);
				fprintf(stdout,"%c", dirRecord.name[j]);
				
			}
			
			for(int j = 0; j < 11; i++)
			{
				char readInChar = dirRecord.name[j];
				recordName.push_back(readInChar);
			}
			// Set the file to deleted in the filesystem
			if(filetoRemove == recordName)
			{
				
			
			}
			//fprintf(stderr,"outer counter index: %d\n", i);
			if ((int)dirRecord.name[1] < 10 )continue;
			
			//refer to the filesystem spec instead of the dumb slides, the offsets were wrong, and were ORing time stamps.
			dirRecord.attr = parseInteger<uint8_t>(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 11);
			dirRecord.highCluster = parseInteger<uint16_t>(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 20);
			dirRecord.lowCluster = parseInteger<uint16_t>(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 26);
		 
			//we shift 16 bits for the or, because we are making room foor the bits in lowCluster for the addition(draw it out kid)
			dirRecord.highCluster <<= 16;
			dirRecord.fClusterLocation = dirRecord.highCluster | dirRecord.lowCluster;
			
		}
}


void Filesystem::openImage()
{
	
  imageFile = fopen (fname,"wb");
  // If file not found
  if (imageFile==NULL){
  	perror ("Error opening file");
  }
 
}


	


