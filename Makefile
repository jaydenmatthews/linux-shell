# a general makefile

CC		 = dcc
EXERCISES	?=
CLEAN_FILES	?=

.DEFAULT_GOAL	= all
.PHONY:		all

-include *.mk

all:	${EXERCISES}

clean:
	-rm -f ${CLEAN_FILES}
