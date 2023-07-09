#ifndef _ENCRYPTION_SERVICES_H
#define _ENCRYPTION_SERVICES_H

/////////////////////////////////////////////////////////////////////////////
/**
 *  @file   EncryptionServices.h
 *  @author Manel Gonzalez Farrera
 *  @date   Apr 2016
 *  @brief  Definition of EncryptionServices
 *
 *  Functions to be documented
 */
/////////////////////////////////////////////////////////////////////////////
 
#include "Logs.h"
//#include <stdio.h>
//#include <iostream>
//#include <fstream>


////////////////////////////////////////////////////////////////////////////////////////////////////
// GLOBAL CONSTANTS
///////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
// CLASS DECLARATION
///////////////////////////////////////////////////////////////////////////////////////////////////

class EncryptionServices : public Logs
{
  public:

    ////////////////////
    // Public Methods //
    ////////////////////
  
    /*
     * Class constructor 
     */
    EncryptionServices(const char* instanceName);

    /*
     * Class destructor 
     */
    ~EncryptionServices() {}
    
    Result isValidSshKey(const char* sshKeyParam);

    std::string generateSshKey(const char* passwd);

    /*
     * Sign file content:
     * Appends a signature at the end of the file
     */
    Result signFileContent(const char* fileName);

    /*
     * Validate signed file content:
     * Checks the file content is consistent with the signature included in the last line of the file
     */
    Result isValidFileContent(const char* fileName);


    // DEPRECATED replaced by transferFile()
    // Result sendLocationFile(const char* serverIp, const char* fileName);

    /*
     * Transfer file to Server FTP into hostId folder if it exists or root folder if not
     */
    Result transferFile(const char* serverIp, const char* hostId, const char* file);

    /*
     * Pull tar files from server and copy to destination directory
     */
    Result pullFilesFromServer(const char* serverIp, const char* hostId, const char* destinationDirectory);

    /*
     * Create signed Tar file:
     * Creates Tar file from contentList and ppends a file Signature.txt
     */
    Result createSignedTarFile(const char* fullFileName, const char* contentList);
 
    /*
     * Validate signed Tar file:
     * Untars file and validates signature
     * If signature is not correct all content is removed 
     */
    Result isValidTarFile(const char* fileName, const char* contentList, const char* destinationDirectory);

  private:

    //////////////////////////////////////
    // Private Data Structures & Methods//
    //////////////////////////////////////

    std::string secretWord_, saltChars_; 

    std::string composeSshPasswd(const char* hwAddr);
    
    std::string generateSalt(const char* characters, unsigned int numChars);

    Result appendFileContent(const char* fileName, std::string& fileContent);  

#if 0 // DEPRECATED: Replaced by MutexServices methods
    Result tryLockLocalMutex(const char* hostId, const char* destinationDirectory, bool& keepLocked);
    Result releaseLocalMutex(const char* hostId, const char* destinationDirectory);

    Result tryLockRemoteMutex(const char* serverIp, const char* hostId);
    Result releaseRemoteMutex(const char* serverIp, const char* hostId);
#endif

    const std::string getHostId();
};

#endif // _ENCRYPTION_SERVICES_H
