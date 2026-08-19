#!/bin/bash
echo "Start Master Data Prep Script"

nsplit=10

bid=0 #Batch ID

aligndir=/run/media/root/Simulation8/PHASE_II_ALIGNMENT_TEST/work/20230111/ALIGN
moduledir=/run/media/root/Simulation8/PHASE_II_ALIGNMENT_TEST/work/20230111/MODULE

echo Shuffle 
#for bcnt in {1..1} #Batch count : bcnt[j] = bid
for(( bcnt=1; bcnt<2; bcnt++ ))
do
  echo "[JHK] batch ID : $bid"

  echo Run DataRandomMerge.
  root -l -b -q DataRandomMerge.C"($bcnt,$nsplit)" &> alignment-input-data-merge.log.$bcnt
  wait
  echo

  echo Run DataSplit.
  root -l -b -q DataSplit.C"($bcnt,$nsplit,$bid)" &> alignment-input-data-split.log.$bcnt
  wait
  echo

  echo "[JHK] Move Original Sources :: Storage -> Storage" 
  mv ./alignment-input-data-merge.log.$bcnt ./step$bcnt/
  mv ./alignment-input-data-split.log.$bcnt ./step$bcnt/
  mv ./MasterData_$bcnt.lst ./step$bcnt/
  mv ./alignment-input-data_$bcnt.root ./step$bcnt/
done
