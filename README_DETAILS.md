### Troubleshooting
- - -

##### Connection Problems

TOSDataBridge needs to 1) connect the client program (e.g. python interpreter) to the Service/Engine backend, and 2) connect the Service/Engine backend to the TOS platform. If either of these steps fails you'll generally be alerted by an error code, exception, or error message, depending on the language.

If you have trouble with (1) connecting the client to the Service/Engine be sure:

- you have run the tosdb-setup.bat install script (and it doesn't output any error messages).
- you have started the service ('SC start TOSDataBridge') and it's running. 'SC query TOSDataBridge' will provide the status.
- you are using the correct version of the client library (e.g. branch 0.7 with a 64-bit Service/Engine and 64-bit client should be using tos-databridge-0.7-x64.dll)

If you can connect to the Service/Engine but have trouble with (2) connecting to the TOS platform be sure:

- you passed 'admin' to the tosdb-setup.bat script if your TOS runs with elevated privileges
- you are running the Service/Engine in the appropriate session. 
     1. open up cmd.exe 
     2. enter 'tasklist' 
     3. find thinkorswim and tos-databridge-engine on the list
     4. if their Session# fields don't match you need to [re-run the tosdb-setup.bat script](README.md#quick-setup) with a custom session arg that matches the Session# of thinkorswim

If none of that solves the problem be sure:
- your anti-virus isn't sandboxing/quarantining any of the binaries and/or blocking communication between them
- to check the log files in /log for more information

If you are still having issues please [file a new issue](issues/new); you may have run into a serious bug that we'd like to fix.


### Important Details 
- - -
- **Exiting Gracefully** You want to avoid shutting down the TOS platform and/or the Service while your client code is running. As a general rule follow this order: 

    1. Clean up: close blocks, call clean_up etc. 
    2. Exit your program
    3. Stop the Service (optional)
    4. Close TOS platform

    Once you've done (i) the remaining order is less important. 

- **DDE Data:** It's important to realize that we (us, you, this code, and yours) are at the mercy of the TOS platform, the DDE technology, and TOS's implementation of it. You may notice the streams of data will not match the Time & Sales perfectly.  If you take a step back and aggregate the data in more usable forms this really shouldn't be an issue.  Another way we are at the mercy of the DDE server is that fields like last price and last size are two different topics. That means they are changing at slightly different times with slightly different time-stamps even if they are the correct pairing. To get around this we can write code like this to simulate a live-stream :
    ```
    >>> block = tosdb.TOSDB_DataBlock()
    >>> block.add_items('GOOG')
    >>> block.add_topics('LAST','VOLUME')
    >>> vol = 0
    >>>
    >>> print("price".ljust(10), "size") 
    >>> while(run_flag):
    ...     v = block.get('GOOG','VOLUME')
    ...     p = block.get('GOOG','LAST')
    ...     # if total volume has changed, output the difference
    ...     if(v != vol): 
    ...         print(str(p).ljust(10), str(v - vol))
    ...     vol = v
    ...     # the less we sleep the more accurate the stream
    ...     time.sleep(.1)
    ```

- **Numerical Values for 'Non-Decimal' Instruments:** The TOS DDE server doesn't handle numerical values for 'non-decimal' instruments well. For instance, trying to get a bid/ask/last etc. for a 10-yr note future (/zn) will not return the fractional part of the price. In these cases use the topic versions suffixed with an x (bidx/askx/lastx etc.) which will provide a string you'll need to parse.

- **Case-Sensitivity:** Case Sensitivity is a minor issues with how Item values are handled. The underlying C/C++ library are case-sensitive; it's up to the client to make sure they are passing case consistent item strings. The Python and Java wrappers are case-insensitive; on receiving item and topic strings they are converted to upper-case by default.

- **Closing Large Blocks:** Currently Closing/Destroying large blocks(1,000,000+ combined data-stream elements) involves a large number of internal deallocations/destructions and becomes quite CPU intensive. The process is spun-off into its own thread but this may fail, returning to the main thread when the library is being freed, or block the python interpreter regardless of when or how it's called. Use caution when creating/closing large blocks.

- **Block Size and Memory:** As mentioned you need to use some sense when creating blocks. As a simple example: let's say you want LAST,BID,ASK for 100 symbols. If you were to create a block of size 1,000,000, WITHOUT DateTime, you would need to allocate over 2.4 GB of memory - not good. As a general rule keep data-streams of similar desired size in the same block, and use new blocks as necessary. In our example if you only need 1,000,000 LAST elems for 10 items and 100 for everything else create 2 blocks: 1) a block of size 1,000,000 with those 10 items and the topic LAST; 2) a block of size 100 with all 100 items and the three topics. Then you would only need a little over 80 MB. (Really you could create three blocks to avoid any overlap but in this case its only a minor performance and space improvement and not worth worth the inconvenience.)

- **Asymmetric Responsibilities & Leaks:** Connection 'probing' only works one way, from master to slave. If the master disconnects abruptly, without sending the requisite remove signal(s), the slave will continue to maintain the client/master's streams. Internally, all it does is keep a ref-count to the streams it's been asked to create and obviously write the necessary data into the appropriate shared memory segments. To see the status of the service and if there are stranded or dangling streams open up the command shell and use **`DumpBufferStatus`** to dump all the current stream information to /log . 

- **DateTimeStamp:** THESE ARE NOT OFFICIAL STAMPS FROM THE EXCHANGE, they are manually created once the TOS DDE server returns the data. They use the system clock to assure high_resolution( the micro-seconds field) and therefore there is no guarantee that the clock is accurate or won't change between stamps, as is made by the STL's std::steady_clock. 

- **Bad Items & Topics:** The implementation can easily handle bad topics passed to a call since it has a large mapping of the allowed topic strings mapped to enum values for TOS_Topics. If a passed string is mapped to a NULL_TOPIC then it can be rejected, or even if it is passed it won't get a positive ACK from the server and should be rejected. Bad item strings on the other hand are a bit of a problem. The DDE server is supposed to respond with a negative ACK if the item is invalid but TOS responds with a positive ACK and a 'N/A' string. **Currently it's up to the user to deal with passing bad Items.** 

- **Pre-Caching:** As mentioned the block requires at least one valid topic AND item, otherwise it can't hold a data-stream. Because of this, if only items(topics) are added they are held in a pre-cache until a valid topic(item) is added. Likewise, if all topics(items) are removed, the remaining items(topics) are moved into the pre-cache. This has two important consequences: 1) pre-cached entries are assumed to be valid until they come out and are sent to the engine and 2) when using the API calls or python methods to see the items/topics currently in the 'block' you WILL NOT see pre-cached items. Use the appropriate API calls or python methods to see what's in the pre-cache.

- **SendMessage vs. SendMessageTimeout:** To initiate a topic with the TOS server we should send out a broadcast message via the SendMessage() system call. This call is built to block to insure the client has had a chance to deal with the ACK message. For some reason, it's deadlocking, so we've been forced to use SendMessageTimeout() with an arbitrary 500 millisecond timeout. Therefore, until this gets fixed adding topics will introduce an amount of latency in milliseconds = 500 x # of topics.




