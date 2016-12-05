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

void local_topics(CommandCtx *ctx);
void local_commands(CommandCtx *ctx);
void local_exit(CommandCtx *ctx);


commands_map_ty
build_commands_map()
{
    commands_map_ty m;

    m.insert( build_commands_map_elem("topics",local_topics) );
    m.insert( build_commands_map_elem("commands",local_commands) );
    m.insert( build_commands_map_elem("quit",local_exit) );
    m.insert( build_commands_map_elem("exit",local_exit) );
    m.insert( build_commands_map_elem("close",local_exit) );
    
    return m;
}

};

commands_map_ty commands_local = build_commands_map();

namespace{

void 
local_exit(CommandCtx *ctx)
{
    ctx->exit = true;       
}

void
local_topics(CommandCtx *ctx)
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
}


void 
local_commands(CommandCtx *ctx)
{
    std::string cmd;

    int count = CMD_OUT_PER_PAGE;   
    auto reset_count = [&count](){ count = CMD_OUT_PER_PAGE; };

    std::cout<< std::endl << "What type of command?" << std::endl << std::endl;
    for(auto & p : commands){
        std::cout<< "- " << p.first << std::endl;
    }
    std::cout<< std::endl << "- back" << std::endl << std::endl;

    while(1){
        prompt_b<< "commands" >> cmd;

        if(cmd == "exit"){
            ctx->exit = true;       
            return;
        }     

        if(cmd == "back")
            return;      

        for(auto & p : commands){
            if(cmd == p.first){
                std::cout<< std::endl << p.second.first << " COMMANDS:" 
                         << std::endl << std::endl;
                for(auto & pp : p.second.second){                  
                    if( !decr_page_latch_and_wait(&count, reset_count) )
                        break;
                    std::cout<<"- "<< pp.first << std::endl;
                }
                std::cout<< std::endl;
                return;
            }
        }

        std::cout<< std::endl << "BAD COMMAND" << std::endl << std::endl ;                   
    }
}

};