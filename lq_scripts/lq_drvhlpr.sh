#!/bin/sh

echo start drvhlpr with param


(drvhlpr -p /tmp/drvhlpr_w0.conf </dev/console 1>/dev/console 2>&1);

if [ $? = 1 ]
then
  echo drvhlpr return MAC HANG
  #/tmp/mtlk_init.sh 
  #drvhlpr will reboot the system while we have bug in SW Watchdog mechanism
  #(. ./reload_mtlk_driver.sh > /dev/null);
elif [ $? = 2 ]
  then
  echo drvhlpr return rmmod
else
  echo return from drvhlpr with error
fi
