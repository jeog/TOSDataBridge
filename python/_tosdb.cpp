/* 
Copyright (C) 2014 Jonathon Ogden	< jeog.dev@gmail.com >

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

#include <Python.h>
#include "tos_databridge.h"

/* 
 * how we extend non-exported constants to the python library
 * note: have to compile as C++ (on Windows)to access the global Topic Mapping 
*/

#ifndef XPLATFORM_PYTHON_CONSTS_ONLY
const TOS_Topics::topic_map_type& TOS_Topics::map = TOS_Topics::_map;
#else
/* no define since we're linking with topics.o instead of static.lib*/
#endif

const char* TOPICS_NAME      = "TOPICS";
const char* DEF_TIMEOUT_NAME = "DEF_TIMEOUT";  
const char* INTGR_BIT_NAME   = "INTGR_BIT";
const char* QUAD_BIT_NAME    = "QUAD_BIT";
const char* STRING_BIT_NAME  = "STRING_BIT";
const char* MAX_STR_SZ_NAME  = "MAX_STR_SZ";
const char* STR_DATA_SZ_NAME = "STR_DATA_SZ";

static struct PyModuleDef _tosdb = { 
  PyModuleDef_HEAD_INIT, 
  "_tosdb", 
  NULL, 
  -1, 
  NULL 
};

PyMODINIT_FUNC PyInit__tosdb(void)
{	
  PyObject* pyMod = PyModule_Create(&_tosdb);	

  /* constant scalars */
  PyObject* i_bit       = Py_BuildValue("i",TOSDB_INTGR_BIT);
  PyObject* q_bit       = Py_BuildValue("i",TOSDB_QUAD_BIT);
  PyObject* s_bit       = Py_BuildValue("i",TOSDB_STRING_BIT);
  PyObject* max_str_sz  = Py_BuildValue("i",TOSDB_MAX_STR_SZ);
  PyObject* str_data_sz = Py_BuildValue("i",TOSDB_STR_DATA_SZ);  
  PyObject* def_timeout = Py_BuildValue("i",TOSDB_DEF_TIMEOUT);   

  /* Topics enum as an immutable sequence */
  PyObject* topicObj = PyTuple_New(TOS_Topics::map.size()); 

  TOS_Topics::topic_map_type::const_iterator1_type cbIter = 
    TOS_Topics::map.cbegin();

  int count = 0;
  do{
    PyTuple_SET_ITEM(topicObj, count++, 
                     PyUnicode_FromString(cbIter->second.c_str()));	
  }while(++cbIter != TOS_Topics::map.cend());	

  PyObject_SetAttrString(pyMod, TOPICS_NAME, topicObj); 
  PyObject_SetAttrString(pyMod, DEF_TIMEOUT_NAME, def_timeout);
  PyObject_SetAttrString(pyMod, INTGR_BIT_NAME, i_bit);
  PyObject_SetAttrString(pyMod, QUAD_BIT_NAME, q_bit);
  PyObject_SetAttrString(pyMod, STRING_BIT_NAME, s_bit);
  PyObject_SetAttrString(pyMod, MAX_STR_SZ_NAME, max_str_sz);
  PyObject_SetAttrString(pyMod, STR_DATA_SZ_NAME, str_data_sz);
  
  return pyMod;
}

