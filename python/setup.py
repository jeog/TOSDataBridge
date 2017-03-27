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

import sys as _sys

if _sys.version_info.major < 3:
    _sys.stderr.write("fatal: tosdb is built for python3!\n")
    exit(1)

from distutils.core import setup as _setup
from distutils.command.build import build as _Build
from distutils.command.install import install as _Install
from distutils.command.clean import clean as _Clean
from re import match as _match, search as _search
from time import asctime as _asctime
from os.path import join as _path_join, dirname as _dirname, realpath as _realpath                    
from os import system as _system, getcwd as _getcwd, remove as _remove
from shutil import rmtree as _rmtree


NAME = 'tosdb'
VERSION = '0.8'
DESCRIPTION = "Python Front-End / Wrapper for TOSDataBridge"
AUTHOR = "Jonathon Ogden"
AUTHOR_EMAIL = "jeog.dev@gmail.com"
PACKAGES = ['tosdb','tosdb/cli_scripts','tosdb/intervalize']  


_AUTO_EXT = '_tosdb' 
# everything should be relative to the python/setup.py
_OUR_PATH = _dirname(_realpath(__file__))
_HEADER_NAME = 'tos_databridge.h'
_HEADER_PATH = _path_join(_OUR_PATH, '..', 'include', _HEADER_NAME)
_OUTPUT_PATH = _path_join(_OUR_PATH, NAME, _AUTO_EXT + '.py')

if _OUR_PATH != _getcwd():
    _sys.stderr.write("fatal: setup.py must be run from its own directory(python/)\n")
    exit(1)


#string that should bookmark the topics in Topic_Enum_Wrapper::TOPICS<T> 
_MAGIC_TOPIC_STR = 'ksxaw9834hr84hf;esij?><'

#regex for finding our header #define consts 
#TODO: adjust so we can pull non-ints
_REGEX_HEADER_CONST = "#define[\s]+([\w]+)[\s]+.*?(-?[\d][\w]*)"

#adjust for topics we had to permute to form valid enum vars
TOPIC_VAL_REPLACE = {'HIGH52':'52HIGH','LOW52':'52LOW'}

#exclude certain header consts
HEADER_PREFIX_EXCLUDES = ['TOSDB_SIG_', 'TOSDB_COMM_', 'TOSDB_PROBE_',
                          'LOG_BACKEND_MUTEX_NAME', 'LOCAL_LOG_PATH', 'TOSDB_SHEM_BUF_SZ']


class TOSDB_SetupError(Exception):
    def __init__(self,*msgs):
        super().__init__(*msgs)  


def _on_exclude_list(h):
    for exc in HEADER_PREFIX_EXCLUDES:
        if _match(exc,h):
            return True
    return False

    
def _pull_consts_from_header(verbose=True):
    consts = []
    lineno = 0
    with open(_HEADER_PATH,'r') as hfile:    
        hfile.seek(0)    
        for hline in hfile:
            lineno += 1
            try:
                groups = _match(_REGEX_HEADER_CONST,hline).groups()        
            except AttributeError:
                continue    
            try:
                g0, g1 = groups
                #custom excluded consts
                if _on_exclude_list(g0):
                    continue
                val = str(hex(int(g1,16)) if 'x' in g1 else int(g1))
                consts.append((g0,val))
                if verbose:
                    print(' ',_HEADER_NAME + ':' + str(lineno)+ ': '+ g0, val)
            except ValueError:
                raise TOSDB_SetupError("invalid header const value", str(g0))   
            except Exception as e:
                raise TOSDB_SetupError("couldn't extract const from regex match", e.args)   
    return consts     


def _pull_topics_from_header(verbose=True):
    read_flag = False
    topics = []
    lineno = 0
    with open(_HEADER_PATH,'r') as hfile:
        for hline in hfile:
            lineno += 1
            if read_flag:      
                if _MAGIC_TOPIC_STR in hline:
                    read_flag = False
                    if verbose:
                        print(' ', _HEADER_NAME + ':' + str(lineno+1) + ': topic enum END')  
                    break         
                try:
                    t = hline.split('=')[0].strip()
                except Exception as e:
                    raise TOSDB_SetupError("failed to parse topic enum line", e.args)
                if not _search('[\W]',t):
                    topics.append(t)         
            else:
                if _MAGIC_TOPIC_STR in hline:
                    read_flag = True
                    if verbose:
                        print(' ', _HEADER_NAME + ':' + str(lineno-1) + ': topic enum BEGIN') 
    return topics


def _build_error_lookup(consts):
    errcs = {}
    for (k,v) in consts:
        if _match("TOSDB_ERROR_.+", k):
            errcs[v] = k.lstrip('TOSDB_')
    return errcs

    
# build a tosdb/_tosdb.py file from the header extracted vals
def _create__tosdb(consts, topics):
    error_consts = _build_error_lookup(consts)
    topic_dict = dict(zip(topics,topics))
    for key in topic_dict:
        if key in TOPIC_VAL_REPLACE: # don't just .update, check all are valid
            topic_dict[key] = TOPIC_VAL_REPLACE[key]    
    with open(_OUTPUT_PATH,'w') as pfile:
        pfile.write('# AUTO-GENERATED BY tosdb/setup.py\n')
        pfile.write('# DO NOT EDIT!\n\n')
        pfile.write('_BUILD_DATETIME = "' + _asctime() + '"\n')
        for k,v in consts:
            pfile.write(k.replace('TOSDB_','',1) + ' = ' + v + '\n')      
        pfile.write('\n')
        pfile.write('from tosdb.meta_enum import MetaEnum\n')
        pfile.write('class TOPICS(metaclass=MetaEnum):\n')
        pfile.write('  fields = ' + str(topic_dict) + '\n')
        pfile.write('\n')
        pfile.write('ERROR_LOOKUP = { \n')
        for k,v in error_consts.items():           
            pfile.write( str(int(k)) + ": '" + v + "', \n" )
        pfile.write('} \n')
        
       

def _init_build():
    print("pulling constants from " + _HEADER_PATH)
    consts = _pull_consts_from_header()
    print("pulling topic enum from " + _HEADER_PATH) 
    topics = _pull_topics_from_header()
    print('auto-generating ' + _OUTPUT_PATH)
    _create__tosdb(consts, topics)
    print('  checking ' + _OUTPUT_PATH)
    try:
        exec("from " + NAME + " import " + _AUTO_EXT)
    except ImportError as ie:
        print('    fatal: auto-generated ' + _OUTPUT_PATH + ' could not be imported !')
        print('    fatal: ' + ie.args[0])
        exit(1)
    print('    success!')



def _check_depends(): #TODO
    print("\n*** NOTE: if you plan on using authenticated virtualization " +
          "(by passing a password to the appropriate 'virtual' calls) " +
          "you need to install the pycrypto package ***")


class InstallCommand(_Install):
    description = "install tosdb"
    def run(self):
        super().run()
        _check_depends()

  
class BuildCommand(_Build):
    description = "build tosdb"
    def run(self):
        _init_build()
        super().run()        


class CleanCommand(_Clean):
    description = "clean tosdb"
    def run(self):                  
        try:
            print("removing tosdb/_tosdb.py ...")
            _remove(_OUTPUT_PATH)
        except:
            pass        
        try:
            print("removing ./build ...")
            _rmtree( _path_join(_OUR_PATH,'build') )
        except:
            pass              
        super().run()  


_setup( name=NAME, version=VERSION, description=DESCRIPTION, 
        author=AUTHOR, author_email=AUTHOR_EMAIL, packages=PACKAGES,
        cmdclass={'install':InstallCommand, 'build':BuildCommand, 'clean':CleanCommand} )


