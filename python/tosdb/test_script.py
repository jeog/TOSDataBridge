import tosdb.intervalize as intrvl
import time

def init():
    intrvl.tosdb.init(root="C:/users/j/documents/github/tosdatabridge")
   # time.sleep(1)
    b = intrvl.tosdb.TOS_DataBlock(1000,date_time=True)    
    b.add_items("xlu","xlv")
    b.add_topics("last")             
    return intrvl.GetOnTimeInterval.send_to_file( b, 'xlu','last',
                                                  "./test_db_file.txt",
                                                  intrvl.TimeInterval.min,
                                                  5, True )
    

