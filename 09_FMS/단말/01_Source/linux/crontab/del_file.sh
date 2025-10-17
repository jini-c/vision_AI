#!/bin/sh
SERVICE='Delete_OldFile'

find /home/fms/log -ctime +1 -exec rm {} \;
