##########################################################################
#                                                                        #
#  Copyright (C) 2004-2019 INTERSEC SA                                   #
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

from logging import ERROR, WARNING, WARN, INFO, DEBUG, CRITICAL

NOTICE = 25
PANIC = 60

logging.addLevelName("PANIC", PANIC)
logging.addLevelName("NOTICE", NOTICE)

class IntersecLogger(logging.Logger):

    def __init__(self):
        # The python 2.6 implementation of the logging module use the old
        # python class model, as a consequence we must not use super() here.
        logging.Logger.__init__(self, "intersec", 0)

    def panic(self, msg, *args, **kwargs):
        self.log(PANIC, msg, *args, **kwargs)

    def notice(self, msg, *args, **kwargs):
        self.log(NOTICE, msg, *args, **kwargs)

class IntersecLogHandler(logging.Handler):
    '''subclass logging.Handler:
         redefine the emit method by calling python binding of our logger
         add two levels
       => in this way, when you use a logger with this handle:
          "LOGGER.error" calls lib-common "logger_error" method etc...
    '''

    def __init__(self, name, parent=None, silent=False, common=None):
        if not common:
            import common
        logging.Handler.__init__(self)
        self.logger = common.Logger(parent, name, silent)
        self.level_map = {
            DEBUG:    common.LOG_DEBUG,
            INFO:     common.LOG_INFO,
            NOTICE:   common.LOG_NOTICE,
            WARNING:  common.LOG_WARNING,
            WARN:     common.LOG_WARNING,
            ERROR:    common.LOG_ERR,
            CRITICAL: common.LOG_CRIT,
            PANIC:    common.LOG_PANIC,
        }

    def emit(self, record):
        level = self.level_map.get(record.levelno)
        msg = self.format(record)
        self.logger.log(level, msg)

#example
if __name__ == "__main__":
    MYHANDLER = IntersecLogHandler(name = 'intersec')
    LOGGER = IntersecLogger()
    LOGGER.addHandler(MYHANDLER)
    TMP = "arg"
    LOGGER.error("I m an error log")
    LOGGER.warning("I m not an error log, I m a warning log")
    LOGGER.warn("mee too :D")
    LOGGER.warn("And I could be use with some: %s", TMP)
    LOGGER.notice("I m noticing you that...")
    LOGGER.debug("I m debugging")
    LOGGER.info("info")
    LOGGER.panic("time to panic and generate a core")
