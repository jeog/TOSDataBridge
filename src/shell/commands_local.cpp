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

#include <algorithm>

#include "shell.hpp"

namespace{

void local_topics(void *ctx=nullptr);
void local_commands(void *ctx=nullptr);
void local_exit(void *ctx=nullptr);


commands_map_ty
build_commands_map_local()
{
    commands_map_ty m;

    m.insert( commands_map_elem_ty("topics",local_topics) );
    m.insert( commands_map_elem_ty("commands",local_commands) );
    m.insert( commands_map_elem_ty("quit",local_exit) );
    m.insert( commands_map_elem_ty("exit",local_exit) );
    m.insert( commands_map_elem_ty("close",local_exit) );
    
    return m;
}

};

commands_map_ty commands_local = build_commands_map_local();

namespace{

void 
local_exit(void *ctx)
{
    *(bool*)ctx = false;        
}

void
local_topics(void *ctx)
{
    const int SPC = 3;  
    const std::string TOPIC_HEAD("TOPICS:   ");

    int wc;
    
    std::vector<std::string> topics;
    
    std::transform(
        TOS_Topics::MAP().cbegin(), 
        TOS_Topics::MAP().cend(), 
        std::insert_iterator<std::vector<std::string>>(topics,topics.begin()),
        [&](const TOS_Topics::topic_map_entry_type& e){ return e.second; }
    );

    std::sort(topics.begin(), topics.end(), std::less<std::string>());

    std::cout<< std::endl << TOPIC_HEAD;   
    wc = TOPIC_HEAD.size();

    for(auto & t : topics){
        if(t == " ") /* exclude NULL Topic */
            continue;       
        if(wc + t.size() > MAX_DISPLAY_WIDTH){ /* stay within maxw chars */
            std::cout<< std::endl << std::string(TOPIC_HEAD.size(),' ');
            wc = TOPIC_HEAD.size();
        }
        std::cout<< t;
        wc += t.size();
        if(wc + SPC < MAX_DISPLAY_WIDTH){
            std::cout<< std::string(SPC,' ');
            wc += SPC;
        }        
    }
    std::cout<< std::endl << std::endl;
    if(ctx)
        *(bool*)ctx = true;        
}


void 
local_commands(void *ctx)
{
    std::string cmd;
    int count = CMD_OUT_PER_PAGE;        

    std::cout<< std::endl << "What type of command?" << std::endl << std::endl
             << "- admin" << std::endl
             << "- get" << std::endl
             << "- stream" << std::endl
             << "- frame" << std::endl << std::endl
             << "- back" << std::endl << std::endl;

    prompt>> cmd;
    std::cin.get();

    if(cmd == "exit"){
        *(bool*)ctx = false;
        return;
    }       

    if(cmd == "admin"){     
        std::cout<< std::endl << "ADMINISTRATIVE COMMANDS:" << std::endl << std::endl;
        for(auto & p : commands_admin){                  
            if( !decr_page_latch_and_wait(&count, [&count](){ count = CMD_OUT_PER_PAGE; } ) )
                break;
            std::cout<<"- "<< p.first << std::endl;
        }
    }else if(cmd == "get"){  
        std::cout<< std::endl << "GET COMMANDS:" << std::endl << std::endl;
        for(auto & p : commands_get){             
            if( !decr_page_latch_and_wait(&count, [&count](){ count = CMD_OUT_PER_PAGE; } ) )
                break;     
            std::cout<<"- "<< p.first << std::endl;
        }    
    }else if(cmd == "stream"){  
        std::cout<< std::endl << "STREAM SNAPSHOT COMMANDS:" << std::endl << std::endl;
        for(auto & p : commands_stream){                  
            if( !decr_page_latch_and_wait(&count, [&count](){ count = CMD_OUT_PER_PAGE; } ) )
                break;   
            std::cout<<"- "<< p.first << std::endl;
        }  
    }else if(cmd == "frame"){  
        std::cout<< std::endl << "FRAME COMMANDS:" << std::endl << std::endl;
        for(auto & p : commands_frame){                 
            if( !decr_page_latch_and_wait(&count, [&count](){ count = CMD_OUT_PER_PAGE; } ) )
                break;       
            std::cout<<"- "<< p.second << std::endl;
        }                   
    }else if(cmd != "back"){
        std::cout<< std::endl << "BAD COMMAND" << std::endl ;        
    }
    
    std::cout<< std::endl;
    *(bool*)ctx = true;   
}

};