///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *  @file   EncryptionServices.cpp
 *  @author Manel Gonzalez Farrera
 *  @date   April 2016
 *  @brief  Implements EncryptionServices
 */
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "EncryptionServices.h"
#include "sha256.h"
#include "MutexServices.h"
//#include "gensalt.h"
#include <string>
#include <fstream>
#include <sstream>
#include <stdlib.h>    /* system */
#include <string.h>    /* strlen */
#include <assert.h>
#include <vector>
#include <time.h>


///////////////////////////////////////////////////////////////////////////////////////////////////
// FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
///////////////////////////////////////////////////////////////////////////////////////////////////

EncryptionServices::EncryptionServices(const char* instanceName) : Logs(instanceName)
{
    logChannels_ = Logger::VERBOSE;

    saltChars_ = "a01b23c45d67e89f";

    // Set secret word
    secretWord_[ 0] = 0x4b;
    secretWord_[ 1] = 0x6f;
    secretWord_[ 2] = 0x6a;
    secretWord_[ 3] = 0x69;
    secretWord_[ 4] = 0x4b;
    secretWord_[ 5] = 0x61;
    secretWord_[ 6] = 0x62;
    secretWord_[ 7] = 0x75;
    secretWord_[ 8] = 0x74;
    secretWord_[ 9] = 0x6f;
    secretWord_[10] = 0x35;
    //LOGGING(VERBOSE, "secretWord is %s", secretWord_.c_str());
}

Result EncryptionServices::isValidSshKey(const char* sshKeyParam)
{
    SHA256 sha256;
    
    if ( sshKeyParam == NULL )      return RESULT_ERROR;
    if ( strlen(sshKeyParam) < 32 ) return RESULT_ERROR;
 
    // Get HW addresses
    std::vector<std::string> hwAddr;
    std::string linuxCommand = "ifconfig | grep HWaddr | awk '{print $5}' >> HWaddr.tmp";
    system(linuxCommand.c_str());
    std::ifstream tmpHwAddrFilePtr("HWaddr.tmp");
    assert( tmpHwAddrFilePtr.is_open() == true );

    std::string oneHwAddr;
    tmpHwAddrFilePtr >> oneHwAddr;
    for ( unsigned int i = 0; tmpHwAddrFilePtr.eof() == false; i++)
    {
        //LOGGING(VERBOSE, "found HWaddr %s", oneHwAddr.c_str());
        hwAddr.push_back( oneHwAddr );
        //LOGGING(VERBOSE, "stored HWaddr %s", hwAddr[i].c_str());
        tmpHwAddrFilePtr >> oneHwAddr;
    }
    tmpHwAddrFilePtr.close();
    system("rm HWaddr.tmp");
    assert( hwAddr.empty() == false );    

    std::string sshKey = sshKeyParam;
    std::string salt = sshKey.substr( 0, 32);
    //LOGGING(VERBOSE, "given salt is %s", salt.c_str());
    std::string givenHash = sshKey.substr(32);
    //LOGGING(VERBOSE, "given hash is %s", givenHash.c_str());

    for ( std::vector<std::string>::const_iterator itHwAddr = hwAddr.begin(); itHwAddr != hwAddr.end(); itHwAddr++ )
    {
        std::string passwd = composeSshPasswd((*itHwAddr).c_str());
        std::string sshPsswd = salt + passwd;
        //LOGGING(VERBOSE, "processing ssh passwd %s + %s...", salt.c_str(), passwd.c_str());
        std::string provenHash = sha256(sshPsswd.c_str());
        //LOGGING(VERBOSE, "proven hash is %s", provenHash.c_str());
        
        if ( givenHash == provenHash ) return RESULT_OK;
    }

    return RESULT_ERROR;
}

std::string EncryptionServices::generateSshKey(const char* passwd)
{
    SHA256 sha256;

    //char* salt = gensalt( "[a-f0-9./]{32}", NULL );
    std::string salt = generateSalt( saltChars_.c_str(), 32 );
    //LOGGING(VERBOSE, "salt generated is %s", salt.c_str());
    std::string saltedPasswd = salt + passwd;
    std::string hash = sha256( saltedPasswd.c_str() );
    //LOGGING(VERBOSE, "hash is %s", hash.c_str());
    std::string sshKey = salt + hash;
    return sshKey;
}

Result EncryptionServices::signFileContent(const char* fileName)
{
    // Read file content
    std::string fileContent = "";
    appendFileContent(fileName, fileContent);    

    // Generate salt'ed content
    std::string salt = generateSalt( saltChars_.c_str(), 32 );
    std::string saltedContent = salt + secretWord_ + fileContent;

    // Generate hash signature
    SHA256 sha256;
    std::string hash = sha256( saltedContent.c_str() );

    // Sign file
    std::ofstream filePtr(fileName, std::ofstream::app);
    if ( !filePtr.is_open() )
    {
        LOGGING(ERRORS, "ERROR opening file %s", fileName);
        return RESULT_ERROR;
    }
    filePtr << salt << hash << std::endl;
    filePtr.close();

    return RESULT_OK;
}

Result EncryptionServices::isValidFileContent(const char* fileName)
{
    std::string fileContent = "", signature;

    // Read file content
    std::ifstream filePtr(fileName);
    if ( !filePtr.is_open() )
    {
        LOGGING(ERRORS, "ERROR opening file %s", fileName);
        return RESULT_ERROR;
    }
    std::string line = "", lineNext = "";
    filePtr >> line;
    while (1)
    {
        filePtr >> lineNext;
        if ( filePtr.eof() )
        {
            signature = line;
            break;
        }
        else
        {
            fileContent = fileContent + line;
            line = lineNext;
        }        
    }
    filePtr.close();

    // Read salt from signature
    std::string salt = signature.substr(0, 32);
    std::string saltedContent = salt + secretWord_ + fileContent;
    
    // Generate hash signature
    SHA256 sha256;
    std::string hash = sha256( saltedContent.c_str() );

    // Validate provided signature
    if ( signature != ( salt + hash ) )
    {
        // Remove file
        LOGGING(VERBOSE, "WARNING provided signature %s is not valid", signature.c_str() );
        LOGGING(VERBOSE, "WARNING removing file %s", fileName);
        char linuxCommand[100];
        sprintf(linuxCommand, "rm -f %s", fileName);
        system(linuxCommand);
        return RESULT_ERROR;
    }

    return RESULT_OK;
}

#if 0 // DEPRECATED: Replaced by transferFile()
Result EncryptionServices::sendLocationFile(const char* serverIp, const char* fileName)
{
    char linuxCommand[200], fbFileName[100];
    sprintf(fbFileName, "%s.feedback", fileName);    

    // Remove feedback file in case it exists
    std::ifstream fbFilePtr;
    if ( fbFilePtr.is_open() )
    {
        fbFilePtr.close();
        sprintf(linuxCommand, "rm -f %s", fbFileName);
        system(linuxCommand);
    }    

    // Send location file; redirect stdout to file for later check
    sprintf(linuxCommand, "ftp -n %s << EOF\n", serverIp);
    sprintf(linuxCommand, "%squote USER \"hosttimer\"\n", linuxCommand);
    sprintf(linuxCommand, "%squote PASS \"%s\"\n", linuxCommand, secretWord_.c_str());
    sprintf(linuxCommand, "%sput %s\n", linuxCommand, fileName);
    sprintf(linuxCommand, "%sget %s %s\n", linuxCommand, fileName, fbFileName);
    sprintf(linuxCommand, "%squit\n", linuxCommand);
    sprintf(linuxCommand, "%sEOF\n", linuxCommand);
    //LOGGING(VERBOSE, "executing FTP linux command...\n%s", linuxCommand);
    system(linuxCommand);

    // Check that location file has been sent correctly
    fbFilePtr.open(fbFileName, std::ifstream::in);
    if ( fbFilePtr.is_open() )
    {
        fbFilePtr.close();
        sprintf(linuxCommand, "rm -f %s", fbFileName);
        system(linuxCommand);
        return RESULT_OK;
    }    
    else return RESULT_ERROR;
}
#endif

Result EncryptionServices::transferFile(const char* serverIp, const char* hostId, const char* file)
{
    // Check that the file to be transferred exists
    std::ifstream filePtr(file);
    if ( !filePtr.is_open() )
    {
        LOGGING(ERRORS, "ERROR not possible to open provided file %s", file);
        return RESULT_ERROR;
    }
    else filePtr.close();

    std::string fullFileName = file;
    std::string path = "";

    std::size_t slashPos = fullFileName.rfind('/');
    if ( slashPos == std::string::npos ) path = "./";
    else                                 path = fullFileName.substr(0, slashPos+1);
    std::string name = fullFileName.substr(slashPos+1);
    LOGGING(VERBOSE, "received parameters serverIp:%s file path:%s and name:%s", serverIp, path.c_str(), name.c_str());

    size_t bufferSize = 400;
    char linuxCommand[bufferSize];

    // Remove feedback file in case it exists
    char fbFileName[100];
    char fbFullName[100];
    sprintf(fbFileName, "%s_feedback", name.c_str());    
    sprintf(fbFullName, "%s%s", path.c_str(), fbFileName);    
    std::ifstream fbFilePtr;
    if ( fbFilePtr.is_open() )
    {
        fbFilePtr.close();
        sprintf(linuxCommand, "rm -f %s", fbFullName);
        LOGGING(VERBOSE, "executing linux command '%s'", linuxCommand);
        system(linuxCommand);
    }    
    LOGGING(VERBOSE, "removed feedback file %s if it existed", fbFileName);

    MutexServices mutexServices("MutexServices", hostId);
    Result result = mutexServices.tryLockRemoteMutex(serverIp, secretWord_.c_str(), hostId);
    if ( result != RESULT_OK )
    {
        LOGGING(ERRORS, "failed to lock Mutes in remote server with result %d, cancelling", result);
        return result;
    }

    auto reportSnprintfError = [this, &linuxCommand] () {
        LOGGING(ERRORS, "ERROR: snprintf buffer size is not enought for '%s'", linuxCommand);
        return RESULT_ERROR;
    };

    // Send file to target server ip
    if ( snprintf(linuxCommand, bufferSize, "ftp -n %s << EOF\n", serverIp) > bufferSize ) return reportSnprintfError();
    std::string auxBuffer = linuxCommand;
    if ( snprintf(linuxCommand, bufferSize, "%squote USER \"hosttimer\"\n", auxBuffer.c_str() ) > bufferSize ) return reportSnprintfError();
    auxBuffer = linuxCommand;
    if ( snprintf(linuxCommand, bufferSize, "%squote PASS \"%s\"\n", auxBuffer.c_str(), secretWord_.c_str()) > bufferSize ) return reportSnprintfError();
    auxBuffer = linuxCommand;
    if ( snprintf(linuxCommand, bufferSize, "%scd %s\n", auxBuffer.c_str(), hostId) > bufferSize ) return reportSnprintfError();
    auxBuffer = linuxCommand;
    if ( snprintf(linuxCommand, bufferSize, "%slcd %s\n", auxBuffer.c_str(), path.c_str()) > bufferSize ) return reportSnprintfError();
    auxBuffer = linuxCommand;
    if ( snprintf(linuxCommand, bufferSize, "%sput %s\n", auxBuffer.c_str(), name.c_str()) > bufferSize ) return reportSnprintfError();
    auxBuffer = linuxCommand;
    if ( snprintf(linuxCommand, bufferSize, "%schmod 666 %s\n", auxBuffer.c_str(), name.c_str()) > bufferSize ) return reportSnprintfError();
    auxBuffer = linuxCommand;
    if ( snprintf(linuxCommand, bufferSize, "%sget %s %s\n", auxBuffer.c_str(), name.c_str(), fbFileName) > bufferSize ) return reportSnprintfError();
    auxBuffer = linuxCommand;
    if ( snprintf(linuxCommand, bufferSize, "%squit\n", auxBuffer.c_str()) > bufferSize ) return reportSnprintfError();
    auxBuffer = linuxCommand;
    if ( snprintf(linuxCommand, bufferSize, "%sEOF\n", auxBuffer.c_str()) > bufferSize ) return reportSnprintfError();
    //LOGGING(VERBOSE, "executing FTP linux command...\n%s", linuxCommand);
    system(linuxCommand);

    // Check file has been sent correctly
    fbFilePtr.open(fbFullName, std::ifstream::in);
    if ( fbFilePtr.is_open() )
    {
        LOGGING(VERBOSE, "feedback file %s received correctly", fbFullName);
        fbFilePtr.close();
        sprintf(linuxCommand, "rm -f %s", fbFullName);
        LOGGING(VERBOSE, "executing linux command %s", linuxCommand);
        system(linuxCommand);
    }    
    else
    {
        LOGGING(ERRORS, "ERROR feedback file %s not found", fbFullName);
        if ( mutexServices.releaseRemoteMutex(serverIp, secretWord_.c_str(), hostId) != RESULT_OK ) LOGMSG(ERRORS, "failed to release Mutex in remote server");
        return RESULT_ERROR;
    }

    result = mutexServices.releaseRemoteMutex(serverIp, secretWord_.c_str(), hostId); 
    if ( result != RESULT_OK )
    {
        LOGGING(ERRORS, "failed to release Mutex in remote server with result %d", result);
        return result;
    }

    return RESULT_OK;
}

Result EncryptionServices::pullFilesFromServer(const char* serverIp, const char* hostId, const char* destinationDirectory)
{
    MutexServices mutexServices("MutexServices", hostId);
    Result result = mutexServices.tryLockRemoteMutex(serverIp, secretWord_.c_str(), hostId);
    if ( result != RESULT_OK )
    {
        LOGGING(ERRORS, "failed to lock Mutex in remote server with result %d, cancelling", result);
        return result;
    }

    bool keepLocked = false;
    result = mutexServices.tryLockLocalMutex(destinationDirectory, keepLocked);
    if ( result != RESULT_OK )
    {
        LOGGING(ERRORS, "failed to lock local Mutex result %d, cancelling", result);
        return result;
    }

    size_t bufferSize = 400;
    char linuxCommand[bufferSize];

    auto reportSnprintfError = [this, &linuxCommand] () {
        LOGGING(ERRORS, "ERROR: snprintf buffer size is not enought for '%s'", linuxCommand);
        return RESULT_ERROR;
    };

    // Pull all tar files from remote Server FTP
    if ( snprintf(linuxCommand, bufferSize, "ftp -i -n %s << EOF\n", serverIp) > bufferSize ) return reportSnprintfError();
    std::string auxBuffer = linuxCommand;
    if ( snprintf(linuxCommand, bufferSize, "%squote USER \"hosttimer\"\n", auxBuffer.c_str() ) > bufferSize ) return reportSnprintfError();
    auxBuffer = linuxCommand;
    if ( snprintf(linuxCommand, bufferSize, "%squote PASS \"%s\"\n", auxBuffer.c_str(), secretWord_.c_str()) > bufferSize ) return reportSnprintfError();
    auxBuffer = linuxCommand;
    if ( snprintf(linuxCommand, bufferSize, "%scd %s\n", auxBuffer.c_str(), hostId) > bufferSize ) return reportSnprintfError();
    auxBuffer = linuxCommand;
    if ( snprintf(linuxCommand, bufferSize, "%slcd %s\n", auxBuffer.c_str(), destinationDirectory) > bufferSize ) return reportSnprintfError();
    auxBuffer = linuxCommand;
    if ( snprintf(linuxCommand, bufferSize, "%smget *.tar\n", auxBuffer.c_str()) > bufferSize ) return reportSnprintfError();
    auxBuffer = linuxCommand;
    if ( snprintf(linuxCommand, bufferSize, "%smdelete *.tar\n", auxBuffer.c_str()) > bufferSize ) return reportSnprintfError();
    auxBuffer = linuxCommand;
    if ( snprintf(linuxCommand, bufferSize, "%squit\n", auxBuffer.c_str()) > bufferSize ) return reportSnprintfError();
    auxBuffer = linuxCommand;
    if ( snprintf(linuxCommand, bufferSize, "%sEOF\n", auxBuffer.c_str()) > bufferSize ) return reportSnprintfError();
    //LOGGING(VERBOSE, "executing FTP linux command...\n%s", linuxCommand);
    system(linuxCommand);

    if ( keepLocked ) LOGMSG(VERBOSE, "keeping local Mutex locked");
    else
    {
        result = mutexServices.releaseLocalMutex(destinationDirectory); 
        if ( result != RESULT_OK )
        {
            LOGGING(ERRORS, "ERROR: failed to release local Mutex with result %d", result);
            if ( mutexServices.releaseRemoteMutex(serverIp, secretWord_.c_str(), hostId) != RESULT_OK ) LOGMSG(ERRORS, "ERROR: failed to release Mutex in remote server");
            return result;
        }
    }

    result = mutexServices.releaseRemoteMutex(serverIp, secretWord_.c_str(), hostId); 
    if ( result != RESULT_OK )
    {
        LOGGING(ERRORS, "ERROR: failed to release Mutex in remote server with result %d", result);
        return result;
    }

    return RESULT_OK;
}

Result EncryptionServices::createSignedTarFile(const char* fullFileName, const char* contentList)
{
    // Lock local mutex in destination directory
    std::string destinationDirectory = fullFileName;
    if ( destinationDirectory.find('/') != std::string::npos )
    {
        destinationDirectory = destinationDirectory.substr(0, destinationDirectory.find_last_of('/') + 1);
    }
    else
    {
        destinationDirectory = "./";
    }
    bool keepLocked = false;
    LOGGING(VERBOSE, "trying to lock local mutex in destinatio directory %s", destinationDirectory.c_str());
    MutexServices mutexServices("MutexServices", getHostId().c_str() );
    Result result = mutexServices.tryLockLocalMutex(destinationDirectory.c_str(), keepLocked);
    if ( result != RESULT_OK ) 
    {
        LOGGING(ERRORS, "ERROR trying to lock local mutex in destination directory %s", destinationDirectory.c_str());
        return result;
    } 

    // Remove tar file in case it exist
    char linuxCommand[100];
    sprintf(linuxCommand, "rm -f %s", fullFileName);
    system(linuxCommand);

    // Read content of files listed and add to tar file
    std::string filesContent = "";
    std::ifstream contentListFilePtr(contentList);
    if ( !contentListFilePtr.is_open() )
    {
        LOGGING(ERRORS, "ERROR opening file %s", contentList);
        return RESULT_ERROR;
    }
    std::string oneFile = "";
    contentListFilePtr >> oneFile;
    // Create tar file with first file found in content list
    if ( oneFile != "" )
    {
        std::string sourceDirectory = oneFile.substr(0, oneFile.find_last_of('/') + 1 );
        std::string fileName = oneFile.substr( oneFile.find_last_of('/') + 1, oneFile.length() );
        //sprintf(linuxCommand, "tar -cf %s %s", fullFileName, oneFile.c_str());
        std::string linuxCommand = std::string("tar -cf ") + fullFileName + " --directory " + sourceDirectory + " " + fileName;
        LOGGING(VERBOSE, "Executing linux commnad '%s'", linuxCommand.c_str() );
        system( linuxCommand.c_str() );
    }  
    while ( !contentListFilePtr.eof() )
    {
        appendFileContent( oneFile.c_str(), filesContent );
        oneFile = "";
        contentListFilePtr >> oneFile;
        // Append next file found in content list
        if ( oneFile != "" )
        {
            std::string sourceDirectory = oneFile.substr(0, oneFile.find_last_of('/') + 1 );
            std::string fileName = oneFile.substr( oneFile.find_last_of('/') + 1, oneFile.length() );
            //sprintf(linuxCommand, "tar -rf %s %s", fullFileName, oneFile.c_str());
            std::string linuxCommand = std::string("tar -rf ") + fullFileName + " --directory " + sourceDirectory + " " + fileName;
            LOGGING(VERBOSE, "Executing linux commnad '%s'", linuxCommand.c_str() );
            system( linuxCommand.c_str() );
        }  
    }

    // Generate salt'ed content
    std::string salt = generateSalt( saltChars_.c_str(), 32 );
    std::string saltedContent = salt + secretWord_ + filesContent;
    
    // Generate hash signature
    SHA256 sha256;
    std::string hash = sha256( saltedContent.c_str() );
     
    // Create Signature.txt file
    std::ofstream signatureFilePtr("Signature.txt");
    if ( !signatureFilePtr.is_open() )
    {
        LOGMSG(ERRORS, "ERROR opening Signature.txt file");
        return RESULT_ERROR;
    }
    signatureFilePtr << salt << hash << std::endl;
    signatureFilePtr.close();

    // Append Signature to Tar file and remove it
    sprintf(linuxCommand, "tar -rf %s Signature.txt", fullFileName);
    LOGGING(VERBOSE, "Executing linux commnad '%s'", linuxCommand);
    system(linuxCommand);
    system("rm -f Signature.txt");

    result = mutexServices.releaseLocalMutex(destinationDirectory.c_str());
    if ( result != RESULT_OK )
    {
        LOGGING(ERRORS, "ERROR releasing local mutex in destination directory %s", destinationDirectory.c_str());
        return result;
    }

    return RESULT_OK; 
}

Result EncryptionServices::isValidTarFile(const char* fileName, const char* contentList, const char* destinationDirectory)
{
    //bool validSignature = false;
    std::string providedSignature = "";

    // Untar file
    //char linuxCommand[100];
    //sprintf(linuxCommand, "tar -xf %s --directory=%s", fileName, destinationDirectory);
    std::string linuxCommand = std::string("tar -xf ") + fileName + " --directory=" + destinationDirectory;
    system( linuxCommand.c_str() );
    
    // Read content of all files listed except signature
    std::string filesContent = "";
    std::ifstream contentListFilePtr(contentList);
    if ( !contentListFilePtr.is_open() )
    {
        LOGGING(ERRORS, "ERROR opening file %s", contentList);
        return RESULT_ERROR;
    }
    std::string oneFileName = "";
    //char linuxRemoveCommand[200] = "rm -f ";
    std::string linuxRemoveCommand = "rm -f ";
    std::string directory = destinationDirectory;
    // Check whether last character of directory is '/' or add
    std::string lastCharacter = directory.substr(directory.size()-1, directory.size());
    if ( lastCharacter != "/" )
    {
        LOGMSG(VERBOSE, "Adding '/' ending charater in destination directory");
        lastCharacter = "/";
        directory = directory + lastCharacter; 
    }
    contentListFilePtr >> oneFileName;
    std::string oneFile = directory + oneFileName;
    while ( !contentListFilePtr.eof() )
    {
        LOGGING(VERBOSE, "one file is %s", oneFile.c_str());
        if ( oneFileName != "Signature.txt" ) appendFileContent( oneFile.c_str(), filesContent );
        else                                  appendFileContent( oneFile.c_str(), providedSignature );
        //LOGGING(VERBOSE, "files content is %s", filesContent.c_str());
        //LOGGING(VERBOSE, "provided signature is %s", providedSignature.c_str());
        //sprintf(linuxRemoveCommand, "%s %s ", linuxRemoveCommand, oneFile.c_str() );
        linuxRemoveCommand += " " + oneFile;
        contentListFilePtr >> oneFileName;
        oneFile = directory + oneFileName;
    }

    // Trim salt from provided signature
    std::string salt = providedSignature.substr(0, 32);
    LOGGING(VERBOSE, "trimmed salt from provided signature is %s", salt.c_str() );

    // Generate salt'ed content
    std::string saltedContent = salt + secretWord_ + filesContent;
    
    // Generate hash signature
    SHA256 sha256;
    std::string hash = sha256( saltedContent.c_str() );
    LOGGING(VERBOSE, "calculated hash is %s", hash.c_str() );

    // Validate provided signature
    if ( providedSignature != ( salt + hash ) )
    {
        // Remove all tar file content
        LOGGING(VERBOSE, "WARNING provided signature %s is not valid", providedSignature.c_str() );
        LOGGING(VERBOSE, "WARNING removing tar file content with command: '%s'", linuxRemoveCommand.c_str() );
        system( linuxRemoveCommand.c_str() );
        return RESULT_ERROR;
    }

    return RESULT_OK;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
///////////////////////////////////////////////////////////////////////////////////////////////////

std::string EncryptionServices::composeSshPasswd(const char* hwAddr)
{
    std::string passwd = "";
    unsigned int i=0, j=0;
    do
    {
        if ( hwAddr[j] == ':' ) j++;
        passwd = passwd + hwAddr[j];
        passwd = passwd + secretWord_[i];
        i++;
        j++;
    }
    while ( i < 12 );

    return passwd;
}

std::string EncryptionServices::generateSalt(const char* characters, unsigned int numChars)
{
    std::string salt = "";
    srand( time(NULL) );
    for ( unsigned int i = 0; i < numChars; i++)
    {
        salt = salt + characters[ rand() % strlen(characters) ];
    }
    return salt;
}

Result EncryptionServices::appendFileContent(const char* fileName, std::string& fileContent)
{
    std::ifstream filePtr(fileName);
    if ( !filePtr.is_open() )
    {
        LOGGING(ERRORS, "ERROR opening file %s", fileName);
        return RESULT_ERROR;
    }
    std::string line = "";
    filePtr >> line;
    while ( !filePtr.eof() )
    {
        fileContent = fileContent + line;
        filePtr >> line;
    }
    filePtr.close();

    return RESULT_OK;
}

#if 0 // DEPRECATED: Replaced by MutexServices methods
Result EncryptionServices::tryLockRemoteMutex(const char* serverIp, const char* hostId)
{
    char linuxCommand[400];
    
    // Get Mutex.locked if exists in remote Server FTP
    sprintf(linuxCommand, "ftp -n %s << EOF\n", serverIp);
    sprintf(linuxCommand, "%squote USER \"hosttimer\"\n", linuxCommand);
    sprintf(linuxCommand, "%squote PASS \"%s\"\n", linuxCommand, secretWord_.c_str());
    sprintf(linuxCommand, "%scd %s\n", linuxCommand, hostId);
    sprintf(linuxCommand, "%sget Mutex.locked Mutex.locked_remote\n", linuxCommand);
    sprintf(linuxCommand, "%squit\n", linuxCommand);
    sprintf(linuxCommand, "%sEOF\n", linuxCommand);
    //LOGGING(VERBOSE, "executing FTP linux command...\n%s", linuxCommand);
    system(linuxCommand);

    // Check owner of Mutex and cancell if different; remove file when read
    std::ifstream mutexFilePtr("Mutex.locked_remote");
    if ( mutexFilePtr.is_open() )
    {
        std::string mutexOwner;
        mutexFilePtr >> mutexOwner;
        mutexFilePtr.close();
        system("rm -f Mutex.locked_remote");
        if ( ( mutexOwner != "" ) && ( mutexOwner != hostId ) )
        { 
            LOGMSG(VERBOSE, "Mutex locked found in remote Server FTP, cancelling pull of files\n");
            return RESULT_CANCELLED;
        }
    }

    // Lock mutex in remote Server FTP
    sprintf(linuxCommand, "echo %s > remoteMutex.toLock", hostId);
    LOGGING(VERBOSE, "executing linux command '%s'", linuxCommand);
    system(linuxCommand);
    sprintf(linuxCommand, "ftp -n %s << EOF\n", serverIp);
    sprintf(linuxCommand, "%squote USER \"hosttimer\"\n", linuxCommand);
    sprintf(linuxCommand, "%squote PASS \"%s\"\n", linuxCommand, secretWord_.c_str());
    sprintf(linuxCommand, "%scd %s\n", linuxCommand, hostId);
    sprintf(linuxCommand, "%sdelete Mutex.locked\n", linuxCommand);
    sprintf(linuxCommand, "%sput remoteMutex.toLock Mutex.locked\n", linuxCommand);
    sprintf(linuxCommand, "%sget Mutex.locked Mutex.locked_remote\n", linuxCommand);
    sprintf(linuxCommand, "%squit\n", linuxCommand);
    sprintf(linuxCommand, "%sEOF\n", linuxCommand);
    //LOGGING(VERBOSE, "executing FTP linux command...\n%s", linuxCommand);
    system(linuxCommand);
    system("rm -f remoteMutex.toLock");

    // Confirm mutex is locked by host
    mutexFilePtr.open("Mutex.locked_remote");
    {
        std::string mutexOwner;
        mutexFilePtr >> mutexOwner;
        mutexFilePtr.close();
        system("rm -f Mutex.locked_remote");
        if ( ( mutexOwner != "" ) && ( mutexOwner != hostId ) )
        { 
            LOGMSG(VERBOSE, "failed to lock Mutex in remote Server FTP, cancelling pull of files\n");
            return RESULT_CANCELLED;
        }
    }

    return RESULT_OK;
}

Result EncryptionServices::releaseRemoteMutex(const char* serverIp, const char* hostId)
{
    char linuxCommand[400];

    // Release mutex in remote Server FTP
    sprintf(linuxCommand, "ftp -i -n %s << EOF\n", serverIp);
    sprintf(linuxCommand, "%squote USER \"hosttimer\"\n", linuxCommand);
    sprintf(linuxCommand, "%squote PASS \"%s\"\n", linuxCommand, secretWord_.c_str());
    sprintf(linuxCommand, "%scd %s\n", linuxCommand, hostId);
    sprintf(linuxCommand, "%sdelete Mutex.locked\n", linuxCommand);
    sprintf(linuxCommand, "%squit\n", linuxCommand);
    sprintf(linuxCommand, "%sEOF\n", linuxCommand);
    //LOGGING(VERBOSE, "executing FTP linux command...\n%s", linuxCommand);
    system(linuxCommand);

    return RESULT_OK;
} 

Result EncryptionServices::tryLockLocalMutex(const char* hostId, const char* destinationDirectory, bool& keepLocked)
{
    std::string mutexFullName = std::string(destinationDirectory) + "Mutex.locked";
    
    // Try open mutex file
    std::ifstream mutexFilePtr(mutexFullName);
    if ( mutexFilePtr.is_open() )
    {
        std::string mutexOwner;
        mutexFilePtr >> mutexOwner;
        mutexFilePtr.close();
        if ( mutexOwner != hostId )
        {
            LOGGING(VERBOSE, "local Mutex locked found with owner %s, cancelling pull of files", mutexOwner.c_str());
            return RESULT_CANCELLED;
        }
        else keepLocked = true; 
    }
    else
    {
        std::string linuxCommand = std::string("echo ") + hostId + " > " + mutexFullName;
        LOGGING(VERBOSE, "executing linux command '%s'", linuxCommand.c_str() );
        system( linuxCommand.c_str() );
    }

    // Confirm mutex has been locked by host
    mutexFilePtr.open( mutexFullName.c_str() );
    if ( mutexFilePtr.is_open() )
    {
        std::string mutexOwner;
        mutexFilePtr >> mutexOwner;
        mutexFilePtr.close();
        if ( mutexOwner != hostId )
        {
            LOGGING(VERBOSE, "failed to lock local Mutex colliding with owner %s, cancelling pull of files", mutexOwner.c_str());
            return RESULT_CANCELLED;
        } 
    }
    else
    {
        LOGMSG(VERBOSE, "failed to lock local Mutex, cancelling pull of files");
        return RESULT_CANCELLED;
    }

    return RESULT_OK;
}

Result EncryptionServices::releaseLocalMutex(const char* hostId, const char* destinationDirectory)
{
    std::string mutexFullName = std::string(destinationDirectory) + "Mutex.locked";

    // Remove mutex file
    std::string linuxCommand = "rm -f " + mutexFullName; 
    LOGGING(VERBOSE, "executing linux command '%s'", linuxCommand.c_str());
    system( linuxCommand.c_str() );

    return RESULT_OK;
}
#endif

const std::string EncryptionServices::getHostId()
{
    // Get HW addresses
    system("rm -rf HWaddr.tmp");
    std::string linuxCommand = "ifconfig | grep HWaddr | grep eth0 | awk '{print $5}' | sed 's/://g' >> HWaddr.tmp";
    system(linuxCommand.c_str());
    linuxCommand = "ifconfig | grep HWaddr | grep wlan0 | awk '{print $5}' | sed 's/://g' >> HWaddr.tmp";
    system(linuxCommand.c_str());

    // Construct Host ID
    std::ifstream tmpHwAddrFilePtr("HWaddr.tmp");
    assert( tmpHwAddrFilePtr.is_open() == true );

    std::string hostId;
    while ( !tmpHwAddrFilePtr.eof() ) tmpHwAddrFilePtr >> hostId;
    LOGGING(VERBOSE, "Host ID is %s", hostId.c_str());

    tmpHwAddrFilePtr.close();
    system("rm -rf HWaddr.tmp");

    return hostId;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// PROTECTED METHODS
///////////////////////////////////////////////////////////////////////////////////////////////////


#if 0 // Moved main to EncryptionServicesExecutable..cpp
///////////////////////////////////////////////////////////////////////////////////////////////////
// MAIN
///////////////////////////////////////////////////////////////////////////////////////////////////

char Logger::logFileName_[] = "EncryptionServices.logs";

int main( int argc, const char* argv[] )
{
    Result result;

    Logger* logger = Logger::getInstance();
    logger->logging("main EncryptionServices creating encryptionServices instance...");
    EncryptionServices encryptionServices("EncryptionServices");

    //char logMessage[200];
    //sprintf(logMessage, "main isValidSshKey called with %d arguments: %s", argc, argv[1]);
    //logger->logging(logMessage);

    std::string param = argv[1];

    if ( param == "-ivsk" || param == "--isValidSshKey" )
    {
        if ( argc == 2 ) std::cin >> param;
        else             param = argv[2];

        if ( encryptionServices.isValidSshKey( param.c_str() ) == RESULT_OK ) std::cout << "1" << std::endl;
        else                                                                  std::cout << "0" << std::endl;
    }
    else if ( param == "-gsk" || param == "--generateSshKey" )
    {
        std::string sshKey = encryptionServices.generateSshKey( argv[2] );
        std::cout << sshKey << std::endl;
    }
    else if ( param == "-sfc" || param == "--signFileContent" )
    {
        if ( encryptionServices.signFileContent( argv[2] ) == RESULT_OK ) std::cout << "0" << std::endl;
        else                                                              std::cout << "1" << std::endl;
    }
    else if ( param == "-ivfc" || param == "--isValidFileContent" )
    {
        if ( encryptionServices.isValidFileContent( argv[2] ) == RESULT_OK ) std::cout << "1" << std::endl;
        else                                                                 std::cout << "0" << std::endl;
    }
#if 0 // DEPRECATED: Preplaced by transferFile()
    else if ( param == "-slf" || param == "--sendLocationFile" )
    {
        if ( encryptionServices.sendLocationFile( argv[2], argv[3] ) == RESULT_OK ) std::cout << "0" << std::endl;
        else                                                                        std::cout << "1" << std::endl;
    }
#endif
    else if ( ( param == "-txf" || param == "--transferFile" ) && ( argc == 5 ) )
    {
        std::cout << encryptionServices.transferFile(argv[2],argv[3],argv[4]) << std::endl;
    }
    else if ( ( param == "-pffs" || param == "--pullFilesFromServer" ) && ( argc == 5 ) )
    {
        result = encryptionServices.pullFilesFromServer( argv[2], argv[3], argv[4] );
        switch (result)
        {
        case RESULT_OK:
            std::cout << "0" << std::endl;
            break;
        case RESULT_CANCELLED:
            std::cout << "2" << std::endl;
            break;
        default:
            std::cout << "1" << std::endl;
        }
    }
    else if ( param == "-cstf" || param == "--createSignedTarFile" )
    {
        if ( encryptionServices.createSignedTarFile( argv[2], argv[3] ) == RESULT_OK ) std::cout << "0" << std::endl;
        else                                                                           std::cout << "1" << std::endl;
    }
    else if ( param == "-ivtf" || param == "--isValidTarFile" )
    {
        if ( encryptionServices.isValidTarFile( argv[2], argv[3], argv[4] ) == RESULT_OK ) std::cout << "1" << std::endl;
        else                                                                               std::cout << "0" << std::endl;
    }
    else std::cout << "encryptionServices: invalid option " << param << std::endl;
}
#endif
