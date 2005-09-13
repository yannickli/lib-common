include ../cflags

TARGET=libcommon.a

libcommon_OBJS = \
	err_report.o  \
	array.o \
	blob.o

all: $(TARGET)

clean:
	$(RM) $(TARGET) *.o *.d .*.d

clobber: clean
	$(RM) $(TARGET)

$(TARGET): $(libcommon_OBJS)
	$(AR) rcs $@ $^

# Generate hidden .d files containing user dependencies next to .o
# using gcc flags: -MMD -MF .$*.d
%.o: %.c
	$(CC) $(CFLAGS) -g -c -o $@ -MMD -MF .$*.d $<

.PHONY: all clean clobber

# Include dependency files if they exist.
-include .*.d
