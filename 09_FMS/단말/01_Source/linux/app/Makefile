# ------------------------------------------------------------------------------
# Logger 
# ------------------------------------------------------------------------------
# Version
MAJOR_VERSION:=0
MINOR_VERSION:=1
VERSION:=$(MAJOR_VERSION).$(MINOR_VERSION)

#WEB SOCKET을 사용하지 않을 경우 0으로 세팅할것
#구 전파 감시국 RTU용 

#------------------------------------------------------------------------------
D_USE_WEBSOCK=1 
#------------------------------------------------------------------------------


# Date
CURRUNT_YEAR := $(shell date +"%Y")
CURRUNT_DATE := $(shell date +"%Y_%m_%d")
CURRUNT_TIME := $(shell date +"%H_%M")

# Project
PROJECT_NAME:=fms_logger

# Directory
TOPDIR := $(shell /bin/pwd)
DIR_APP_BIN:=$(TOPDIR)/bin
DIR_INCLUDE:=$(TOPDIR)/include

LIB_DIR:=lib
DIR_CLI:=cli
DIR_OBSERVER:=observer
DIR_LOGGER:=logger

# Tool Chain
CC:=gcc
STRIP:=strip

# Shell Command
MKDIR:=mkdir
CP:=cp
DD:=dd
RM:=rm
MV:=mv
CHOWN:=chown
MOUNT:=mount
UMOUNT:=umount
TOUCH:=touch
MKNOD:=mknod
CHMOD:=chmod

# cflags
CFLAGS+=-I$(DIR_INCLUDE)
CFLAGS+= -I/usr/include/mysql
CFLAGS+= -L/user/local/lib/mysql
CFLAGS+= -DAPP_MAJOR_VERSION='"$(MAJOR_VERSION)"'
CFLAGS+= -DAPP_MINOR_VERSION='"$(MINOR_VERSION)"'
CFLAGS+= -Wall
CFLAGS+= -Wformat
CFLAGS+= -pthread
CFLAGS+= -lrt
CFLAGS+= -lcrypt
CFLAGS+= -lc

# Option Use WebSocket 

ifeq ($(D_USE_WEBSOCK), 1 )
CFLAGS+= -D USE_WEBSOCK
endif

# Library Path
#LD_LIBRARY_PATH=$(CROSS_COMPILE_LIB_PATH)
#export LD_LIBRARY_PATH

# Local Path export
export PATH:=$(PATH):.

# Application
APP_INIT=initialize
APP_CLI=rtu_cli
APP_OBSERVER=fms_observer
APP_LOGGER=fms_logger

# ------------------------------------------------------------------------------
# obj
# ------------------------------------------------------------------------------
# Library 
#LIB_LINK = $(LIB_DIR)/util.o $(LIB_DIR)/core_conf.o $(LIB_DIR)/conf.o $(LIB_DIR)/sem.o $(LIB_DIR)/shm.o $(LIB_DIR)/sps_queue.o $(LIB_DIR)/evt_queue.o \
#		   $(LIB_DIR)/nmea.o $(LIB_DIR)/com_ioctl.o $(LIB_DIR)/gpio.o $(LIB_DIR)/dsp_save.o $(LIB_DIR)/util_cli.o $(LIB_DIR)/update.o $(LIB_DIR)/ubt_update.o 

#LIB_LINK = $(LIB_DIR)/util.o $(LIB_DIR)/core_conf.o $(LIB_DIR)/conf.o $(LIB_DIR)/sem.o $(LIB_DIR)/shm.o $(LIB_DIR)/com_sns.o \
#           $(LIB_DIR)/net.o

LIB_LINK = $(LIB_DIR)/util.o $(LIB_DIR)/core_conf.o $(LIB_DIR)/conf.o $(LIB_DIR)/sem.o $(LIB_DIR)/shm.o $(LIB_DIR)/com_sns.o

#========================================================================
# OBSERVER DIR
OBSERVER_SRC_ALL=
OBSERVER_SRC= main.c
OBSERVER_SRC_ALL+= $(addprefix $(DIR_OBSERVER)/, $(OBSERVER_SRC))
OBSERVER_OBJ = $(addprefix $(TOPDIR)/,$(OBSERVER_SRC_ALL:.c=.o)) $(LIB_LINK)
#========================================================================


#========================================================================
# CLI DIR
CLI_SRC_ALL=
CLI_SRC= main.c
CLI_SRC_ALL+= $(addprefix $(DIR_CLI)/, $(CLI_SRC))
CLI_OBJ = $(addprefix $(TOPDIR)/,$(CLI_SRC_ALL:.c=.o)) $(LIB_LINK)
#========================================================================

# Logger DIR
LOGGER_SRC_ALL=
LOGGER_SRC=	main.c \
			fms_client.c \
			op_server.c \
			update_client.c \
			control.c \
			radio.c	\
			gpio_mcu.c \
			pwr_c.c \
			remote.c \
			temp.c \
			pwr_mng.c \
			ctrl_db.c \
			pwr_m.c \
			apc_ups.c \
			websock.c \
			net.c
LOGGER_SRC_ALL+= $(addprefix $(DIR_LOGGER)/, $(LOGGER_SRC))
LOGGER_OBJ = $(addprefix $(TOPDIR)/,$(LOGGER_SRC_ALL:.c=.o)) $(LIB_LINK)

# ------------------------------------------------------------------------------
# Target
# ------------------------------------------------------------------------------
all: fms_observer rtu_cli fms_logger  

# build sources

fms_observer: $(OBSERVER_OBJ) 
	$(CC) $(CFLAGS) $^ -lm -o $(DIR_APP_BIN)/$(APP_OBSERVER)
	$(STRIP) $(DIR_APP_BIN)/$(APP_OBSERVER)
	sudo $(CHMOD) 755 $(DIR_APP_BIN)/$(APP_OBSERVER)	
	@echo "============================================================================"
	@echo " Build Result :" $(DIR_APP_BIN)/$(APP_OBSERVER)
	@echo "============================================================================"
	@echo " "

rtu_cli: $(CLI_OBJ) 
	$(CC) $(CFLAGS) $^ -lm -o $(DIR_APP_BIN)/$(APP_CLI) -lmysqlclient 
	$(STRIP) $(DIR_APP_BIN)/$(APP_CLI)
	sudo $(CHMOD) 755 $(DIR_APP_BIN)/$(APP_CLI)	
	@echo "============================================================================"
	@echo " Build Result :" $(DIR_APP_BIN)/$(APP_CLI)
	@echo "============================================================================"
	@echo " "

fms_logger: $(LOGGER_OBJ) 
ifeq ($(D_USE_WEBSOCK), 1 )
	$(CC) $(CFLAGS) $^ -lm -o $(DIR_APP_BIN)/$(APP_LOGGER) -lwebsockets -lmysqlclient  
else
	$(CC) $(CFLAGS) $^ -lm -o $(DIR_APP_BIN)/$(APP_LOGGER) -lmysqlclient  
endif
	$(STRIP) $(DIR_APP_BIN)/$(APP_LOGGER)
	sudo $(CHMOD) 755 $(DIR_APP_BIN)/$(APP_LOGGER)	
	
	@echo "============================================================================"
	@echo " Build Result :" $(DIR_APP_BIN)/$(APP_LOGGER)
	@echo "============================================================================"
	@echo " "

# make app file system

clean:
	sudo $(RM) -rf $(INIT_OBJ)
	sudo $(RM) -rf $(OBSERVER_OBJ)
	sudo $(RM) -rf $(CLI_OBJ)
	sudo $(RM) -rf $(LOGGER_OBJ)
	sudo $(RM) -rf $(DIR_APP_BIN)/*
	@echo " "	
	@echo "============================================================================"
	@echo " All Clean Complete..."
	@echo "============================================================================"
	@echo " "	
