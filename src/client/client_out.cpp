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

#include "tos_databridge.h"

/* THIS COULD USE SOME WORK */

std::ostream& 
operator<<(std::ostream& out, const generic_type& gen)
{
    out<< gen.as_string();
    return out;
}


std::ostream& 
operator<<(std::ostream& out, const DateTimeStamp& dts)
{
    out << dts.ctime_struct.tm_mon+1 << '/'
        << dts.ctime_struct.tm_mday << '/'
        << dts.ctime_struct.tm_year+1900 <<' '
        << dts.ctime_struct.tm_hour<<':'
        << dts.ctime_struct.tm_min<<':'
        << dts.ctime_struct.tm_sec<<':'
        << dts.micro_second;

    return out;
}


std::ostream& 
operator<<(std::ostream& out, const DateTimeStamp* dts)
{
    if(dts){       
        out << dts->ctime_struct.tm_mon+1 << '/'
            << dts->ctime_struct.tm_mday << '/'
            << dts->ctime_struct.tm_year+1900 <<' '
            << dts->ctime_struct.tm_hour<<':'
            << dts->ctime_struct.tm_min<<':'
            << dts->ctime_struct.tm_sec<<':'
            << dts->micro_second;
    }

    return out;
}

std::ostream& 
operator<<(std::ostream& out, const generic_matrix_type& mat)
{  
    for(auto & item : mat){
        std::cout<< item.first << " ::: ";
        for(auto & topic : item.second)    
            std::cout<< topic.first << "[" << topic.second<< "] ";         
        out<<std::endl; 
    }
    return out;
}


std::ostream& 
operator<<(std::ostream& out, const generic_dts_matrix_type& mat)
{  
    for(auto & item : mat){
        std::cout<< item.first << " ::: ";
        for(auto & topic : item.second)    
            std::cout<< topic.first << "[" << topic.second << "] ";    
        out<<std::endl; 
    }
    return out;
}


std::ostream& 
operator<<(std::ostream& out, const generic_map_type::value_type& sv)
{
    out<< sv.first << ' ' << sv.second;
    return out;  
}


std::ostream& 
operator<<(std::ostream& out, const generic_dts_type& sv)
{
    out<< sv.first << ' ' << sv.second;
    return out;  
}


std::ostream& 
operator<<(std::ostream& out, const dts_vector_type& vec)
{
    for(const auto & v : vec) 
        out<< vec << ' '; 
    return out;
}


std::ostream & 
operator<<(std::ostream& out, const generic_vector_type& vec)
{
    for(const auto & v : vec) 
       out<< v << ' '; 
    return out;
}


std::ostream & 
operator<<(std::ostream& out, const generic_dts_vectors_type& vecs)
{
    auto f_iter = vecs.first.cbegin();
    auto s_iter = vecs.second.cbegin();

    for( ; 
         f_iter != vecs.first.cend() && s_iter != vecs.second.cend(); 
         f_iter++, s_iter++)
    {
         out<< *f_iter <<' '<< *s_iter <<std::endl;
    }

    return out;
}


std::ostream & 
operator<<(std::ostream& out, const generic_map_type& dict)
{
    for(const auto & x : dict) 
        out<< x << std::endl;
    return out;
}


std::ostream & 
operator<<(std::ostream& out, const generic_dts_map_type& dict)
{
    for(const auto & x : dict) 
      out<< x.first << ' ' << x.second << std::endl;
    return out;
}


template<typename T> 
std::ostream& 
operator<<(std::ostream& out, const std::pair<T,DateTimeStamp>& pair)
{
    out<< pair.first <<' '<< pair.second;
    return out;
}


template<typename T> 
std::ostream& 
operator<<(std::ostream& out, const std::vector<T>& vec)
{
    for(const auto & v : vec) 
        out<< v << ' '; 
    return out;
}


template<typename T> 
std::ostream& 
operator<<(std::ostream& out, const std::pair<std::vector<T>,dts_vector_type>& vecs)
{
    auto f_iter = vecs.first.cbegin();
    auto s_iter = vecs.second.cbegin();

    for( ; 
         f_iter != vecs.first.cend() && s_iter != vecs.second.cend(); 
         f_iter++, s_iter++)
    {
        out<< generic_type(*f_iter) << ' ' << *s_iter << std::endl;
    }

    return out;
}
