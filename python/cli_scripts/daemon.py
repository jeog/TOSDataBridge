#!/usr/bin/env python

#
# daemon.py by Sander Marechal : 
# http://www.jejik.com/articles/2007/02/a_simple_unix_linux_daemon_in_python/
#
# made a few tweaks for python3 and tab/spacing size, J.O
#
     
import sys, os, time, atexit
from signal import SIGTERM
     
class Daemon:
    """
    A generic daemon class.
           
    Usage: subclass the Daemon class and override the run() method
    """
    def __init__( self, pidfile, stdin='/dev/null', stdout='/dev/null', 
                  stderr='/dev/null'):
        self.stdin = stdin
        self.stdout = stdout
        self.stderr = stderr
        self.pidfile = pidfile
           
    def daemonize(self):
        """
        do the UNIX double-fork magic, see Stevens' "Advanced
        Programming in the UNIX Environment" for details (ISBN 0201563177)
        http://www.erlenstar.demon.co.uk/unix/faq_2.html#SEC16
        """
        try:
            pid = os.fork()
            if pid > 0:
            # exit first parent
                sys.exit(0)
        except OSError as e:
            message = "fork #1 failed: %d (%s)\n"
            print( message % (e.errno, e.strerror), file=sys.stderr)      
            sys.exit(1)
           
        # decouple from parent environment
        os.chdir("/")
        os.setsid()
        os.umask(0)
        
        # do second fork
        try:
            pid = os.fork()
            if pid > 0:
            # exit from second parent
                sys.exit(0)
        except OSError as e:            
            print( "fork #2 failed: %d (%s)\n" % (e.errno, e.strerror), 
                   file=sys.stderr )
            sys.exit(1)
           
        #redirect standard file descriptors
        sys.stdout.flush()
        sys.stderr.flush()
        si = open(self.stdin, 'r')
        so = open(self.stdout, 'a+')
        se = open(self.stderr, 'a+', 1)
        os.dup2(si.fileno(), sys.stdin.fileno())
        os.dup2(so.fileno(), sys.stdout.fileno())
        os.dup2(se.fileno(), sys.stderr.fileno())
        
        # write pidfile
        atexit.register(self.delpid)
        pid = str(os.getpid())            
        open(self.pidfile,'w+').write("%s\n" % pid)       
           
    def delpid(self):
        os.remove(self.pidfile)
    
    def start(self):
        """
        Start the daemon
        """
        # Check for a pidfile to see if the daemon already runs
        try:
            pf = open(self.pidfile,'r')
            pid = int(pf.read().strip())
            pf.close()
        except IOError:
            pid = None
           
        if pid:
            message = "pidfile %s already exist. Daemon already running?\n"
            print(message % str(self.pidfile), file=sys.stderr)
            sys.exit(1)
        
        # Start the daemon
        self.daemonize()  
        self.run()
     
    def stop(self):
        """
        Stop the daemon
        """
        # Get the pid from the pidfile
        try:
            pf = open(self.pidfile,'r')
            pid = int(pf.read().strip())
            pf.close()
        except IOError:
            pid = None
           
        if not pid:
            message = "pidfile %s does not exist. Daemon not running?\n"
            print(message % str(self.pidfile), file=sys.stderr)
            return # not an error in a restart
     
        # Try killing the daemon process       
        try:
            while 1:
                os.kill(pid, SIGTERM)
                time.sleep(0.1)
        except OSError as e:
            e = str(e)
            if "No such process" in e:
                if os.path.exists(self.pidfile):
                    os.remove(self.pidfile)
            else:
                print(e)
                sys.exit(1)
     
    def restart(self):
        """
        Restart the daemon
        """
        self.stop()
        self.start()
                           

