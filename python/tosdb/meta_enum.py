class MetaEnum(type):
    """ metaclass that creates an enum object w/ user-defined values
    
    class Color(metaclass=Enum):      
        fields = ( 'Red', 'Blue', 'Green' )

    class Speed(metaclass=Enum):      
        fields = { 'fast':300, 'moderate':200, 'slow':100, 'default':300 }  
    
    >>> Speed.fast.val
    300  
    >>> Speed.fast.name
    'fast'  
    >>> str( Speed.fast )
    '300'  
    >>> Speed.fast in Speed: # check if we belong to Speed
    True
    >>> Speed.fast == Speed.default: # compare values (only if we have them)
    True
    >>> Speed.fast is Speed.default: # same values but different objects
    False  

    raises Enum.EnumError(Exception)
    """      

    class EnumError(Exception):
        def __init__(self,*msgs):
            Exception.__init__(self,*msgs)
  
    def __new__(cls,name,bases,d):
        from collections import Mapping         
            
        # restrict subclassing
        for b in bases:              
            if isinstance(b,MetaEnum):        
                raise MetaEnum.EnumError(b.__name__ + ' can not be subclassed')
  
        # cls needs a fields attribute that defines '__iter__'
        if 'fields' not in d:
            raise MetaEnum.EnumError('class using metaclass Enum must define fields')
        if not hasattr(d['fields'], '__iter__'):
            raise MetaEnum.EnumError( str(cls) + '.fields must define __iter__' )   

        def _field_eq(self, other):          
            try:
                return self._enum_class == other._enum_class and \
                       self._val is not None and self._val == other._val                   
            except:               
                return False                                 
   
        #our field type
        our_field_dict = {    
            '__str__': lambda s: str(s._val) if s._val is not None else '',                    
            '__eq__': _field_eq,  # <-- compare if of same enum and with vals        
            '_enum_class': None               
        }
        our_field_clss = type('EnumField',(), our_field_dict)

        def _field_prop_no_set(self,obj,value):        
           raise MetaEnum.EnumError("can't set constant enum field")

        # our 'property' descriptor to protect the field
        our_field_prop_dict = {
            '__init__': lambda self, obj: setattr(self,'_obj',obj),
            '__get__':  lambda self, obj, objtype: self._obj,
            '__set__': _field_prop_no_set,  
            '_set_enum_class': lambda self, t: setattr(self._obj,'_enum_class',t)                     
        }       
        our_field_prop_clss = type('EnumFieldProperty',(), our_field_prop_dict)        

        # remove fields
        fields = d.pop('fields')  

        for n in fields:           
            if not isinstance(n,str):
                raise MetaEnum.EnumError( 'Enum names must be strings' ) 
            # create an EnumField instance for each name
            obj = our_field_clss()  
            obj._name = n             
            if isinstance( fields, Mapping):  
                # attach a val if fields provides a mapping to one                       
                obj._val = fields[n]
                if not hasattr( obj._val, '__str__'):
                     raise MetaEnum.EnumError( 'Enum value must define __str__') 
            else:
                obj._val = None 
            # bind stateful descriptors of same object in metaclass and class                
            setattr(cls,n, our_field_prop_clss( obj ) )                     
            d[n] = our_field_prop_clss( obj ) 

        # create name/val properties for each field
        our_field_clss.name = property( lambda self : self._name )
        our_field_clss.val = property( lambda self : self._val )                         

        # no need to instantiate the class
        def _no_init(self,*val): 
            raise MetaEnum.EnumError('Enum ' + name + ' can not be instantiated')
        d['__init__'] = _no_init    
    
        # return an iter of the EnumFields to support 'in' built-in
        def _iter(cls): 
            return iter([ getattr(cls,f) for f in dir(cls) 
                          if isinstance(getattr(cls,f), our_field_clss) ])
        d['_iter'] = classmethod(_iter)   

        # cleanup metaclass namespace if we can
        def _del(c):         
          for f in dir(c):
              if isinstance(getattr(c,f), our_field_clss):
                  delattr(cls,f)
        d['_del'] = classmethod(_del)

        # reattach a reverse lookup for convenience
        d['val_dict'] = { fields[k]:k for k in fields }
        
        # the finished class object
        clss = super(MetaEnum,cls).__new__(cls,name,bases,d)

        # bind a reference to it, in each EnumField, for compare ops
        for n in fields:           
            d[n]._set_enum_class(clss)
            getattr(cls,n)._enum_class = clss        
        
        return clss         

    def __iter__(self):
        # iterate over the enum_fields
        return self._iter()

    def __del__(self):
        # try to clean up our namespace
        self._del()


