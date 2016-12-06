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
stream_prompt_basic prompt_b("[-->");

namespace{

language default_language = language::none;

template<int W, const char FILL>
void
_display_header_line(std::string pre, std::string post, std::string text);

template<int W, int INDENT, char H>
void
_display_header(std::string hpre, std::string hpost);

commands_map_of_maps_ty
build_commands_map_of_maps()
{
    typedef commands_map_of_maps_ty::value_type elem_ty;

    commands_map_of_maps_ty m;

    m.insert( elem_ty("admin", command_display_pair("ADMINISTRATIVE",commands_admin)) );
    m.insert( elem_ty("get", command_display_pair("GET",commands_get)) );
    m.insert( elem_ty("stream", command_display_pair("STREAM-SNAPSHOT",commands_stream)) );
    m.insert( elem_ty("frame", command_display_pair("FRAME",commands_frame)) );
    m.insert( elem_ty("local", command_display_pair("LOCAL",commands_local)) );

    return m;
}

};

commands_map_of_maps_ty commands = build_commands_map_of_maps();


int main(int argc, char* argv[])
{         
    std::string cmd;

    _display_header<MAX_DISPLAY_WIDTH, LEFT_INDENT_SIZE, '-'>("[--", "--]");

    while(1){ 
        new_command:
        try{
            prompt>> cmd;   
            
            for(auto & c : commands){
                for(auto & p : c.second.second){
                    p.second.exit = false;
                    if(p.first == cmd){                    
                        p.second.func(&p.second);
                        if(p.second.exit)
                            goto exit_prompt;
                        goto new_command;
                    }
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

    commands_local["topics"].func(nullptr); 
}

std::unordered_map<language, std::pair<std::string,std::string>>
build_language_strings()
{
    std::unordered_map<language, std::pair<std::string,std::string>> m;

    m.insert( std::make_pair(language::none, std::make_pair("NONE", "no default; ask user during call")) );
    m.insert( std::make_pair(language::c, std::make_pair("C", "use C version of call(if available)")) );
    m.insert( std::make_pair(language::cpp, std::make_pair("C++", "use C++ version of call(if available)")) );

    return m; 
}

};

std::unordered_map<language, std::pair<std::string,std::string>> 
language_strings = build_language_strings();

language
set_default_language(language l)
{
    language oldl = default_language;
    default_language = l;
    return oldl;
}


language
get_default_language()
{
    return default_language;
}


commands_map_ty::value_type
build_commands_map_elem(std::string name, commands_func_ty func, std::string doc)
{
    CommandCtx c = {name, doc, func, false};  
    auto v = commands_map_ty::value_type(name, std::move(c));
    return v;
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



