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
    certain 'container' objects if initializer lists are not supported. It accepts a 
    custom template class that returns a functor to transform each entry. It also also
    accepts a template class that needs to overload assignment and should behave 
    like (the default) insert iterator, controling over how each element is inserted.
    
    The 'container' MUST expose an appropriate public 'value_type' typedef 

    Ex. 

    std::map<std::string, int> my_map_type;
    
    template<typename RetTy, typename ArgTy>
    struct MyTransform{
        RetTy
        operator()(ArgTy i)
        {
            int p = std::get<1>(i) * std::get<2>(i) * std::get<3>(i);
            return std::make_pair(std::get<0>(i), p);
        }  
    };

};

    const my_map_type my_map = 
        InitializerChain<my_map_type, MyTransform>('one',1,1,1)('two',2,2,2)('three',3,3,3);

    This is equivalent to:

        const my_map_type my_map = { {'one',3}, {'two',8}, {'three',27} };  

    * * *

    A standard InitList like approach would just use the default template args:

    const my_map_type my_simple_map = 
        InitializerChain<my_map_type>('one',3)('two',8)('three',27);

*/


 

template<typename RetTy, typename ArgTy>
struct NoTransform{
    RetTy
    operator()(ArgTy arg)
    {
        return arg;
    }
};   


template<typename T, 
         template<typename RetTy, typename ArgTy> class F = NoTransform,
         template<typename ContainerTy> class I = std::insert_iterator>
class InitializerChain {
    T _t;  

public:
    template<typename Arg>
    explicit 
    InitializerChain(Arg arg)                                    
        { 
            I<T>(_t,_t.end()) = F<T::value_type,Arg>()(arg);            
        }

    template<typename Arg>
    InitializerChain& 
    operator()(Arg arg) 
    {
        I<T>(_t,_t.end()) = F<T::value_type,Arg>()(arg);
        return *this;
    }  

    operator T() const
    {
        return _t;
    }
};


#endif JO_TOSDB_INITIALIZER_CHAIN