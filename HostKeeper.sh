#!/bin/bash
###############################################################################
#
#  @file   HostKeeper.sh
#  @author Manel Gonzalez Farrera
#  @date   March 2016
#  @brief  Definition of HostKeeper
#
#  Functions:
#   1) Keep SSH access closed, and open only when secure unlock key is provided.
#      Description of unlock key mechanish:
#      - FTP server permanently running in host and access is granted to a known user only secured with password.
#      - HostKeeper checks evey N seconds if unlock key is present; if so SSH access is open.
#      - After SSH access is granted HostKeepers continues to check if key is still present;
#        if not SSH access is closed and all sshd process are killed.
#      - After M seconds even if key is still present, SSH access is closed, key is deleted,
#        and all sshd processes are killed. 
#      Description of the key:
#      - Key is uploaded to host by FTP protocol.
#      - Key is a file ./FTP/SshKey.set containing hasshed password.
#
###############################################################################


#### INIT VARIABLES
sleepTime=1                      ;# [seconds]
#webServerIp="www.lenamdevs.com"
webServerIp="192.168.1.61"


#### LOCAL FUNCTIONS

function log {
    timeStamp=`date +"20%y/%2m/%2d|%H:%M:%S"`
    lastCharHex=`tail -c 1 HostTimer.logs | od -x | grep 0000000 | awk '{print $2}'`
    if [ "$lastCharHex" == "000a" ]; then echo $timeStamp $1
    else
        echo
	echo $timeStamp $1
    fi
}

function isValidTarContent {
    # Params
    tarFile=$1
    listFile=$2
    validTarContent=$3

    count=0
    #listOfFiles="HostKeeper.sh_update Signature.txt"
    for file in $validTarContent 
    do
        isFound=`tar -tf $tarFile | grep $file | wc -l`
        let "count+=isFound" 
    done
    totalFiles=`cat $listFile | wc -l`
    if [ $count -ne $totalFiles ]; then return 0; else return 1; fi
}

mutexOwner=""
function tryLockLocalMutex {
    # Lock exclusion mutex for local FTP folder if not locked by another owner or wait
    if [ `find $1 -name Mutex.locked | wc -l` -eq 1 ]
    then
        mutexOwner=`cat $1"Mutex.locked"`
        if [ $mutexOwner != $hostId ]; then return 1; fi
    else
        echo $hostId > $1"Mutex.locked"
    fi
    # Confirm that mutex is lock by host 
    mutexOwner=`cat $1"Mutex.locked"`
    if [ $mutexOwner != $hostId ]; then return 1; fi

    return 0
}

function releaseLocalMutex {
    rm -f $1"Mutex.locked"
}

# Initializing logs
log "####################    INITIALIZATION OF HostKeeper.sh    ####################"


# Stoping SSH access
log "HostKeeper.sh: stoping SSH access PENDING!!!"
####sudo /etc/init.d/ssh stop
####sudo kill -9 `ps -ef | grep sshd | grep -v "/usr/sbin/" | awk '{print $2}'`
####find ./FTP/SshKey.set | xargs rm
touch ./FTP/SshKey.set ; chown pi:pi ./FTP/SshKey.set
accessGranted=1
accessGrantedCounter=0


# Get Host ID
eth0HwAddr=`ifconfig eth0 | grep ether | awk '{print $2}' | sed 's/://g'`
#wlan0HwAddr=`ifconfig wlan0 | grep ether | awk '{print $2}' | sed 's/://g'`
hostId=$eth0HwAddr


# Start HostTimer
sudo killall HostTimer
./HostTimer 2>&1 >> HostTimer.logs &
HostTimerPID=`ps -ef | grep ./HostTimer | grep -v grep | awk '{print $2}'`
log "HostKeeper.sh: started HostTimer with PID $HostTimerPID"


# FUNCTION : sshAccessKeeper
#accessGranted=0
#accessGrantedCounter=-1
accessGrantedTimeOut=600  ;# corresponding to aprox x sleepTime [seconds]
function sshAccessKeeper {
    # Increment access counter
    if [ $accessGrantedCounter -gt -1 ]; then let "accessGrantedCounter+=1"; fi

    # Check request for SSH access
    if [ `find ./FTP/ -name SshKey.set | wc -l` -eq 1 ]
    then
        if [ $accessGranted -eq 0 ]
        then
            log "HostKeeper.sh: SSH access key set"
            # DEPRECATED: if [ `cat ./FTP/SshKey.set | ./isValidSshKey` -eq 1 ]
            if [ `cat ./FTP/SshKey.set | ./EncryptionServices -ivsk` -eq 1 ]
            then
                log "HostKeeper.sh: starting SSH access"
		resetStr="000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
                echo $resetStr > ./FTP/SshKey.set
                accessGranted=1
                sudo /etc/init.d/ssh start
                accessGrantedCounter=0
            else
                log "HostKeeper.sh: WARNING wrong SSH access key found"
                rm ./FTP/SshKey.set
            fi
        else 
            if [ $accessGrantedCounter -gt $accessGrantedTimeOut ]
            then
                log "HostKeeper.sh: stoping SSH access by time out"
                sudo /etc/init.d/ssh stop
                sudo kill -9 `ps -ef | grep sshd | grep -v "/usr/sbin/" | awk '{print $2}'`
                rm ./FTP/SshKey.set
                accessGranted=0
                accessGrantedCounter=-1           
            fi 
        fi
    else
        if [ $accessGranted -eq 1 ]
        then
            log "HostKeeper.sh: SSH key is removed hence to stop ccess"
            sudo /etc/init.d/ssh stop
            sudo kill -9 `ps -ef | grep sshd | grep -v "/usr/sbin/" | awk '{print $2}'`
            accessGranted=0
            accessGrantedCounter=-1           
        fi
    fi    
}

# FUNCTION : serverFtpPuller
#            Pull any content in the WebServer FTP to the Host Timer 
ftpPullerCounter=-1
function serverFtpPuller {
    ftpPullerTimeOut=$1

    # Increment ftp puller counter
    if [ $ftpPullerCounter -eq $ftpPullerTimeOut ]; then ftpPullerCounter=0; else let "ftpPullerCounter+=1"; fi

    # Filter execution period
    if [ $ftpPullerCounter -ne 0 ]; then return; fi

    # Lock exclusion mutex for local FTP folder if not locked by another owner or wait
    if [ `find ./FTP/ -name Mutex.locked | wc -l` -eq 1 ]
    then
        mutexOwner=`cat ./FTP/Mutex.locked`
        if [ $mutexOwner != $hostId ]; then return; fi
    else
        echo $hostId > ./FTP/Mutex.locked
    fi
    # Confirm that mutex is lock by host 
    mutexOwner=`cat ./FTP/Mutex.locked`
    if [ $mutexOwner != $hostId ]; then return; fi
    # To be replaced by:
    # if [ `tryLockLocalMutex ./FTP/` -ne 0 ]; then return; fi

    # Pull files from server
    result=`./EncryptionServices -pffs $webServerIp $hostId ./FTP/ | tail -f -n 1`
    case $result in
    0)
        if [ `find ./FTP/ -name *.tar | wc -l` -ne 0 ]
        then
             pulledFiles=`ls ./FTP/*.tar`
             log "HostKeeper.sh: Server FTP Puller got files '$pulledFiles'"
        fi
        ;;
    1)
        log "HostKeeper.sh: ERROR attempting to pull files from Server FTP"
        ;;
    2)
        log "HostKeeper.sh: CANCELLED attempting to pull files from Server FTP"
        ;;
    *)
        log "HostKeeper.sh: ERROR attempting to pull files from Server FTP returned unexpexted result $result"
    esac

    # Release exclusion mutex
    rm -f ./FTP/Mutex.locked
    # To be replaced by:
    # releaseLocalMutex ./FTP/
}

# FUNCTION : selfUpdateKeeper
#            Perform the update of the HostKeeper.sh bash script if a valid tar package HostKeeper.update.tar is found 
selfUpdateCounter=-1
selfUpdateTimeOut=13          
function selfUpdateKeeper {
    tarFile="./FTP/HostKeeper.update.tar"
    listFile="./TMP/HostKeeper.update.list"

    # Increment self update counter
    if [ $selfUpdateCounter -eq $selfUpdateTimeOut ]; then selfUpdateCounter=0; else let "selfUpdateCounter+=1"; fi

    # Filter execution period
    if [ $selfUpdateCounter -ne 0 ]; then return; fi

    # Check if self update tar file exists in FTP folder
    if [ `find . -wholename $tarFile | wc -l` -eq 1 ]
    then
        log "HostKeeper.sh: HostKeeper update tar file found, proceding with HOSTKEEPER UPDATE"

        # Cleaning TMP folder
        rm -f ./TMP/*

        # CHECKS BEFORE validating and untarting file
        # Create program update list file
        tar -tf $tarFile > $listFile
        # CB1: Check that only 2 files HostKeeper.sh_update and Signature are included in the HostKeeper update package 
        count=0
        listOfFiles="HostKeeper.sh_update Signature.txt"
        for file in $listOfFiles 
        do
            isFound=`tar -tf $tarFile | grep $file | wc -l`
            let "count+=isFound" 
        done
        totalFiles=`cat $listFile | wc -l`
        if [ $count -ne $totalFiles ]
        then
            log "HostKeeper.sh: WARNING more than expected 2 files found in tar file, ABORTING HostKeeper update"
            rm -f $tarFile $listFile
            return
        fi

        # Check HostKeeper update tar file is valid
        if [ `./EncryptionServices -ivtf $tarFile $listFile ./TMP/` -eq 1 ]
        then
            log "HostKeeper.sh: HOSTKEEPER UPDATE package is valid, continuing..."

            # Move current script to HostTimer.sh_old
            mv -f HostKeeper.sh HostKeeper.sh_old

            # Move new HostKeeper.sh_update to run folder and provide execution rights
            mv -f ./TMP/HostKeeper.sh_update ./HostKeeper.sh
            sudo chmod +x ./HostKeeper.sh

            # Removing files
            rm -f $tarFile $listFile ./TMP/Signature.txt

            # Rebooting to use new script
            log "HostKeeper.sh: HOSTKEEPER UPDATE rebooting to use new updated script"
            sudo sync ; sudo hdparm -F /dev/sda
            sudo reboot

        else
            log "HostKeeper.sh: WARNING not valid signature found in tar file, ABORTING HostKeeper update"

            # Remove files .tar and .list and signature 
            rm -f $tarFile $listFile 
        fi   
    fi
}

# FUNCTION : timerUpdateKeeper
#            Perform the update of the binary HostTimer if a valid tar package HostTimer.update.tar is found
timerUpdateCounter=-1
timerUpdateTimeOut=7          
function timerUpdateKeeper {
    tarFile="./FTP/HostTimer.update.tar"
    listFile="./TMP/HostTimer.update.list"

    # Increment timer update counter
    if [ $timerUpdateCounter -eq $timerUpdateTimeOut ]; then timerUpdateCounter=0; else let "timerUpdateCounter+=1"; fi

    # Filter execution period
    if [ $timerUpdateCounter -ne 0 ]; then return; fi

    # Check if timer update tar file exists in FTP folder
    if [ `find . -wholename $tarFile | wc -l` -eq 1 ]
    then
        log "HostKeeper.sh HostTimer update tar file found, proceding with HOSTTIMER UPDATE"
    
        # Cleaning TMP folder
        rm -f ./TMP/*

        # CHECKS BEFORE validating and untarting file
        # Create program update list file
        tar -tf $tarFile > $listFile
        # CB1: Check that only 2 files are included in the HostTimer update package 
        if [ `cat $listFile | wc -l` -ne 2 ]
        then
            log "HostKeeper.sh: WARNING more than expected 2 files found in tar file, ABORTING HostTimer update"
            rm -f $tarFile $listFile
            return
        fi
        # CB2: Check that one HostTimer_uptate file is included in the HostTimer update package 
        if [ `grep HostTimer_update $listFile | wc -l` -ne 1 ]
        then
            log "HostKeeper.sh: WARNING no HostTimer_update file found in tar file, ABORTING HostTimer update"
            rm -f $tarFile $listFile
            return
        fi
        # CB3: Check that one Signature.txt file is included in the HostTimer update package 
        if [ `grep Signature.txt $listFile | wc -l` -ne 1 ]
        then
            log "HostKeeper.sh: WARNING no Signature.txt file found in tar file, ABORTING HostTimer update"
            rm -f $tarFile $listFile
            return
        fi

        # Check HostTimer update tar file is valid
        if [ `./EncryptionServices -ivtf $tarFile $listFile ./TMP/` -eq 1 ]
        then
            log "HostKeeper.sh: HOSTTIMER UPDATE packate is valid, continuing..."

            # Kill HostTimer process
            sudo killall HostTimer

            # Move new HostTimer_update to run folder and provide execution rights
            mv ./TMP/HostTimer_update ./HostTimer
            sudo chmod +x ./HostTimer

            # Run HostTimer 
            ./HostTimer 2>&1 >> HostTimer.logs &
            HostTimerPID=`ps -ef | grep ./HostTimer | grep -v grep | awk '{print $2}'`
            log "HostKeeper.sh: HOSTTIMER UPDATE re-started HostTimer with PID $HostTimerPID"

        else
            log "HostKeeper.sh: WARNING not valid signature found in tar file, ABORTING HostTimer update"
        fi   

        # Remove files .tar and .list and signature 
        rm -f $tarFile $listFile ./TMP/Signature.txt
    fi
}

# FUNCTION : timerAliveKeeper
prevTimerWm=0
timerWmCounter=0
timerWmTimeOut=600        ;# corresponding to aprox x sleepTime [seconds]
function timerAliveKeeper {
    # Read current timer week minute
    timerWm=`cat HostTimer.status | grep -a "Week Minute:" | awk '{print $3}'`
    let "timerWm+=0"

    # Update timer watchdog counter
    if [ $timerWm -eq $prevTimerWm ]; then let "timerWmCounter+=1"
    else
        timerWmCounter=0
	prevTimerWm=$timerWm
    fi

    # Check that timer is alive or re-start
    if [ $timerWmCounter -gt $timerWmTimeOut ]
    then
        log "HostKeeper.sh: ERROR HostTimer is not alive, timerWm=$timerWm prevTimerWm=$prevTimerWm timerWmCounter=$timerWmCounter timerWmTimeOut=$timerWmTimeOut, re-starting"
        sudo killall HostTimer
        ./HostTimer 2>&1 >> HostTimer.logs &
        HostTimerPID=`ps -ef | grep " ./HostTimer" | grep root | grep -v grep | awk '{print $2}'`
        log "HostKeeper.sh: started HostTimer with PID $HostTimerPID"
        timerWmCounter=0
    fi
}

# FUNCTION : programUpdateKeeper
programUpdateCounter=-1
programUpdateTimeOut=5          
function programUpdateKeeper {
    tarFile="./FTP/Program.update.tar"
    listFile="./TMP/Program.update.list"
    updateDate="Date:\"Not reported\""

    # Increment program update counter
    if [ $programUpdateCounter -eq $programUpdateTimeOut ]; then programUpdateCounter=0; else let "programUpdateCounter+=1"; fi

    # Filter execution period
    if [ $programUpdateCounter -ne 0 ]; then return; fi

    # UPDATE STEP 1: Check that Program.update flag does not exist; if it does wait
    if [ `find . -name Program.update | wc -l` -eq 1 ]
    then
        log "HostKeeper.sh: Program.update flag is still raised" 
        return
    fi 

    # UPDATE STEP 2: Check if program update tar file exists in FTP folder
    if [ `find . -wholename $tarFile | wc -l` -eq 1 ]
    then
        log "HostKeeper.sh Program update tar file found, proceding with PROGRAM UPDATE"

        # Cleaning TMP folder
        rm -f ./TMP/*

        # CHECKS BEFORE validating and untarting file
        # Create program update list file
        tar -tf $tarFile > $listFile
        # CB1: Check that at least one _uptate file is included in the program update package 
        if [ `grep _update $listFile | wc -l` -lt 1 ]
        then
            log "HostKeeper.sh: WARNING no _update files found in tar file, ABORTING program update"
            rm -f $tarFile $listFile
            return
        fi
        # CB2: Check that only "_update" files other than the license are included in the tar file
        if [ `grep _update $listFile | wc -l` -ne `grep -v Signature.txt $listFile | wc -l` ]
        then
            log "HostKeeper.sh: WARNING not allowed files are contained in tar file, ABORTING program update"
            rm -f $tarFile $listFile
            return
        fi

        # UPDATE STEP 3: Check Program update tar file is valid
        if [ `./EncryptionServices -ivtf $tarFile $listFile ./TMP/` -eq 1 ]
        then
            log "HostKeeper.sh: PROGRAM UPDATE packate is valid, continuing..."

            # CHECKS AFTER validation
            # CA1: Check that if new Program.set_update exists, corresponding program is included in the package
            if [ `find ./TMP/ -name Program.set_update | wc -l` -eq 1]
            then
                programSet=`cat ./TMP/Program.set_update`
                if [ `find ./TMP/ -name $programSet".prog_update" | wc -l` -ne 1 ]
                then
                    log "HostKeeper.sh: WARNING no set .prog_update file found in update package, ABORTING program update"
                    rm -f $tarFile $listFile ./TMP/*
                    return
                fi
            fi

            log "HostKeeper.sh: PROGRAM UPDATE creating file Update.list required by HostTimer"
            cat $listFile | grep _update > Update.list

            log "HostKeeper.sh: PROGRAM UPDATE reading State with timestamp to record in Program.updated.txt flag"
	    if [ -f Program.info_update ]
	    then
	        updateTimeStamp=`cat Program.info_update| grep "State:"`
	    fi

            log "HostKeeper.sh: PROGRAM UPDATE moving _update files from ./TMP/ to ./ folder"
            mv -f ./TMP/*.*_update .

            # UPDATE STEP 4: Raise program update flag 
            log "HostKeeper.sh: PROGRAM UPDATE to raise Program.update flag"
            touch ./Program.update            

        else
            log "HostKeeper.sh: WARNING not valid signature found in tar file, ABORTING program update"
        fi   

        # PENDING TO TEST start
	# Rename Program.update.list as Program.updated.txt adding timestamp (Date):
	updatedFlag="./TMP/Program.updated.txt"
	echo $updateTimeStamp > $updatedFlag
        cat $listFile >> $updatedFlag
	# Sign Program.updated flag content and send to WebTimerServer
        if [ `./EncryptionServices -sfc $updatedFlag` -eq 1 ]
        then
            log "HostKeeper.sh: ERROR signing $updatedFlag"
        else
            result=`./EncryptionServices -txf $webServerIp $hostId $updatedFlag | tail -f -n 1`
            if [ $result -eq 1 ]
            then
                log "HostKeeper.sh: ERROR sending $updatedFalg to WebTimerServer with result $result"
            fi
        fi
	# PENDING TO TEST end

        # Remove files .tar and .list (renamed to $updatedFlag) and signature 
        rm -f $tarFile $listFile $updatedFlag ./TMP/Signature.txt
    fi
}

# FUNCTION : wifiUpdateKeeper
wifiUpdateCounter=-1
wifiUpdateTimeOut=7
function wifiUpdateKeeper {
    wifiUpdateFile="./FTP/WiFi.settings_update"
    interfacesFile="/etc/network/interfaces"
    supplicantFile="/etc/wpa_supplicant/wpa_supplicant.conf"

    # Increment wifiUpdate counter
    if [ $wifiUpdateCounter -eq $wifiUpdateTimeOut ]; then wifiUpdateCounter=0; else let "wifiUpdateCounter+=1"; fi

    # Filter execution period
    if [ $wifiUpdateCounter -ne 0 ]; then return; fi

    # Check if wifi update file exists in FTP folder
    if [ `find . -wholename $wifiUpdateFile | wc -l` -eq 1 ]
    then
        log "HostKeeper.sh WiFi update file found, proceding with WIFI UPDATE"

        # Check that content of update file is valid
        if [ `./EncryptionServices -ivfc $wifiUpdateFile` -eq 1 ]
        then        
            log "HostKeeper.sh: WIFI UPDATE file is valid, continuing..."

            # Read WiFi settings
            ssid=`cat $wifiUpdateFile | grep "ssid: " | awk '{print $2}'`            
            psk=`cat $wifiUpdateFile | grep "psk: " | awk '{print $2}'`            
            id_str="endUserWiFi"

            # Update interfaces file
            if [ `sudo grep $id_str $interfacesFile | grep iface | grep inet | grep dhcp | wc -l` -eq 1 ]
            then
                log "HostKeeper.sh: WIFI UPDATE interfaces already contains id_str=\"$id_str\", replacing occurrence..."
                # TO DO: To improve adding any number of blanks or tabs between iface and ssid
                sudo sed -i 's/iface[\t ]*'"$id_str"'[\t ]*inet[\t ]*dhcp.*/iface '"$id_str"' inet dhcp/' $interfacesFile
            else
                log "HostKeeper.sh: WIFI UPDATE interfaces does NOT contain id_str=\"$id_str\", appending..."
                echo -e "iface $id_str inet dhcp" | sudo tee --append $interfacesFile
            fi

            # Update wpa_supplicant.conf file
            IFS='%' # Set Internal Field Separator to keep blank spaces
            newSettings="    ssid=\"$ssid\"\n    psk=\"$psk\"\n    id_str=\"$id_str\""
            if [ `sudo grep "id_str=\"$id_str\"" $supplicantFile | wc -l` -eq 1 ]
            then
                log "HostKeeper.sh: WIFI UPDATE wpa_supplicat.conf already contains id_str=\"$id_str\", replacing occurence..."
                sudo sed -i ':a;N;$!ba;s/    ssid=\"'"$ssid"'\".*id_str=\"'"$id_str"'\"/'"$newSettings"'/g' $supplicantFile
            else
                log "HostKeeper.sh: WIFI UPDATE wpa_supplicat.conf does NOT contain id_str=\"$id_str\", appending..."
                echo -e "network={\n$newSettings\n}\n" | sudo tee --append $supplicantFile     
            fi
            unset IFS

            # Acknowledge update       
            mv -f $wifiUpdateFile $wifiUpdateFile"_resultOk"
            result=`./EncryptionServices -txf $webServerIp $hostId $wifiUpdateFile"_resultOk" | tail -f -n 1`
            if [ $result -eq 1 ]
            then
                log "HostKeeper.sh: ERROR sending anknowledge of wifi update to WebTimerServer"
            fi
            
            # Remove wifi update acknowlede file
            rm -f $wifiUpdateFile_resultOk

        else
            log "HostKeeper.sh: WARNING content of update file is not valid, ABORTING WiFi update"
            rm -f $wifiUpdateFile
        fi
    fi
}

# FUNCTION : linkUpKeeper
linkUpCounter=-1
linkUpTimeOut=30          
function linkUpKeeper {
    # Increment linkUp counter
    if [ $linkUpCounter -eq $linkUpTimeOut ]; then linkUpCounter=0; else let "linkUpCounter+=1"; fi

    # Filter execution period
    if [ $linkUpCounter -ne 0 ]; then return; fi

    linkUp=`ip addr | grep "state UP" | awk '{print $2}'`
    if [ "$linkUp" == "" ]
    then
        # Link is down, proceding to restore link
        log "HostKeeper.sh: WARNING network link is down, attempting to restore..."
        ifdown eth0
        ifup eth0
        linkUp=`ip addr | grep "state UP" | awk '{print $2}'`
        if [ "$linkUp" == "" ]
        then
            ifdown wlan0
            ifup wlan0
            linkUp=`ip addr | grep "state UP" | awk '{print $2}'`
            if [ "$linkUp" == "" ]
            then
                log "HostKeeper.sh: WARNING unable to restore network link that is still down"
            else
                log "HostKeeper.sh: wlan0 network link is up successfully restored"
            fi
        else
            log "HostKeeper.sh: eth0 network link is up successfully restored"
        fi
    fi
}

# FUNCTION : locationReporter
currentPublicIp="0.0.0.0"
locatorCounter=-1
locatorTimeOut=900 ;# corresponding to aprox x sleepTime [seconds]
function locationReporter {
    # Increment locator counter
    if [ $locatorCounter -eq $locatorTimeOut ]; then locatorCounter=0; else let "locatorCounter+=1"; fi

    # Filter execution period
    if [ $locatorCounter -ne 0 ]; then return; fi

    # Get current public IP of router to internet
    newPublicIp=`curl ipecho.net/plain; echo`
    if [ "$newPublicIp" == "" ]; then newPublicIp=`wget http://ipecho.net/plain -O - -q ; echo`; fi
    if [ "$newPublicIp" == "" ]; then newPublicIp=`curl ipv4.icanhazip.com`; fi
    if [ "$newPublicIp" == "" ]; then newPublicIp=`wget -qO- icanhazip.com`; fi
    if [ "$newPublicIp" == "" ]; then newPublicIp=`curl v4.ident.me; echo`; fi
    if [ "$newPublicIp" == "" ]; then newPublicIp=`wget http://ipinfo.io/ip -qO -`; fi

    if [ "$newPublicIp" != "$currentPublicIp" ]
    then
        # REPORT NEW LOCATION
        log "HostKeeper.sh: reporting new location public IP $newPublicIp to WebTimerServer"
        echo $newPublicIp > $hostId".location"
        ip addr | grep "state UP" --after-context=2 | awk '{print $2}' >> $hostId".location"
        #macAddress=`grep ':[a-f0-9]\{2\}:[a-f0-9]\{2\}:[a-f0-9]\{2\}:' tmp.location`
        #mv tmp.location $macAddress".location"

        # Sign file content and send to WebTimerServer
        if [ `./EncryptionServices -sfc $hostId".location"` -eq 1 ]
        then
            log "HostKeeper.sh: ERROR signing location file"
        else
            result=`./EncryptionServices -txf $webServerIp $hostId $hostId".location" | tail -f -n 1`
            if [ $result -eq 1 ]
            then
                log "HostKeeper.sh: ERROR sending location file to WebTimerServer"
            fi
        fi
        currentPublicIp=$newPublicIp            
    fi        
}

# FUNCTION : statusReporter
prevMD5=-1
statusCounter=-1
statusTimeout=10
function statusReporter {
    # Increment status counter
    if [ $statusCounter -eq $statusTimeout ]; then statusCounter=0; else let "statusCounter+=1"; fi

    # Filter execution period
    if [ $statusCounter -eq 0 ]; then return; fi

    # Filter if status has not changed
    MD5=`md5sum HostTimer.status | awk '{print $1}'`
    if [ "$MD5" == "$prevMD5" ]; then return; fi

    # Report new status
    prevMD5=$MD5
    cp HostTimer.status ./TMP/$hostId".status"
    if [ `./EncryptionServices -sfc ./TMP/$hostId".status"` -eq 1 ]
    then
        log "HostKeeper.sh: ERROR signing content of status file"
    else
        result=`./EncryptionServices -txf $webServerIp $hostId ./TMP/$hostId".status" | tail -f -n 1`
        if [ $result -eq 1 ]
        then
            log "HostKeeper.sh: ERROR sending status file to WebTimerServer"
        fi
    fi
}

# FUNCTION : Main execution loop
while [ 1 ]
do
    # Check request for SSH access
    # sshAccessKeeper

    # Check HostTimer is alive
    timerAliveKeeper

    # Check internet link is up
    linkUpKeeper

    # Check for WiFi settings update
    wifiUpdateKeeper

    # Pull any files in WebServer FTP
    serverFtpPuller 60

    # Check for self update
    selfUpdateKeeper

    # Check for HostTimer update
    timerUpdateKeeper

    # Check HostTimer program update
    programUpdateKeeper

    # Report location
    locationReporter

    # Report status
    statusReporter

    # Reset host watchdog
    echo 0 > HostKeeper.watchdog

    sleep $sleepTime 
done
