#!/bin/bash
echo "Start Master Data Prep Script"

TAG=$1

if [ "$TAG" == "" ]; then
  echo "PLEASE SET SUBMISSION NAME"
  exit
fi

homedir=$PWD

o2dir=/home/alice/Software/v20230501
#o2dir=/run/media/root/Storage1/Software/v20230501

echo "homedir : ${homedir}"
echo "o2dir   : ${o2dir}"

eval `alienv load -w ${o2dir}/sw O2/latest`

nsplit=8

bid=0 #Batch ID

BASESTEP=900
NSTEPBYBATCH=1


aligndir="${homedir}/ALIGN"
alignworker="${aligndir}/${TAG}"

paramsdir="${homedir}/PARAMS"

moduledir="${homedir}/MODULE"

resultdir="${homedir}/RESULT"

modulename="v20250627-a_vtx.useAPR.hitCluster.useDCA.ver4_core.v20251024"
modulename="v20250627+a_vtx.useAPR.hitCluster.useDCA.ver4_core.v20251024"

resultcontainer="${resultdir}/${TAG}"

echo "Train Local" 

for(( ns=0; ns<${nsplit}; ns++ ))
do
  cd ${homedir}
  mkdir -p ${alignworker}/align_worker_$ns
  cp ${moduledir}/${modulename}.tgz ${alignworker}/align_worker_$ns/${modulename}.tgz
  cd ${alignworker}/align_worker_$ns
  tar -zxvf ${modulename}.tgz
  cd ${homedir}  
done

for(( bcnt=1; bcnt<11; bcnt++ ))
do
  echo "[STEP 0] batch ID : $bcnt"
  cd ${homedir}
  mkdir -p ${resultcontainer}/result_step_${bcnt}
  
  cd ${homedir}  
  
  echo "[STEP 1] Data Preparation"

  cd ${homedir}  
  cd ./RUN/MasterDataScript

  echo Run DataRandomMerge.
  root -l -b -q DataRandomMerge.C"($bcnt,20)" &> alignment-input-data-merge.log.$bcnt
  wait
  echo

  echo Run DataSplit.
  root -l -b -q DataSplit.C"($bcnt,$nsplit,$bid)" &> alignment-input-data-split.log.$bcnt
  wait
  echo

  mv ./alignment-input-data-merge.log.$bcnt ./step$bcnt/
  mv ./alignment-input-data-split.log.$bcnt ./step$bcnt/
  mv ./MasterData_$bcnt.lst ./step$bcnt/
  mv ./alignment-input-data_$bcnt.root ./step$bcnt/
  
  mv ./step$bcnt ${alignworker}/step$bcnt
  
  cd ${homedir}
  
  echo "[STEP 2] Execute Alignment Modules"
  cd ${homedir}

  input_step=`expr ${BASESTEP} \+ \( ${bcnt} - 1 \) \* ${NSTEPBYBATCH}`
  start_step=`expr ${BASESTEP} \+ \( ${bcnt} - 1 \) \* ${NSTEPBYBATCH} \+ 1`
  end_step=`expr ${BASESTEP} \+ \( ${bcnt} \) \* ${NSTEPBYBATCH}`
  for(( ns=0; ns<${nsplit}; ns++ ))
  do
    echo "Link Data Input File : worker $ns :: ${alignworker}/step$bcnt/alignment-input-data-split${ns}.root -> XXXXinput.root"
    cd ${alignworker}/align_worker_$ns/${modulename}
    rm -f XXXXinput.root
    ln -s ${alignworker}/step$bcnt/alignment-input-data-split${ns}.root XXXXinput.root


    if [ $bcnt -eq 1 ] ; then
      echo "COPY FROM REFERNCE" 
      cp ${paramsdir}/MLPTrain_Step${input_step}.tgz MLPTrain_Step${input_step}.tgz
    else
      echo "COPY FROM PREVIOUS SOURCE" 
      cp ${paramsdir}/${TAG}/MLPTrain_Step${input_step}.tgz MLPTrain_Step${input_step}.tgz
    fi

    echo "#### current directory : ${alignworker}/align_worker_$ns/${modulename}"     
    ls -altr
    echo "#### "     
    tar -zxvf MLPTrain_Step${input_step}.tgz
    
    echo "./process_all_master.sh ${TAG} ${start_step} ${end_step}"
    ./process_all_master.sh ${TAG} ${start_step} ${end_step} &
    sleep 15s
    cd ${homedir}
  done
  echo "[STEP 2-1] Wait Alignment Modules Execution"
  wait
 
  echo "[STEP 2-2] Save Alignment Modules Result"
  for(( ns=0; ns<${nsplit}; ns++ ))
  do 
    cd ${alignworker}/align_worker_$ns/${modulename} 
    for((nStep=${start_step}; nStep<=${end_step}; nStep++ ))
    do
      mv MLPTrain_Step${nStep}.tgz MLPTrain_Step${nStep}-aw${ns}.tgz
      mv MLPTrain_Step*.tgz ${resultcontainer}/result_step_${bcnt}/
      
      mkdir ../buffer_step$bcnt
      mv MLPTrain_Step* ../buffer_step$bcnt/
    done
    cd ${homedir}     
  done
  
  echo "[STEP 3] Merge Alignment Parameters"

  cd ./RUN/MergeParamsScript
  mkdir -p weights_step${bcnt}
  cd ${homedir}
  
  for(( ns=0; ns<${nsplit}; ns++ ))
  do 
    cd ${resultcontainer}/result_step_${bcnt}    
    tar -zxvf MLPTrain_Step${end_step}-aw${ns}.tgz
    mv MLPTrain_Step${end_step} MLPTrain_Step${end_step}-aw${ns}
    cd MLPTrain_Step${end_step}-aw${ns}/weights
    weightfile=`ls -altr |grep "weights_Epoch" | awk '{print $9}' |tail -n1`
    splitindex=`printf "%.3d" ${ns}`
    
    cp ${weightfile} ${homedir}/RUN/MergeParamsScript/weights_step${bcnt}/weights_n${splitindex}.txt
    cd ${homedir}    
  done  
  
  cd ${homedir}
      
  cd ./RUN/MergeParamsScript
  root -l -b -q WeightsMerge.C"($bcnt)" &> alignment-params-merge.log.$bcnt
  
  mv alignment-params-merge.log.$bcnt ${alignworker}/step$bcnt/alignment-params-merge.log.$bcnt
  mv Monitor_MergeWeights_step$bcnt.root ${alignworker}/step$bcnt/Monitor_MergeWeights_step$bcnt.root
  mv weights_merge_$bcnt.lst ${alignworker}/step$bcnt/weights_merge_$bcnt.lst
  mv weights_merge_step$bcnt.txt ${alignworker}/step$bcnt/weights_merge_step$bcnt.txt
  mv weights_step$bcnt ${alignworker}/step$bcnt/weights_step$bcnt
   
  cd ${homedir} 
  
  echo "[STEP 3-1] Prepare Updated Alignment Parameters" 
  mkdir -p PARAMS/${TAG}
  cd PARAMS
  cp MLPTrain_Step${BASESTEP}.tgz ${TAG}/MLPTrain_Step${BASESTEP}.tgz
  cd ${TAG}
  tar -zxvf MLPTrain_Step${BASESTEP}.tgz
  mv MLPTrain_Step${BASESTEP} MLPTrain_Step${end_step}
  rm MLPTrain_Step${end_step}/weights/weights.txt
  cp ${alignworker}/step$bcnt/weights_merge_step$bcnt.txt MLPTrain_Step${end_step}/weights/weights.txt
  tar -zcvf MLPTrain_Step${end_step}.tgz MLPTrain_Step${end_step}
   
  cd ${homedir} 
  
done
