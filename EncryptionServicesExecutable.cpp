///////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *  @file   EncryptionServicesExecutable.cpp
 *  @author Manel Gonzalez Farrera
 *  @date   April 2018
 *  @brief  Implements EncryptionServicesExecutable used by HostKeeper.sh
 */
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "EncryptionServices.h"


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


