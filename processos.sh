#!/bin/bash
while true;do
	echo Processos....
	ps -l
	echo
	echo IPCS.....
	ipcs
	sleep 1
	clear
done
