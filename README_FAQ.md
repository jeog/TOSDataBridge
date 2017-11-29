### Frequently Asked Questions
- - -

##### What does TOSDataBridge do?

It provides (programmatic) access to real-time streaming data from the ThinkOrSwim(TOS) platform.

##### What type of data does it provide?

It attempts to provide all the data that TOS exports via DDE: about 60 fields in total. Some of the more popular are LAST, VOLUME, NET_CHANGE, BID, ASK. It also provides [access to the 19 CUSTOM fields exported by TOS](README_DETAILS.md#custom-topics), allowing users to export custom calculations, studies, strategy triggers etc. from the platform.

##### What type of securities/instruments does it work with ?

Generally any symbol that the TOS platform recognizes: equities('AAPL'), ETFs('SPY'), futures('/ZC'), options('.SPY180119C250'), indices('$DJT'), forex('EUR/USD'), mutual-funds(VFIAX), and certain indicators($TRIN). If you find an exception please let us know. Different securities are more suited to certain data fields than others. For instance, GAMMA is relevant to options but not much else. 

##### Do I need Windows?

Yes, only the Windows version of TOS exports DDE data. The python API does provide a virtual layer that allows access from any system as long as it is physically or virtually networked with a Windows system running TOS and the TOSDataBridge Service. 

##### Do I need to have TOS running at all times?

Yes.

##### Do I need to know how to program?

Yes. If you're new to programming, and willing to learn, the Python API is the place to start. Python is an interactive language that is very user friendly and easy to learn. There are plenty of free tutorials online and we provide a tutorial for using the Python API in python/tosdb/tutorial.md.

##### What programming languages are supported?

C, C++, Java, and Python interfaces are provided. The Java interface is still in development so it is the least stable.

##### I'm having trouble connecting to the Service/Engine and/or the TOS platform. What do I do?

[Review the troubleshooting docs](README_TROUBLESHOOTING.md). 

##### How do I build/debug TOSDataBridge?

[Review the Build Notes](README.md#build-optional). Release builds include a (.pdb) symbols file for debugging. 

##### How is this different than Ameritrade's API?

It's almost completely different. All TOSDataBridge provides is the ability to (locally) pull data off the TOS platform. It's not affiliated with Ameritrade. It doesn't (directly) communicate with their servers. You can't execute orders etc. It's purely a data extraction tool.

##### Why don't the data and associated time-stamps exactly match the Time & Sales data of the platform/exchange?

TOSDataBridge relies on [TOS's use of the Window's DDE mechanism.](README_DETAILS.md#dde-data). 

