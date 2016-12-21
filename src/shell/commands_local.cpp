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
#include <iomanip>

#include "shell.hpp"

namespace{

void local_topics(CommandCtx *ctx);
void local_commands(CommandCtx *ctx);
void local_exit(CommandCtx *ctx);
void local_language(CommandCtx *ctx);

}; /* namespace */

CommandsMap commands_local(
    CommandsMap::InitChain("topics",local_topics,"list TOPICS that TOS accepts") 
                          ("commands",local_commands, "list commands for this shell")
                          ("language",local_language, "set default language(C/C++/none)") 
                          ("exit",local_exit, "exit this shell") 
);


namespace{

bool
_decr_page_latch_and_wait(int *n, std::function<void(void)> cb_on_wait );

void
_display_default_language()
{
    auto l = language_strings[get_default_language()];

    std::cout<< std::endl << "Current default language: " << l.first << " (" << l.second << ")"  
             << std::endl << std::endl;
}

void 
local_exit(CommandCtx *ctx)
{
    ctx->exit = true;       
}

void 
local_language(CommandCtx *ctx)
{
    std::string input;
        
    _display_default_language();

    std::cout<< "Would you like to change default language?(y/n)";
    prompt>> input << stream_prompt::endl;

    if(input == "n")
        return;
    else if(input != "y"){
        std::cerr<< "INVALID - did not change default language" << std::endl << std::endl;
        return;
    }

    std::cout<< "Please select new default language:" << std::endl;
    for(auto & s : language_strings)
        std::cout<< "  - " << std::setw(8) << s.second.first 
                 << " - " << s.second.second << std::endl;
    std::cout<< std::endl;
    prompt>> input;

    for(auto & s : language_strings){
        if(input == s.second.first){
            set_default_language(s.first);           
            _display_default_language();           
            return;
        }
    }

    std::cerr<< std::endl << "INVALID (check case) - did not change default language" 
             << std::endl << std::endl;   
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
        if(wc + t.size() >= MAX_DISPLAY_WIDTH){ /* stay within maxw chars */
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
    int maxl;

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

        for(auto & group : commands){
            if(cmd == group.first){
                std::cout<< std::endl << group.second.first << " COMMANDS:" 
                         << std::endl << std::endl;
                maxl = 0;
                for(auto & c : group.second.second)
                    maxl = max(maxl, c.first.size());               
                for(auto & c : group.second.second){                  
                    if( !_decr_page_latch_and_wait(&count, reset_count) )
                        break;
                    std::cout<<"  - "<< std::setw(maxl + 2) << c.first 
                             << (c.second.doc.empty() ? "" : " - ") << c.second.doc << std::endl;
                }
                std::cout<< std::endl;
                return;
            }
        }

        std::cout<< std::endl << "BAD COMMAND" << std::endl << std::endl ;                   
    }
}

bool
_decr_page_latch_and_wait(int *n, std::function<void(void)> cb_on_wait )
{
    std::string more_y_or_n;

    if(*n > 0){
        --(*n);       
    }else{
        std::cout<< std::endl << "  More? (y/n) ";         
        prompt>> more_y_or_n;               
        cb_on_wait();
        if(more_y_or_n != "y") /* stop on everythin but 'y' */
            return false;
        std::cout<< std::endl;
    }
    return true;
}

}; /* namespace */