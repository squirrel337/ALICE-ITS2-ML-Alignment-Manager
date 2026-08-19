#!/bin/bash
echo "Start Merge Params Script"

bid=0

rm get_batch
wget 113.198.138.222/get_batch

input="./get_batch"

bid=$(<get_batch)

echo Align Weights download
scp -P 22315 dclab@113.198.138.222:/var/www/align_cbnu/repo/apiv1/static/$bid/*.txt .
echo

echo Weights Merge
root -l -b WeightsMerge.C($bid) & > alignment-params-merge.log.$bid
echo

echo Upload Merged Weight
ssh -p 22315 ./weights_merge_step$bid.txt dclab@113.198.138.222:/var/www/align_cbnu/repo/apiv1/static/$bid/
echo

echo Upload merge Log
ssh -p 22315 ./alignment-params-merge.log.$bid dclab@113.198.138.222:/var/www/align_cbnu/repo/apiv1/static/$bid/
echo 

echo Upload Analysis root
ssh -p 22315 ./Monitor_MergeWeights_step$bid.root dclab@113.198.138.222:/var/www/align_cbnu/repo/apiv1/static/$bid/
echo
