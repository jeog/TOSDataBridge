/* 
Copyright (C) 2014 Jonathon Ogden   < jeog.dev@gmail.com >

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

#include <iomanip>
#include <algorithm>

#include "shell.hpp"

/* 
    NOTE: this shell is for testing/querying/debugging interface and engine. 

   *** It DOES NOT check input, format all output etc. etc.***
 */

stream_prompt prompt("[-->");


namespace{

template<int W, const char FILL>
void
_display_header_line(std::string pre, std::string post, std::string text);

template<int W, int INDENT, char H>
void
_display_header(std::string hpre, std::string hpost);

}

int main(int argc, char* argv[])
{         
    std::string cmd;

    _display_header<MAX_DISPLAY_WIDTH, LEFT_INDENT_SIZE, '-'>("[--", "--]");

    while(1){ 
        new_command:
        try{
            prompt>> cmd;   

            /* handle local commands */
            bool r = true;
            for(auto & p : commands_local){
                if(p.first == cmd){                    
                    p.second((void*)&r);
                    if(!r)
                        goto exit_prompt;
                    goto new_command;
                }
            }
            
            /* handle 'admin' api commands */
            for(auto & p : commands_admin){
                if(p.first == cmd){                    
                    p.second(nullptr);
                    goto new_command;
                }
            }

            /* handle 'get' api commands */
            for(auto & p : commands_get){
                if(p.first == cmd){                    
                    p.second(nullptr);
                    goto new_command;
                }
            }

            /* handle 'stream' api commands */
            for(auto & p : commands_stream){
                if(p.first == cmd){                    
                    p.second(nullptr);
                    goto new_command;
                }
            }

            /* handle 'frame' api commands */
            for(auto & p : commands_frame){
                if(p.first == cmd){                    
                    p.second(nullptr);
                    goto new_command;
                }
            }

            /* default to here */        
            std::cout<< std::endl << "BAD COMMAND" << std::endl << std::endl;
            
        }catch(TOSDB_Error& e){             
            std::cerr<< std::endl << "*** TOSDB_Error caught by shell **" << std::endl
                     << std::setw(15) << "    Process ID: "<< e.processID() << std::endl
                     << std::setw(15) << "    Thread ID: "<< e.threadID() << std::endl
                     << std::setw(15) << "    tag: " << e.tag() << std::endl
                     << std::setw(15) << "    info: " << e.info() << std::endl
                     << std::setw(15) << "    what: " << e.what() << std::endl << std::endl;
            goto exit_prompt;
        }

        continue;        

        /* use goto so we can cleanly break out of main while loop */
        exit_prompt: 
        {
            std::string cont;
            std::cout<< std::endl << "Exit shell? (y/n) ";
            prompt>> cont;
            std::cout<< std::endl;
            if(cont == "y")
                break;
            else if(cont != "n"){
                std::cout<< "INVALID - enter 'y' or 'n'" << std::endl;
                goto exit_prompt;
            }
        }
    }
    return 0; 
}


namespace{
  

template<int W, const char FILL>
void
_display_header_line(std::string pre, std::string post, std::string text)
{
  int f = W - (pre.size() + post.size() + text.size()) - 1; // account for newline
  if(f < 0)
      throw std::exception("can't fit header line");
  
  std::cout<< pre << std::string(int(f/2), FILL) 
           << text << std::string(f - int(f/2), FILL) 
           << post << std::endl;
}

template<int W, int INDENT, char H>
void
_display_header(std::string hpre, std::string hpost)
{
    char lpath[MAX_PATH+40+40];  
    TOSDB_GetClientLogPath(lpath,MAX_PATH+40+40);   

    _display_header_line<W, H>(hpre,hpost, "");
    _display_header_line<W, H>(hpre,hpost, "");
    _display_header_line<W,' '>(hpre,hpost, "");
    _display_header_line<W,' '>(hpre,hpost, "Welcome to the TOS-DataBridge Command Shell");
    _display_header_line<W,' '>(hpre,hpost, "Copyright (C) 2014 Jonathon Ogden");
    _display_header_line<W,' '>(hpre,hpost, "");
    _display_header_line<W, H>(hpre,hpost, "");
    _display_header_line<W, H>(hpre,hpost, "");
    _display_header_line<W,' '>(hpre,hpost, "");
    _display_header_line<W,' '>(hpre,hpost, "This program is distributed WITHOUT ANY WARRANTY.");
    _display_header_line<W,' '>(hpre,hpost, "See the GNU General Public License for more details.");
    _display_header_line<W,' '>(hpre,hpost, "");
    _display_header_line<W,' '>(hpre,hpost, "Use 'Connect' command to connect to the Service.");
    _display_header_line<W,' '>(hpre,hpost, "Use 'commands' for a list of commands by category.");
    _display_header_line<W,' '>(hpre,hpost, "Use 'topics' for a list of topics(fields) TOS accepts.");
    _display_header_line<W,' '>(hpre,hpost, "Use 'exit' to exit.");
    _display_header_line<W,' '>(hpre,hpost, "");
    _display_header_line<W,' '>(hpre,hpost, "NOTE: Topics/Items are case sensitive; use upper-case");
    _display_header_line<W,' '>(hpre,hpost, "");
    _display_header_line<W, H>(hpre,hpost, "");           
    _display_header_line<W, H>(hpre,hpost, "");
        
    int wc = INDENT;

    std::cout<< std::endl << std::setw(INDENT) << std::left << "LOG: ";    
    for(auto s : std::string(lpath)){
        if(++wc >= MAX_DISPLAY_WIDTH){
            std::cout<< std::endl << std::string(INDENT,' ') ;
            wc = INDENT;
        }
        std::cout<< s;
    }

    std::cout<< std::endl << std::endl << std::setw(INDENT) << std::left 
             << "PID: " << GetCurrentProcessId() << std::endl;

    commands_local["topics"](nullptr); 
}

};
