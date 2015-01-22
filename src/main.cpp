/* 
Copyright (C) 2014 Jonathon Ogden     < jeog.dev@gmail.com >

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see http://www.gnu.org/licenses.
*/

#include "tos_databridge.h"
#include <fstream>

char TOSDB_LOG_PATH[MAX_PATH+20];

BOOL WINAPI DllMain(HANDLE mod, DWORD why, LPVOID res)
{        
    switch(why){
    case DLL_PROCESS_ATTACH:
        {       
 
        LPCSTR appFolder = "\\tos-databridge";

        GetEnvironmentVariable( "APPDATA", TOSDB_LOG_PATH, MAX_PATH+20 ); 

        strcat_s( TOSDB_LOG_PATH, appFolder );
        CreateDirectory( TOSDB_LOG_PATH, NULL );    

        strcat_s( TOSDB_LOG_PATH, "\\" );
        TOSDB_StartLogging( std::string(TOSDB_LOG_PATH)
                            .append("tos-databridge-shared.log").c_str() );

        break; 

        }
    case DLL_THREAD_ATTACH:  break;
    case DLL_THREAD_DETACH:  break;
    case DLL_PROCESS_DETACH: break;        
    default:                 break;
    }

    return TRUE;
}
