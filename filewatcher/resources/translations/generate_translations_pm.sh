#! /bin/bash

ts_file_list=(`ls resources/translations/*.ts`)

for ts in "${ts_file_list[@]}"
do
	/usr/lib/qt6/bin/lrelease "${ts}"
done
