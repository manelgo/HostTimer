#!/bin/bash
###############################################################################
#
#  @file   HostWatchdog.sh
#  @author Manel Gonzalez Farrera
#  @date   March 2016
#  @brief  Definition of HostWatchdog
#
#  Function: Watches over HostKeeper.sh is alive, otherwise reboots.
#
###############################################################################
 

# Init variables
sleepTime=10                 ;# [seconds]
timeOut=30                   ;# corresponding to aprox x sleepTime [seconds]
touch HostKeeper.watchdog

function log {
    timeStamp=`date +"20%y/%2m/%2d|%H:%M:%S"`
    lastCharHex=`tail -c 1 HostTimer.logs | od -x | grep 0000000 | awk '{print $2}'`
    if [ "$lastCharHex" == "000a" ]; then echo $timeStamp $1
    else
        echo
	echo $timeStamp $1
    fi
}

# Initializing logs
log "####################    INITIALIZATION OF HostWatchdog.sh    ####################"


# Main execution loop
while [ 1 ]
do
    counter=`cat HostKeeper.watchdog`
    let "counter+=1"
    echo $counter > HostKeeper.watchdog
    let "filter=counter%timeOut"

    if [ $filter -eq 0 ]
    then
        loops=0
        let "loops=counter/timeOut"        
        log "HostWatchdog.sh: ERROR HostKeeper.sh is dead for $counter counts and $loops reboot loops"
        if [ $loops -eq 3 ]
        then
            if [ `find ./ -name HostKeeper.sh_old | wc -l` -eq 1 ]
            then
                log "HostWatchdog.sh: ERROR found persistently, restoring previous HostKeeper.sh_old script"
                mv -f HostKeeper.sh_old HostKeeper.sh
            else
                log "HostWatchdog.sh: ERROR found persistently, NO old HostKeeper.sh_old found for restoring"
            fi
            log "HostWatchdog.sh: ERROR found persistently, reseting watchdog counter"
            echo 0 > HostKeeper.watchdog
        fi

        log "HostWatchdog.sh: ERROR found, flushing HDD cache and executing recovery reboot"
        sudo sync ; sudo hdparm -F /dev/sda
        sudo reboot
    fi

    sleep $sleepTime 
done
