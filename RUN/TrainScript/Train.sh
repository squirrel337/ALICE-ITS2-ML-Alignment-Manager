#!/bin/bash
echo "Start Script"

rm get_align_data
wget 113.198.138.222/get_align_data

cnt=0
wid=0
bid=0
nNow=0
nSteps=0
jparallel=0
nCore=0
nData=0
nEpoch=0
bxz0=0
bxz1=0
byz0=0
byz1=0
strRoot=''
strConst=''
strResult=''

input="./get_align_data"
while IFS= read -r line
do
  #echo "$line"
  IFS="," read -ra STR <<< "$line"
  for x in "${STR[@]}"
  do
    cnt=$[cnt+1]
#    echo "$cnt=$x"
    case $cnt in
      1)
        wid="$x"
        echo "wid=$wid"
        ;;
      2)
        bid="$x"
        echo "bid=$bid"
        ;;
      3)
        nNow="$x"
        echo "nNow=$nNow"
        ;;
      4)
        nSteps="$x"
        echo "nSteps=$nSteps"
        ;;
      5)
        jparallel="$x"
        echo "jparallel=$jparallel"
        ;;
      6)
        nCore="$x"
        echo "nCore=$nCore"
        ;;
      7)
        nData="$x"
        echo "nData=$nData"
        ;;
      8)
        nEpoch="$x"
        echo "nEpoch=$nEpoch"
        ;;
      9)
        bxz0="$x"
        echo "bxz0=$bxz0"
        ;;
      10)
        bxz1="$x"
        echo "bxz1=$bxz1"
        ;;
      11)
        byz0="$x"
        echo "byz0=$byz0"
        ;;
      12)
        byz1="$x"
        echo "byz1=$byz1"
        ;;
      13)
        strRoot="$x"
        echo "strData=$strRoot"
        ;;
      14)
        strConst="$x"
        echo "strConst=$strConst"
        ;;
      15)
        strResult="$x"
        echo "strResult=$strResult"
        ;;
    esac
  done
done < "$input" 

echo 
echo root Data receiving.[$strRoot]
wget 113.198.138.180/static/$strRoot
echo Data received.
echo 


echo MLPTrain Data receiving.[$strConst]
wget 113.198.138.180/static/$strConst
echo MLPTrain Data received.
echo 


echo Creating directory. 
ssh -p 22315 dclab@113.198.138.222 mkdir /var/www/align_cbnu/repo/apiv1/static/$bid/$wid
echo 

StepLIST=$(seq $nNow $nSteps)

for s in $StepLIST
do
  nStep=$s
  echo "Current Step $nStep"
  echo "gROOT->ProcessLine("'"'".L ./Ymlp/src/YDetectorGeometry.cxx"'"'");"     > batch_train
  echo "gROOT->ProcessLine("'"'".x run_train_circle.C(${nStep},${jparallel},${nCore},${nData},${nEpoch},${strRoot},${strConst})"'"'");"         >> batch_train

  ./process.sh
  mv train.log MLPTrain_Step${nStep}/train-G${g}.log

  tar -cvzf MLPTrain_Step${nStep}.tgz MLPTrain_Step${nStep}/*
  echo Upload train Result
  ssh -p 22315 ./MLPTrain_Step${nStep}.tgz dclab@113.198.138.222:/var/www/align_cbnu/repo/apiv1/static/$bid/$wid/
  echo 
done

echo 
echo Report Result
wget 113.198.138.180/report_result?wid=$wid&now=$nNow&result=$bid/$wid/MLPTrain_Step6.tgz
echo 
