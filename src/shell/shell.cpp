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

stream_prompt prompt("[-->");
stream_prompt_basic prompt_b("[-->");

/* to avoid compile-time ordinality issues we pass the commands groups by 
   reference via CommandsMapRef */
commands_map_of_maps_ty 
commands = InitializerChain<commands_map_of_maps_ty>
    ( "admin", "ADMINISTRATIVE", CommandsMapRef(commands_admin) )
    ( "get", "GET", CommandsMapRef(commands_get) )
    ( "stream", "STREAM-SNAPSHOT", CommandsMapRef(commands_stream) )
    ( "frame", "FRAME", CommandsMapRef(commands_frame) )
    ( "local", "LOCAL", CommandsMapRef(commands_local) );

language_strings_ty 
language_strings = InitializerChain<language_strings_ty>
    ( language::none, "NONE", "no default; ask user during call")
    ( language::c, "C", "use C version of call(if available)")
    ( language::cpp, "C++", "use C++ version of call(if available)");


namespace{

template<int W, const char FILL>
void
_display_header_line(std::string pre, std::string post, std::string text);

template<int W, int INDENT, char H>
void
_display_header(std::string hpre, std::string hpost);

}; /* namespace */

int main(int argc, char* argv[])
{           
    std::string cmd;
   
    _display_header<MAX_DISPLAY_WIDTH, LEFT_INDENT_SIZE, '-'>("[--", "--]");

    while(1){ 
        new_command:
        try{
            prompt>> cmd;   
            
            for(auto & group : commands){
                for(auto & c : group.second.second){                    
                    if(c.first != cmd)
                        continue;
                    c.second.exit = false;
                    c.second.func(&c.second);                  
                    if(c.second.exit)
                        goto exit_prompt;
                    goto new_command;
                    
                }
            }      

            /* default to here */        
            std::cout<< std::endl << "BAD COMMAND" << std::endl << std::endl;
            
        }catch(const TOSDB_Error& e){             
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

language _default_language = language::none;

}; /* namespace */


language
set_default_language(language l)
{
    language oldl = _default_language;
    _default_language = l;
    return oldl;
}


language
get_default_language()
{
    return _default_language;
}


size_type 
get_cstr_entries(std::string label, char ***pstrs, CommandCtx *ctx)
{  
    std::string nentry;
    std::string item;

    prompt_for("# " + label + "s", &nentry, ctx);    

    unsigned long n = 0;
    try{
        n = std::stoul(nentry);
    }catch(...){
        std::cerr<< std::endl << "INVALID INPUT" << std::endl << std::endl;
        *pstrs = nullptr;
        return 0;
    }

    try{
        *pstrs = NewStrings(n, TOSDB_MAX_STR_SZ);

        for(size_type i = 0; i < n; ++i){
            prompt_b<< ctx->name;
            prompt<< label << "(" << (i+1) << ") " >> item;        
            strcpy_s((*pstrs)[i], item.length() + 1, item.c_str());
        }
    }catch(...){
        DeleteStrings(*pstrs, n);
        *pstrs = nullptr;
        return 0;
    }

    return n;
}


bool 
prompt_for_cpp(CommandCtx *ctx, int recurse) /*recurse=2*/
{
    std::string cpp_y_or_n;

    language l = get_default_language();

    if(l == language::c) 
        return false;
        
    if(l == language::cpp)
        return true;            

    prompt_for("use C++ version? (y/n)", &cpp_y_or_n, ctx);

    if(cpp_y_or_n == "y")
        return true;
    else if(cpp_y_or_n == "n")
        return false;
    else if(recurse > 0){
        std::cout<< "INVALID - enter 'y' or 'n'" << std::endl;
        return prompt_for_cpp(ctx, recurse-1);
    }else{
        std::cout<< "INVALID - defaulting to C version" << std::endl;
        return false;
    }
}


bool 
prompt_for_datetime(std::string block, CommandCtx *ctx)
{
    std::string dts_y_or_n;
  
    if( !TOSDB_IsUsingDateTime(block) )
        return false;          
   
    prompt_for("get datetime stamp? (y/n)", &dts_y_or_n, ctx);
                    
    if(dts_y_or_n == "y")
        return true;
    else if(dts_y_or_n != "n")              
        std::cout<< "INVALID - default to NO datetime" << std::endl;

    return false;    
}


void
prompt_for_block_item_topic(std::string *pblock, 
                            std::string *pitem, 
                            std::string *ptopic,
                            CommandCtx *ctx)
{
    prompt_for("block", pblock, ctx);
    prompt_for("item", pitem, ctx);    
    prompt_for("topic", ptopic, ctx);
}


void
prompt_for_block_item_topic_index(std::string *pblock, 
                                  std::string *pitem, 
                                  std::string *ptopic, 
                                  std::string *pindex, 
                                  CommandCtx *ctx)
{
    prompt_for_block_item_topic(pblock, pitem, ptopic, ctx);
    prompt_for("index", pindex, ctx); 
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

    commands_local.at("topics").func(nullptr); 
}

}; /* namespace */

