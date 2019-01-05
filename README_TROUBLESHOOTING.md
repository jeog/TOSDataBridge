### Troubleshooting
- - -

#### Connection Problems

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

If you are still having issues please [file a new issue](https://github.com/jeog/TOSDataBridge/issues/new); you may have run into a serious bug that we'd like to fix.

#### Bad Futures Data

Ameritrade has changed the symbols for futures contracts. [The new symbols.](README.md#new-symbols-for-futures-contracts)

#### Error(s) adding Item and/or Topics

*TODO*


#### Custom Topic Problems

*TODO*



