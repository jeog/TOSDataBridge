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

#include <python.h>
#include "tos_databridge.h"

/* 
    how we extend non-exported constants to the python library
    note: we have to compile as C++ to access the global Topic Mapping 
*/

const TOS_Topics::topic_map_type& 
TOS_Topics::globalTopicMap = TOS_Topics::_globalTopicMap;

const char* TOPICS_NAME      =  "TOPICS";
const char* INTGR_BIT_NAME   =  "INTGR_BIT";
const char* QUAD_BIT_NAME    =  "QUAD_BIT";
const char* STRING_BIT_NAME  =  "STRING_BIT";
const char* MAX_STR_SZ_NAME  =  "MAX_STR_SZ";
const char* STR_DATA_SZ_NAME =  "STR_DATA_SZ";
const char* DEF_TIMEOUT_NAME =  "DEF_TIMEOUT";

static struct PyModuleDef _tosdb = { PyModuleDef_HEAD_INIT, 
                                     "_tosdb", 
                                     NULL, 
                                     -1, 
                                     NULL };

PyMODINIT_FUNC PyInit__tosdb(void)
{	
    PyObject* pyMod = PyModule_Create(&_tosdb);	
    /* 
        our constant scalars 
    */
    PyObject* iBitObj = Py_BuildValue("i",TOSDB_INTGR_BIT);
    PyObject* qBitObj = Py_BuildValue("i",TOSDB_QUAD_BIT);
    PyObject* sBitObj = Py_BuildValue("i",TOSDB_STRING_BIT);
    PyObject* maxStrSzObj = Py_BuildValue("i",TOSDB_MAX_STR_SZ);
    PyObject* strDatSzObj = Py_BuildValue("i",TOSDB_STR_DATA_SZ);
    PyObject* defTOObj = Py_BuildValue("i",TOSDB_DEF_TIMEOUT);
    /* 
        our Topics enum as an immutable sequence 
    */
    PyObject* topicObj = 
        PyTuple_New( TOS_Topics::globalTopicMap.size() ); 

    TOS_Topics::topic_map_type::const_iterator1_type cbIter = 
        TOS_Topics::globalTopicMap.cbegin();

    int count = 0;
    do
        PyTuple_SET_ITEM( topicObj, 
                          count++, 
                          PyUnicode_FromString(cbIter->second.c_str()) );	
    while( ++cbIter != TOS_Topics::globalTopicMap.cend() );	

    PyObject_SetAttrString( pyMod, TOPICS_NAME, topicObj);
    PyObject_SetAttrString( pyMod, INTGR_BIT_NAME, iBitObj );
    PyObject_SetAttrString( pyMod, QUAD_BIT_NAME, qBitObj );
    PyObject_SetAttrString( pyMod, STRING_BIT_NAME, sBitObj );
    PyObject_SetAttrString( pyMod, MAX_STR_SZ_NAME, maxStrSzObj );
    PyObject_SetAttrString( pyMod, STR_DATA_SZ_NAME, strDatSzObj );
    PyObject_SetAttrString( pyMod, DEF_TIMEOUT_NAME, defTOObj );
    
    return pyMod;
}

