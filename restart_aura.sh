#!/bin/bash
ps -ef | grep aura++ | grep -v grep
if [ $? != 0 ]
then
     cp ~/aura-bot/aura.log ~/aura-bot/OLDaura.log
     cd ~/aura-bot/ && nohup aura++ > aura.log 2>&1 &
fi
