#! /bin/bash

root=$1
proj=$2
vm1=$3
vm2=$4

for (( i = $vm1; i <= $vm2; i++ ))
do
	echo scp -r $root/$proj root@10.0.1.$i:~/
	scp -r $root/$proj root@10.0.1.$i:~/
done

for (( i = $vm1; i <= $vm2; i++ ))
do
	ssh -C root@10.0.1.$i "cd $proj && make"
done
