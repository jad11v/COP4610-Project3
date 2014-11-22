#include "file.h"

/*
Converts the string to uppercase and trims the proper character
*/
string File::normalizeToUppercase(string valueToNormalize, char characterToRemove){
    
    	// We ignore any record that is just . or .. 
    	if(valueToNormalize == "." || valueToNormalize == ".."){
    		return valueToNormalize;
    	}
    	
	    // Trim the character to remove if one is passed in
	    valueToNormalize.erase(remove(valueToNormalize.begin(), valueToNormalize.end(), characterToRemove), valueToNormalize.end());   
		
		// Uppercase record
		transform(valueToNormalize.begin(), valueToNormalize.end(), valueToNormalize.begin(),::toupper);
		
		return valueToNormalize;
}