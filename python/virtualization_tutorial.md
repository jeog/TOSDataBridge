### tosdb/ tutorial 
---

This tutorial attempts to show simple example usage of the tosdb virtual layer. Please refer to *\__init__.\__doc__* for an explanation of the virtual layer and the myriad ways to initialize and use it, not show in this tutorial.

The screen shots are of the local/windows side. The code blocks are from the remote/linux side.

---

##### Local Interactive Init

![](./../res/vtut_loc_1.png)

```
jon@jdeb:~/dev/TOSDataBridge$ python3
>>> import tosdb
>>> vblock = tosdb.VTOSDB_DataBlock(('192.168.56.101',55555))
>>> tosdb.admin_init(('192.168.56.101',55555))
>>> tosdb.vconnected()
True
>>> del vblock
>>> tosdb.admin_close()
>>> exit()
```

##### Remote Interactive Init

![](./../res/vtut_loc_2.png)

```
jon@jdeb:~/dev/TOSDataBridge$ python3
>>> import tosdb
>>> tosdb.admin_init(('192.168.56.101',55555))
>>> tosdb.vinit(root="C:/TOSDataBridge")
True
>>> tosdb.vconnected()
True
>>> vblock = tosdb.VTOSDB_DataBlock(('192.168.56.101',55555))
>>> del vblock
>>> tosdb.vclean_up()
>>> tosdb.admin_close()
>>> exit()
```

##### Local Server Init 

![](./../res/vtut_loc_3.png)

```
jon@jdeb:~/dev/TOSDataBridge$ python3
>>> import tosdb
>>> vblock = tosdb.VTOSDB_DataBlock(('192.168.56.101',55555))
>>> tosdb.admin_init(('192.168.56.101',55555))
>>> tosdb.vconnected()
True
>>> del vblock
>>> tosdb.admin_close()
>>> exit()
``` 

*...or directly from the command line...*

```
jon@jdeb:~/dev/TOSDataBridge$ python3 python/tosdb --virtual-client "192.168.56.101 55555"
(InteractiveConsole)
>>> vconnected()
True
>>> exit()
```

