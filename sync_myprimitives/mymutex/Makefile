TARGET_2 = mutex_test
SRCS_2 = list.c mutex.c main.c

CC=gcc
RM=rm
CFLAGS= -g -Wall
INCLUDE_DIR="."

all: ${TARGET_2}

${TARGET_2}: list.h ${SRCS_2}
	${CC} ${CFLAGS} -I${INCLUDE_DIR} ${SRCS_2} -o ${TARGET_2}

clean:
	${RM} -f *.o ${TARGET_2}