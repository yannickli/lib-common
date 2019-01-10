##########################################################################
#                                                                        #
#  Copyright (C) INTERSEC SA                                             #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################
import logging
import common

class IntersecLogger(logging.Logger):
    def __init__(self):
        logging.Logger.__init__(self, "intersec", 0)

    def panic(self, msg):
        self.log(60, msg)

    def notice(self, msg):
        self.log(25, msg)

class IntersecLogHandler(logging.Handler):
    '''subclass logging.Handler:
         redefine the emit method by calling python binding of common e_log
         add two levels
       => in this way, when you use a logger with this handle:
          "logger.error" called lib-common "e_error" method etc...
    '''
    def __init__(self):
        logging.Handler.__init__(self)
        logging.addLevelName("PANIC", 60)
        logging.addLevelName("NOTICE", 25)

    def emit(self, record):
        msg = self.format(record)
        if record.levelno == logging._levelNames["ERROR"]:
            common.log(common.LOG_ERR, msg)
        elif record.levelno == logging._levelNames["WARNING"]\
          or record.levelno == logging._levelNames["WARN"]:
            common.log(common.LOG_WARNING, msg)
        elif record.levelno == logging._levelNames["NOTICE"]:
            common.log(common.LOG_NOTICE, msg)
        elif record.levelno == logging._levelNames["INFO"]:
            common.log(common.LOG_INFO, msg)
        elif record.levelno == logging._levelNames["DEBUG"]:
            common.log(common.LOG_DEBUG, msg)
        elif record.levelno == logging._levelNames["PANIC"]:
            common.log(common.LOG_PANIC, msg)
        elif record.levelno ==  logging._levelNames["CRITICAL"]:
            common.log(common.LOG_CRIT, msg)

myHandler = IntersecLogHandler()
logger = IntersecLogger()
logger.addHandler(myHandler)

if __name__ == "__main__":
    '''example'''
    logger.error("I m an error log")
    logger.warning("I m not an error log, I m a warning log")
    logger.warn("mee too :D")
    logger.warn("And I could be use with some: %s" % "arg")
    logger.notice("I m noticing you that...")
    logger.debug("I m debugging")
    logger.info("info")
    logger.panic("time to panic and generate a core")
