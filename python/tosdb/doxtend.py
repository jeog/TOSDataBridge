# Copyright (C) 2014 Jonathon Ogden     < jeog.dev@gmail.com >
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#   See the GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License,
#   'LICENSE.txt', along with this program.  If not, see 
#   <http://www.gnu.org/licenses/>.

from inspect import isfunction as _isfunction

class DoxtendError(Exception):
    def __init__(self, *msg):
        super().__init__(*msg)

def doxtend(*bases, func_name=None, sep='\n\n'):
    """ Inherit doc-string(s) from other functions/methods.  

    bases: base objects to search in, 'None' looks in globals()
    func_name: alternative name to search for(defaults to name of decorated function)
    sep: str to place between inherited docstring

    Attempts to order doc-strings in a cummulative/hierarchical fashion - moving
    left to right through 'bases' - excluding the same string(s) from different 
    objects(except for the final (non)derived doc-string which is guaranteed to 
    be included last)"
    """
    def doxtend_decorator(func):
        nonlocal bases, func_name
        uniq_doc_strs = []
        if func_name is None:
            func_name = func.__name__  
        for bc in bases:
            try:
                f = globals()[func_name] if bc is None else getattr(bc,func_name)
            except (KeyError, AttributeError):
                raise DoxtendError("couldn't find function '" + func_name + "'")
            if f.__doc__ not in uniq_doc_strs:
                uniq_doc_strs.append(f.__doc__)
        if uniq_doc_strs:
            if func.__doc__:
                uniq_doc_strs.append(func.__doc__)
            func.__doc__ = sep.join(uniq_doc_strs)   
        return func
    ### doxtend_decorator() ###
    
    if bases:
        if _isfunction(bases[0]):    
            f = bases[0] 
            bases = (None,)       
            return doxtend_decorator(f)
        else: 
            return doxtend_decorator
    else:
        bases = (None,)
        return doxtend_decorator



def _test():
    def f():
        """ (1, 5) global f() docstring """
        pass
      
    globals()['f'] = f

    class B:
        def f(self):
            """ B.f() docstring """
            pass
        def f2(self):
            """ (3) B.f2() docstring """
            pass

    class Z(B):
        @doxtend(B,func_name='f2',sep="\n seperator between B.f2() and Z.f() \n")
        def f(self):
            """ (4) Z.f() docstring """
            pass

    class D(B):
        @doxtend # <- looking in global by default
        def f(self):
            """ (2) D.f() docstring """
            pass

    class DD(D):
        @doxtend(D,Z,None,sep="\n seperator between D.f() / Z.f() / f() and DD.f() \n")
        def f(self):
          """ (6) DD.f() docstring """
          pass

    print('DD.f.__doc__:\n\n')
    print(DD.f.__doc__)
    globals().pop('f')
