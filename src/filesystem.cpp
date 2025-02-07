#include "filesystem.h"
#include "directory.h"
#include <vector>
#include <sstream>

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
  int dataSector;
  uint32_t nextCluster;
  nextCluster = BPB_RootClus;
  FirstDataSector = BPB_ResvdSecCnt + (BPB_NuMFATs * BPB_FATz32);
  while(nextCluster != 268435448)
  {
    
   dataSector = findFirstSectorOfCluster(nextCluster);
  //fprintf(stdout,"First Sector of First RD cluster %u ",firstSectorClusterRD);
  
  // Gets the FATEntry for the location of the next Root Directory Cluster
  FATEntryRCluster = this->findFatEntry(nextCluster);

  //The second cluster number of the root directory. 
  nextCluster = parseInteger<uint32_t>(fdata + FATEntryRCluster.FATOffset + FATEntryRCluster.FATsecNum * BPB_BytsPerSec);
  getRootDirectoryContents(dataSector);

 	}

}

void Filesystem::getRootDirectoryContents(int FirstDataSector)
{

	string recordName;
	fileRecord record;
	
	record.currentFolder = "root";
	if(DEBUG)
		cout << "cluster sector"<<FirstDataSector << endl;
	//number of records, had to divide by 32, because we were reading too far.
	for(int i = 0; i < BPB_BytsPerSec/32; i++)
		{
			recordName="";
			// If it's a long file name, skip over the record.
			if((int)*(fdata + (FirstDataSector * BPB_BytsPerSec) + i * 32 + 11) == 15)continue;	
			if((int)*(fdata + (FirstDataSector * BPB_BytsPerSec) + i * 32 + 0) == 229)continue;	
			//if the record is empty, then break, because we have reached the end of the sector, or cluster? does it really matter?
			if((int)*(fdata + (FirstDataSector * BPB_BytsPerSec) + i * 32 + 0) == 0)break;	
			for (int j = 0; j < 11; j++)
			{
				//fprintf(stderr,"inner counter index: %d\n", j);
				//fprintf(stderr,"name byte value: %d, at index: %d ", nameByte,j); 	
				record.name[j] = (char)parseInteger<uint8_t>(fdata + (FirstDataSector * BPB_BytsPerSec) + i * 32 + j);
				if(DEBUG)
				fprintf(stdout,"%c", record.name[j]);
				
			}
			if(DEBUG)
			cout << " ";
			//fprintf(stderr,"outer counter index: %d\n", i);
			if ((int)record.name[1] < 10 )continue;
			
			//refer to the filesystem spec instead of the dumb slides, the offsets were wrong, and were ORing time stamps.
			record.attr = parseInteger<uint8_t>(fdata + (FirstDataSector * BPB_BytsPerSec) + i * 32 + 11);
			record.highCluster = parseInteger<uint16_t>(fdata + (FirstDataSector * BPB_BytsPerSec) + i * 32 + 20);
			record.lowCluster = parseInteger<uint16_t>(fdata + (FirstDataSector * BPB_BytsPerSec) + i * 32 + 26);
			record.LstACC = parseInteger<uint16_t>(fdata + (FirstDataSector * BPB_BytsPerSec) + i * 32 + 18);
			record.fileSize = 512;
			//we shift 16 bits for the or, because we are making room foor the bits in lowCluster for the addition(draw it out kid)
			record.highCluster <<= 16;
			record.fClusterLocation = record.highCluster | record.lowCluster;
			
			int temp;
			for(int i =0; i < 11; i++)
			{
				temp += (int)record.name[i];
				
			}
			temp += record.fClusterLocation;
			record.unique = temp;
			// Gets the number that we need to pass into findFirstSectorOfCluster
			//fClusterLocation = record.highCluster + record.lowCluster;
			
			// Get the sector for the contents of the file to read from it
			//int sectorToPass = findFirstSectorOfCluster(record.fClusterLocation);
			// Reads the file data
			//fileHandler.getFileData(sectorToPass, fdata);
			
			if(DEBUG){
				fprintf(stdout,"Entry Type: %x ", record.attr);
				fprintf(stdout,"next cluster location: %d", record.fClusterLocation);
				cout << endl;
			}
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
	//EOC chain is 268435455
	dataSector = 0;
	uint32_t nextCluster;
	while(clusterIndex  != 268435455 && clusterIndex != 268435448)
	{
		FirstDataSector = BPB_ResvdSecCnt + (BPB_NuMFATs * BPB_FATz32);
		dataSector = findFirstSectorOfCluster(clusterIndex);
		// Gets the FATEntry for the location of the next directory cluster
		FATEntryRCluster = this->findFatEntry(clusterIndex);
		//The second cluster number of the root directory.
		nextCluster = parseInteger<uint32_t>(fdata + FATEntryRCluster.FATOffset + FATEntryRCluster.FATsecNum * BPB_BytsPerSec);	
		

		clusterIndex = nextCluster;
		//this will be called for as many additional clusters there are for a directory.
		if(toDo == 0){
			PrintCurrentDirectory(dataSector);
			// If removing file
		}else if(toDo == 1){
			readFilesystemforFile(dataSector,filetoRemove);
		}else if(toDo == 2){
			PrintCurrentDirectory(dataSector, true);
		}
	}	
	
	if(DEBUG)
	fprintf(stdout,"EOC Marker hit %x\n", clusterIndex);

	
}

//this function gets the cluster number for a directory from the vector, and then calls the function to get the data sector and future clusters for the sectors.
bool Filesystem::getDirectoryClusterNumber(string directory)
{
	string recordName;
	bool foundRecord = false;
	bool foundDot = false; // If found . then we know we can transition to .. soon
	
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
				/*if(recordName[0] == '.')
					cout << "dot stuff" << endl;*/
				directory = normalizeToUppercase(directory, ' ');
				cout << "Directory read: " << recordName  << " file cluster loc: " <<  files[i].fClusterLocation << " current cluster loc: " <<  currentClusterLocation << endl;
				
				// For now we make any use of .. go to 2 (root cluster)
				if(recordName == "." && files[i].fClusterLocation == currentClusterLocation){
					foundDot = true;
				}
				else if(directory == ".." && foundDot){
					if(files[i].fClusterLocation == 0){
						findDirectoriesForCluster(2, 0);
					}else{
						findDirectoriesForCluster(files[i].fClusterLocation, 0);
					}
					
					foundRecord = true;
					break;
				}
				
				if(directory == "." && recordName == directory && files[i].fClusterLocation == currentClusterLocation){
					findDirectoriesForCluster(files[i].fClusterLocation, 0);
					foundRecord = true;
					break;
				}else{
					continue;
				}
				
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
	
	fileRecord dirRecord1, dirRecord2;
	bool printedDir = false; // If directory has not been printed
	for(int i = 0; i < BPB_BytsPerSec/32; i++)
		{
		
		// If it's a long file name, skip over the record.
			if((int)*(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 11) == 15)continue;	
			if((int)*(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 0) == 229)continue;	
			//if the record is empty, then break, because we have reached the end of the sector, or cluster? does it really matter?
			if((int)*(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 0) == 0)break;	
			for (int j = 0; j < 11; j++)
			{
				//fprintf(stderr,"inner counter index: %d\n", j);
				//fprintf(stderr,"name byte value: %d, at index: %d ", nameByte,j); 	
				dirRecord.name[j] = (char)parseInteger<uint8_t>(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + j);
				if((int)dirRecord.name[0] == 229)break;
				if(isChangeDirectory == false)
				fprintf(stdout,"%c", dirRecord.name[j]);
				
			}
			if(isChangeDirectory == false)
			cout << " ";
			//fprintf(stderr,"outer counter index: %d\n", i);
			if ((int)dirRecord.name[1] < 10 )continue;
			
			int temp;
			for(int i =0; i < 11; i++)
			{
				temp += (int)dirRecord.name[i];
				
			}
			temp += dirRecord.fClusterLocation;
			dirRecord.unique = temp;
			//refer to the filesystem spec instead of the dumb slides, the offsets were wrong, and were ORing time stamps.
			dirRecord.attr = parseInteger<uint8_t>(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 11);
			dirRecord.highCluster = parseInteger<uint16_t>(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 20);
			dirRecord.lowCluster = parseInteger<uint16_t>(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 26);
		 	
			//we shift 16 bits for the or, because we are making room foor the bits in lowCluster for the addition(draw it out kid)
			dirRecord.highCluster <<= 16;
			dirRecord.fClusterLocation = dirRecord.highCluster | dirRecord.lowCluster;
			dirRecord.currentFolder = workingDirectory;
			dirRecord.fileSize =  parseInteger<uint32_t>(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 28);
			// If were going to store and the file hasn't been pushed yet
			//cout << "Next Cluster Location: " << dirRecord.fClusterLocation << endl;
			if(store == true && valuesStoredMap.count(dirRecord.unique) == 0){
					if(dirRecord.name[0] == '.' && dirRecord.name[1] == ' '){
						dirRecord1 = dirRecord;
					}else if(dirRecord.name[0] == '.' && dirRecord.name[1] == '.'){
						dirRecord2 = dirRecord;
					}else if(printedDir == false){
						files.push_back(dirRecord1);
						files.push_back(dirRecord2);
					}
					files.push_back(dirRecord);
					valuesStoredMap[dirRecord.unique] = 1;
					printedDir = true;
			}
				
			
			if(isChangeDirectory == false)
			cout << endl;
		}
}

/*
	Checks if the directory exists in the current working directory
*/
bool Filesystem::directoryExistsAndChangeTo(string directoryName){
	// Uppercase user input for comparison
	transform(directoryName.begin(), directoryName.end(), directoryName.begin(),::toupper);
	/*
		We must note that the directory name the user enters may just be RED 
		(no spaces) but the stored value may contain spaces so we must know when
		to ignore that.
	*/

	for(unsigned int i = 0; i < files.size(); i++)
	{
		int x = i;
		string recordName = convertCharNameToString(i, 11);
		
		// Go through file name one character at a time
		// and add its name to the recordName
		// for(int j = 0; j < 11; j++)
		// {
		// 	char readInChar = files[i].name[j];
		// 	recordName.push_back(readInChar);
		// }	
		
		cout << "Record Name: " << recordName << "   " << "cluster number: " << files[i].fClusterLocation << endl;
		
		// cout << "Record Name: " << recordName.length() << endl;
		
		// //fprintf()
		// cout << "Directory Name: " << directoryName << endl;
		// cout << "Directory Name: " << directoryName.length() << endl;

		// Trims and converts to uppercase the record and directoryname
		recordName = normalizeToUppercase(recordName, ' ');
		//directoryName = normalizeToUppercase(directoryName,' ');
		//cant do this
		//directoryName = normalizeToUppercase(directoryName, '.');
		
		// If directory and recordname = directorName
		
		//cout << "recordName " << recordName << " directoryName " << directoryName << " entry type " << (int)files[i].attr << endl; 
		while(recordName == directoryName && (int)files[i].attr == 16 && x!=0)
		{
			
			//cout <<"currentClusterLocation" <<currentClusterLocation << endl;
			if(directoryName == ".")
			{
				currentClusterLocation = files[i].fClusterLocation;
				return true;
			}
			else if(directoryName == "..")
			{
				if(files[i].fClusterLocation == 0)
					{
						
						currentClusterLocation = 2;
						workingDirectory = "root";
						return true;
					}
				if(files[i].fClusterLocation == currentClusterLocation)
					{
						
						currentClusterLocation = files[i].fClusterLocation;
						findDirectoriesForCluster(files[i].fClusterLocation,2);
						return true;
					}
				
				}
			x--;
			string filename = convertCharNameToString(x,11);
			filename = normalizeToUppercase(filename, ' ');
			// cout << "filename" << filename << endl;
			// cout << ".. cluster location" << files[x].fClusterLocation << endl;
			// cout <<"current cluster location" <<currentClusterLocation << endl;
			//First time this block of code runs, currentClusterLocaiton is the root cluster;
			if(filename == "..")
			{
				if(files[x].fClusterLocation == currentClusterLocation || files[x].fClusterLocation == 0)
				{
					
					currentClusterLocation = files[i].fClusterLocation;
					findDirectoriesForCluster(files[i].fClusterLocation,2);
					return true;
				}
			}
			
			// If were in the root directory
			if(currentClusterLocation == 2 )
			{
				
				currentClusterLocation = files[i].fClusterLocation;
				findDirectoriesForCluster(files[i].fClusterLocation,2);
				return true;
			}

		}


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
			/*lastIFileLocation = 1;
			
			dirFound = true; // Directories found
			findDirectoriesForCluster(files[i].fClusterLocation, 2);
			break;
		}
	
	}
	return dirFound;
}*/

		
		
	}
return false;
}


/*
	Changes the directory and checks if it exists.
	
	TODO NYI - Need to check if directory would be transformed into version of what
	we are comparing it against, SEE FAT documentation.
*/
void Filesystem::changeDirectory(string directoryName)
{
	// If the directory exists (pretty self explanatory)
	previousWorkingDirectory = workingDirectory;
	workingDirectory = directoryName;
	if(directoryExistsAndChangeTo(directoryName))
	{
		if(DEBUG)
		cout << "Working directory changed to " << directoryName  << endl;
		
	}
	else
	{
		workingDirectory = previousWorkingDirectory;
		cout << "Directory " << directoryName << " was not found" << endl;
		return;
	}
}

/*
	Checks if the directory exists in the current working directory
	Type is the flag to check to see if what was passed in is a file
	or a directory. 
*/
int Filesystem::directoryExists(string directoryName, int type){
	
	
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

	/*
		We must note that the directory name the user enters may just be RED 
		(no spaces) but the stored value may contain spaces so we must know when
		to ignore that.
	*/

	for(unsigned int i = 0; i < files.size(); i++)
	{
		int x = i;
		string recordName = convertCharNameToString(i, 11);
		//cout << x << endl;
		// Go through file name one character at a time
		// and add its name to the recordName
		// for(int j = 0; j < 11; j++)
		// {
		// 	char readInChar = files[i].name[j];
		// 	recordName.push_back(readInChar);
		// }	
		
		//cout << "Record Name: " << recordName << "   " << "cluster number: " << files[i].fClusterLocation << endl;
		
		
		recordName = normalizeToUppercase(recordName, ' ');
		directoryName = normalizeToUppercase(directoryName, ' ');
		while(recordName == directoryName && (int)files[i].attr == propertyType)
		{
			if(currentClusterLocation == 2 )
			{
				
				currentFileIndex = i;
				return 2;
			}
			
			x--;
			string filename = convertCharNameToString(x,11);
			filename = normalizeToUppercase(filename, ' ');
			
			//cout << "filename " << filename <<  " current cluster loc " << currentClusterLocation << " filesx fCluster "  << files[x].fClusterLocation << endl;
			//First time this block of code runs, currentClusterLocaiton is the root cluster;
			if(filename == ".")
			{
				if(files[x].fClusterLocation == currentClusterLocation || files[x].fClusterLocation == 0)
				{
						currentFileIndex = i;
						return files[x].fClusterLocation;
				}
			}
			
			

		}
	
	
	}
	return -1;
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
	// converts passed in file name
	file_name  = normalizeToUppercase(file_name, '.');
	
	if(directoryExists(file_name, 0) >= 0){
		cout << "File to open is not a file" << endl;
		return;
	}else if(directoryExists(file_name, 1) == -1)
	{
		cout << "The file " << file_name << " does not exist" << endl;
		return;
	}
	
	if(fileTable.count(files[currentFileIndex].unique) == 0)
	{
		int permType;
		// Handles the type so we can change it to an int to store
		if(mode == "r"){
			permType = 0;
		}else if(mode == "w"){
			permType = 1;
		}
		else if (mode == "rw")
		{
			permType = 2;
		}
		fileTable[files[currentFileIndex].unique] = permType;
		cout << file_name << " Has been opened with" << " mode " << mode << endl;
	}
	else{
		cout << "file is already open" << endl;
	}
		
}

/*
	Closes the passed in file
*/
void Filesystem::closeFile(string file_name)
{
	// converts passed in file name
	file_name  = normalizeToUppercase(file_name, '.');
	// set current file index
	directoryExists(file_name, 1);
	
	// If file is open
	if(fileTable.count(files[currentFileIndex].unique) == 0){
		cout << "The file " << file_name << " is not already open" << endl;
	}
	// Otherwise we remove it from the open file table
	else{
		fileTable.erase(files[currentFileIndex].unique);
		cout << file_name << " is now closed" << endl;
	}
}

/*
	Checks if the file passed in is in the current directory and 
	removes it if so
*/
void Filesystem::removeFile(string file)
{
	// Converts file
	file = normalizeToUppercase(file, '.');
	cout << file << endl;
	filetoRemove = file;
	// Returns the . index value
	int fileIndex = directoryExists(filetoRemove,1);
	//int dotCluster = files[dotClusterIndex].fClusterLocation;
	 if(fileIndex != -1)
	 {	
	 		string compare = "";
   			//compare = convertCharNameToString(fileIndex);
   			if(currentClusterLocation == 2)
   			{
   			findDirectoriesForCluster(2,1);
   			}
   			findDirectoriesForCluster(fileIndex,1);
			files[currentFileIndex].name[0] = 229;
		string hack = "#" + file;
		
		addFile(2050,hack,false);
		cout << "File " << file << " removed" << endl;
}
    // Otherwise we didnt find the directory
    else
    {
    	cout << "File " << file << " not found" << endl;
    }
	
}

/*
    Removes a directory with the passed in directoryName,
    also checks if the directory already exists in the current
    working directory
*/
void Filesystem::removeDirectory(string directoryName){
	// Converts directoryName
	directoryName = normalizeToUppercase(directoryName, '.');
	
	filetoRemove = directoryName;
	
	// Returns the . index value
	int fileIndex = directoryExists(directoryName, 0);
	
	// Check to see if the directory contains any subfolders
	getDirectoryClusterNumber(directoryName);
	
	if(verifyEmptyDirectory(dataSector)){
		cout << "Directory " << directoryName << " is not empty" << endl;
	//int dotCluster = files[dotClusterIndex].fClusterLocation;
	// If directory found
	 }else if(fileIndex != -1)
	 {
	 		if(DEBUG)
	 			cout << fileIndex << " file index" << endl;
	 		
			string compare = "";
   			//compare = convertCharNameToString(fileIndex);
   			if(currentClusterLocation == 2)
   			{
   			findDirectoriesForCluster(2,1);
   			}
   			findDirectoriesForCluster(fileIndex,1);
			
			files[currentFileIndex].name[0] = 229;
	}
    // Otherwise we didnt find the directory
    else
    {
    	cout << "Directory " << directoryName << " not found" << endl;
    }
}


void Filesystem::readFilesystemforFile(int directoryDataSector,string filetoRemove)
{
	fileRecord dirRecord;
	if(DEBUG)
		cout << directoryDataSector << endl;
	openImage();
	
	for(int i = 0; i < BPB_BytsPerSec/32; i++)
		{
			
			string recordName = "";
		// If it's a long file name, skip over the record.
			if((int)*(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 11) == 15)continue;	
			//if((int)*(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 11) == 16)continue;	
			//if the record is empty, then break, because we have reached the end of the sector, or cluster? does it really matter?
			if((int)*(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 0) == 0)break;	
			
			for (int j = 0; j < 11; j++)
			{
				//fprintf(stderr,"inner counter index: %d\n", j);
				//fprintf(stderr,"name byte value: %d, at index: %d ", nameByte,j); 	
				dirRecord.name[j] = (char)parseInteger<uint8_t>(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + j);
				if(DEBUG)
					fprintf(stdout,"%c", dirRecord.name[j]);
				
			}
			for(int j = 0; j < 11; j++)
			{
				char readInChar = dirRecord.name[j];
				recordName.push_back(readInChar);
			}
			recordName = normalizeToUppercase(recordName, ' ');
			
			if(DEBUG){
				cout << "File to remove " << filetoRemove << endl;
				cout << "Record name " << recordName << endl;
			}
			
			// Set the file to deleted in the filesystem
			if(filetoRemove == recordName)
			{
				if(DEBUG)
					cout << "setting byte" << endl;	
				uint8_t bytes[] = {0xE5};
				//cout << bytes[0] << endl;
				long offset = (directoryDataSector * BPB_BytsPerSec) + i * 32 + 0;
				fseek(imageFile,offset,SEEK_SET);
				fwrite(bytes,sizeof(uint8_t),sizeof(bytes),imageFile);
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
		
		closeImage();
}


void Filesystem::openImage()
{
	
  imageFile = fopen (fname,"r+");
  // If file not found
  if (imageFile==NULL){
  	perror ("Error opening file");
  }
 
}

void Filesystem::displayVectorContents()
{

	for(unsigned int i =0; i < files.size(); i++)
	{
		for(int j = 0; j < 11; j++)
		{
			cout << files[i].name[j];
		}
		cout << endl;
	}
}


void Filesystem::closeImage()
{
	fclose(imageFile);
}

void Filesystem::entrySize(string entry_name)
{
	cout << currentClusterLocation << endl;
	if(directoryExists(entry_name,0) >= 0)
	{
		cout << "current file index" << currentFileIndex << endl;
		cout << "size of entry" << entry_name << " is " << files[currentFileIndex].fileSize << endl;
	}
	
	else if(directoryExists(entry_name,1) >= 0)
	{
		cout << "current file index" << currentFileIndex << endl;
		cout << "size of entry " << entry_name << " is " << files[currentFileIndex].fileSize << endl;
	}

}

void Filesystem::Read(string file_name, unsigned int start_pos,int num_bytes)
{
	file_name = normalizeToUppercase(file_name, '.'); 
	
	openImage();
	char buffer[num_bytes];
	if(directoryExists(file_name,1) < 0)
	{
		cout << "file" << file_name << " is not a file" << endl;
		return;
		
	}
	findDirectoriesForCluster(files[currentFileIndex].fClusterLocation,3);
	
	if(files[currentFileIndex].fileSize < start_pos)
	{
		cout << "Start position is greater than file size" << endl;
		return;
	}
	//checks to see if the file exists in the open file table
	if(fileTable.count(files[currentFileIndex].unique) == 0)
	{
		cout << "File " << file_name << "is not in the open file table" << endl;
		return;
	}
	//checks to see if file in open file table is open for writing not reading or rw
	if(fileTable[files[currentFileIndex].unique] == 1)
	{
		cout << "File " << file_name << "is not open for reading" << endl;
		return;
	}
	for(int j = 0; j < num_bytes; j++)
	{
		
		long offset = (dataSector * BPB_BytsPerSec) + start_pos  + j;
		fseek(imageFile,offset,SEEK_SET);
		fread(buffer,sizeof(char),sizeof(buffer),imageFile);
		fprintf(stdout,"%c",(char)buffer[j]);	
	}
	closeImage();
}

void Filesystem::Write(string file_name, unsigned int start_pos,string quoted_data)
{
	quoted_data.erase(remove(quoted_data.begin(), quoted_data.end(), '"'), quoted_data.end());   
	cout << "Quoted data: " << quoted_data	 << endl;
	file_name = normalizeToUppercase(file_name, '.'); 
	int charCount = quoted_data.length();
	cout << "character count: " << charCount << endl;
	openImage();

	uint8_t *bytes = new uint8_t[quoted_data.size()+1];
	bytes[quoted_data.size()]=0;
	memcpy(bytes,quoted_data.c_str(),quoted_data.size());


	if(directoryExists(file_name,1) < 0)
	{
		cout << "File" << file_name << " is not a file" << endl;
		return;
	}
	findDirectoriesForCluster(files[currentFileIndex].fClusterLocation,3);
	
	if(fileTable.count(files[currentFileIndex].unique) == 0)
	{
		cout << "File " << file_name << "is not in the open file table" << endl;
		return;
	}
	//checks to make sure that the file open for writing, if not, return
	if(fileTable[files[currentFileIndex].unique] != 1)
	{
		cout << "File " << file_name << "is not open for writing" << endl;
		return;
	}
	

	if(start_pos + charCount > files[currentFileIndex].fileSize)
	{
		files[currentFileIndex].fileSize += start_pos + charCount;
		
	}
	for(int j = 0; j < charCount; j++)
	{
		cout << "writing" << endl;
		long offset = (dataSector * BPB_BytsPerSec) + start_pos  + j;
		fseek(imageFile,offset,SEEK_SET);
		fwrite(bytes,sizeof(uint8_t),sizeof(bytes),imageFile);
		

	}

	closeImage();
}


/*
	Checks if the directory is empty in currentFileIndex
	by finding the first . and .. and then seeing if their is anything
	between that record and the next . and ..
*/
bool Filesystem::verifyEmptyDirectory(int directoryDataSector)
{
		fileRecord dirRecord;
		int recordsFound = 2;
		for(int i = 0; i < BPB_BytsPerSec/32; i++)
		{
			
			string recordName = "";
		// If it's a long file name, skip over the record.
			if((int)*(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 11) == 15)continue;	
			//if((int)*(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 11) == 16)continue;	
			if(DEBUG)
				cout << "hit 1 - recordsFound " << recordsFound << endl;
			//if the record is empty, then break, because we have reached the end of the sector, or cluster? does it really matter?
			if((int)*(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + 0) == 0) return (recordsFound == 0) ? false : true;	
			
			for (int j = 0; j < 11; j++)
			{
				//fprintf(stderr,"inner counter index: %d\n", j);
				//fprintf(stderr,"name byte value: %d, at index: %d ", nameByte,j); 	
				dirRecord.name[j] = (char)parseInteger<uint8_t>(fdata + (directoryDataSector * BPB_BytsPerSec) + i * 32 + j);
			}
			for(int j = 0; j < 11; j++)
			{
				char readInChar = dirRecord.name[j];
				recordName.push_back(readInChar);
			}	
			recordName.erase(remove(recordName.begin(), recordName.end(), ' '), recordName.end()); 
		// If we contain a . or .. then decrement counter
		if(recordName == "." || recordName == ".."){
			recordsFound--;
		}
		// If not an empty directory
		else if((int)recordName[0] != -27){
			recordsFound++;
		}
	
	}
	
	return (recordsFound == 0) ? false : true;	
}

void Filesystem::createFile(string filename)
{
  int dotCluster = directoryExists(filename,1);
  if(dotCluster > 0)
  {
    cout << "file " << filename << " already exists" << endl;
    return;
  }
  if(DEBUG)
  	cout << "current cluster location" << currentClusterLocation << endl;
 findDirectoriesForCluster(currentClusterLocation,3);
 addFile(dataSector,filename,false);

}

void Filesystem::addFile(int dataSector,string filename,bool dir)
{
  vector<fileRecord>::iterator iter;
  openImage();
  string currentWorkDirectory = normalizeToUppercase(workingDirectory, ' ');
  
  // Uppercase the filename
  transform(filename.begin(), filename.end(), filename.begin(),::toupper);
  
  string recordName;
  fileRecord record;
  if(filename.size() != 11)
    filename.resize(11);
  uint8_t *bytes = new uint8_t[filename.size()];
  //bytes[filename.size()]=0;
  memcpy(bytes,filename.c_str(),filename.size());
  
  record.currentFolder = workingDirectory;
  if(DEBUG)
    cout << "cluster sector"<<dataSector << endl;
  //number of records, had to divide by 32, because we were reading too far.
  for(int i = 0; i < BPB_BytsPerSec/32; i++)
    {
      
      recordName="";
      // If it's a long file name, skip over the record.
      if((int)*(fdata + (dataSector * BPB_BytsPerSec) + i * 32 + 11) == 15)continue;  
      if((int)*(fdata + (dataSector * BPB_BytsPerSec) + i * 32 + 0) == 229)continue;  
      //if the record is empty, then break, because we have reached the end of the sector, or cluster? does it really matter?
      //i dont know how to allocate a new cluster if it is full, assuming not..
      if((int)*(fdata + (dataSector * BPB_BytsPerSec) + i * 32 + 0) == 0)
      {
        for(int z = 0; z < 11; z++)
        {
          record.name[z] = bytes[z];
        }
		if(DEBUG)
        	cout << "im adding a file" << endl;
        long offset = (dataSector * BPB_BytsPerSec) + i * 32 + 0;
        fseek(imageFile,offset,SEEK_SET);
        fwrite(bytes,sizeof(uint8_t),sizeof(bytes),imageFile);
        
        for(int j = 11; j < 32; j++)
        {
          if( j == 11)
            {
             uint8_t byte[] = {0x00};
				if(!dir)
              uint8_t byte[] = {0x20};
              else
              {
              cout <<"value " << byte << endl;
              uint8_t byte[] = {0x10};	
              }
              
              offset = (dataSector * BPB_BytsPerSec) + i * 32 + j;
              fseek(imageFile,offset,SEEK_SET);
              fwrite(byte,sizeof(uint8_t),sizeof(byte),imageFile);
          }
          if(j == 20 && dir)
          {
          	uint16_t lowbytes = (uint32_t)nextDirectorycluster & 0xFFFF;
          	//uint16_t lowbytes[] = {(uint32_t)nextDirectorycluster & 0xFFFF};
          	offset = (dataSector * BPB_BytsPerSec) + i * 32 + j;
          	fseek(imageFile,offset,SEEK_SET);
          	fwrite(&lowbytes,sizeof(uint16_t),sizeof(lowbytes),imageFile);
          	fprintf(stdout, "%d", lowbytes);
          	
          }
        if(j == 26 && dir)
        {
        	uint16_t highbytes = (uint32_t)nextDirectorycluster >> 16;
          	offset = (dataSector * BPB_BytsPerSec) + i * 32 + j;
          	fseek(imageFile,offset,SEEK_SET);
          	fwrite(&highbytes,sizeof(uint16_t),sizeof(highbytes),imageFile);
          	cout << "high word" << highbytes << endl;
        }
        
        if(j == 28)
        {
          uint8_t bytes[] = {0x0,0x0,0x0,0x0};
          offset = (dataSector * BPB_BytsPerSec) + i * 32 + j;
          fseek(imageFile,offset,SEEK_SET);
          fwrite(bytes,sizeof(uint8_t),sizeof(bytes),imageFile);
        }
        record.fileSize = 0;
        if(!dir)record.attr = 32;
      	if(dir)record.attr = 16;
        }
      for(iter = files.begin();iter!=files.end(); iter++)
      { 
        if((char)(*iter).name[0] =='.' && (char)(*iter).name[1] !='.' )
        {
          
          //cout << ".'s cluster location" << (*iter).fClusterLocation << endl;
          //cout << "current Cluster Location" << currentClusterLocation << endl;
          if((*iter).fClusterLocation == currentClusterLocation)
          {
            files.insert(iter+2,record);
            break;
          }
        }
      }
      break;
    }
      //fprintf(stderr,"outer counter index: %d\n", i);
      if ((int)record.name[1] < 10 )continue;
      
      //refer to the filesystem spec instead of the dumb slides, the offsets were wrong, and were ORing time stamps.
      
      
      
       
      
    }

  closeImage();
}

/*find the first entry in the fat that is zero, the index of that fat entry is the cluster number where
the data exist should exist for that cluster. point that cluster to the end of the cluster chain.*/
void Filesystem::Undelete()
{

	
	
}

void Filesystem::createDirectory(string directoryName)
{
	 
	nextDirectorycluster = findEmptyFAT();
	findDirectoriesForCluster(currentClusterLocation,3);
 	addFile(dataSector,directoryName,true);
 	
 	//add . and .. entried in the cluster number of the new directory
 	int newclustersector = findFirstSectorOfCluster(nextDirectorycluster);
 	//addFile(newclustersector,".",false);
 
}

int Filesystem::findEmptyFAT()
{
	cout << "test" << endl;
	FatEntry entry;
	int x = 2;
	int nextCluster = -1;
	while(nextCluster != 0 )
	{	cout << "next free cluster" << (uint32_t)nextCluster << endl;
		entry = findFatEntry(x);
		nextCluster = parseInteger<uint32_t>(fdata + entry.FATOffset + entry.FATsecNum * BPB_BytsPerSec);
		x++;
	}
	return x-1;
}


void Filesystem::makeDirectory(string directoryName){
    // Check if the directory already exists
    if(directoryExists(directoryName) != -1)
    {
    	cout << "The directory " << directoryName << " already exists." << endl;
    	
    }
    // Otherwise we create the directory
    else
    {
     	createDirectory(directoryName);
    }
}
