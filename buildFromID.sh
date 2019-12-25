#!/bin/bash
source ~/ee.sh
declare -a DATAS=(1 2 3);

for data in  "${DATAS[@]}";
do
	/home/kangxiang/Distribute/Grace_slave/bin/lrelease/buildTripleBitFromN3 /sdc1/datasets/result_7500_10y-slave-$data /sdc1/datasets/LUBM_7500_10y-slave-$data
	echo buildTripleBitFromN3 slave-$data finished
done