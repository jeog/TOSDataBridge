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

#ifndef JO_TOSDB_INITIALIZER_CHAIN
#define JO_TOSDB_INITIALIZER_CHAIN

/* 
    InitializerChain is a 'chaining' mechanism for inline global construction of 
    certain objects if initializer lists are not supported. It also accepts an 
    initialization function.
    
    The object must support insertion and expose a value_type typedef (map, vector etc.)

    Ex. 

    std::map<std::string, int> my_map_type;

    1) define a function that accepts one arg and returns the value_type(optional):
            
        my_map_type::value_type
        my_transform( std::tuple<std::string,int,int,int> i )
        {
            int p = std::get<1>(i) * std::get<2>(i) * std::get<3>(i);
            return std::make_pair(std::get<0>(i), p);
        }

    2) use the InitializerChain like so:

       const my_map_type my_map = 
           InitializerChain<my_map_type, 
                            std::tuple<std::string,int,int,int>, 
                            my_transform>('one',1,1,1)('two',2,2,2)('three',3,3,3);

    This is equivalent to:

        const my_map_type my_map = { {'one',3}, {'two',8}, {'three',27} };      
*/

template<typename T, typename Arg>
inline T
NoTransform(Arg arg)
{    
    return arg;
}

template<typename T, typename Arg, 
         typename T::value_type(*func)(Arg)=NoTransform<typename T::value_type,Arg>>
class InitializerChain{
    T _t;  

public:
    explicit 
    InitializerChain(Arg arg)                                    
        { 
            std::inserter(_t,_t.end()) = func(arg);
        }

    InitializerChain& 
    operator()(Arg arg) 
    {
        std::inserter(_t,_t.end()) = func(arg);
        return *this;
    }  

    operator T() const
    {
        return _t;
    }
};


#endif JO_TOSDB_INITIALIZER_CHAIN