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
    certain 'container' objects if initializer lists are not supported. It accepts an 
    optional custom functor for transforming/inserting each element.
    
    The 'container' MUST expose an appropriate public 'value_type' typedef 

    Ex. 

    std::map<std::string, int> my_map_type;
    std::p<std::string, int, int> my_entry_type;
    
    void
    MyInserter(std::map<std::string, int>& m, my_entry_type e)
    {            
        m[std::get<0>(e)] = std::get<1>(e) * std::get<2>(e);            
    }   

    const my_map_type my_map = 
        InitializerChain<my_map_type,my_entry_type>(MyInserter)
            my_entry_type("one",1,1)
            my_entry_type("two",2,2)
            my_entry_type("three",3,3);

    This is equivalent to:

        const my_map_type my_map = { {"one",1}, {"two",4}, {"three",9} };   


    - OR - (in certain cases, like this one) we can use the default version:

    const my_map_type my_map = 
        InitializerChain<my_map_type>()
            ("one",1*1)
            ("two",2*2)
            ("three",3*3);

*/

template<typename T, typename A>
void
DefaultInsert(T& t, A a)
{
    t.insert(t.end(), a);
}

template<typename T, typename A = T::value_type, 
         /* standard insert available to most containers */          
         void(*func)(T&, A) = DefaultInsert<T,A> > 
class InitializerChain {
    T _t;   

public:   
   
    InitializerChain(A arg)     
        {  
            operator()(arg);
        }

    template<typename First, typename Second>
    InitializerChain(First f, Second s)     
        {  
            operator()(f, s);
        }

    template<typename First, typename Second, typename Third>
    InitializerChain(First f, Second s, Third t)     
        {  
            operator()(f, s, t);
        }

    InitializerChain& 
    operator()(A arg)  
    {
        func(_t, arg);
        return *this;       
    }  

    template<typename First, typename Second>
    InitializerChain& 
    operator()(First f, Second s)  
    {
        return operator()(std::pair<First,Second>(f,s));        
    }  

    template<typename First, typename Second, typename Third>
    InitializerChain& 
    operator()(First f, Second s, Third t)  
    {
        return operator()(f, std::pair<Second,Third>(s,t));        
    }  

    operator T() const
    {
        return _t;
    }
};


#endif JO_TOSDB_INITIALIZER_CHAIN