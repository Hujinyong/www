#!/bin/bash

CURDIR=$(pwd)
ROOTDIR=$CURDIR/..

PART1='
ifeq ($(RTE_SDK),)
	$(error "Must define RTE_SDK")
endif

ifeq ($(RTE_TARGET),)
	$(error "Must define RTE_SDK")
endif

include $(RTE_SDK)/mk/rte.vars.mk
'
PART2="
CFLAGS += -g
CFLAGS += -I$ROOTDIR/dist/include 
CFLAGS += -I$ROOTDIR/dist/include/net
CFLAGS += -I$ROOTDIR/dist/include/dns 
CFLAGS += -L$ROOTDIR/dist/lib
ifeq (\$(TRACE_MBUF),1)
CFLAGS += -DTRACE_MBUF
endif
LDFLAGS += -lnetproto
ifeq (\$(CONFIG_DNS),y)
LDFLAGS += -ldns
endif

DEPDIRS-y += net
DEPDIRS-\$(CONFIG_DNS) += dns

"
PART3=

PART4='include $(RTE_SDK)/mk/rte.extapp.mk'

function genmk {
    if [ ! -d $1 ];then
        echo "not directory $1"
        return
    fi

    if [ -f $1/Makefile ]
    then
        return
      #  cp $1/Makefile $1/Makefile_bak
    fi

    cd $1
    echo "$PART1" > Makefile
    APART2="${PART2}APP = $1
    "
    echo "$APART2" >> Makefile

    for i in *.c
    do
        APART3="${PART3}SRCS-y += $(basename $i)
        "
    done

    echo "$APART3" >> Makefile
    echo "$PART4" >> Makefile
    cd $CURDIR
}

recurse()
{
    for i in $1/*
    do
        if [ -d $i ]
        then
            genmk $(basename $i)
        fi
    done
}

recurse $CURDIR
