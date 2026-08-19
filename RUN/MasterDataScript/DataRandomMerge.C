
// 20M / 10000event

// 200 clients * 25000 events / training by one machine = 5,000,000 events -> 20*500 = 10G


// 1st trial ~2023
//TString 119pc : dataSetDir="/run/media/root/SimulationA/2022pp/alignment-input/LHC22f_Group";

// 2nd trial 2024 using dca
// TString 119pc : dataSetDir="/run/media/root/SimulationA/2022pp/alignment-input/LHC22f_Standard/20231126_aligned_v20240129+CERN_inputs";

//TString dataSetDir="/run/media/root/Storage1/Data/2022pp/alignment-input/LHC22f_Standard/20231126_aligned_v20240129+CERN_inputs";
//TString dataSetDir="/run/media/root/Storage1/Data/2022pp/alignment-input/LHC22f_Standard/20240311_aligned_v20240129+CERN+vtxfocusAI5_inputs";
//TString dataSetDir="/run/media/root/Storage1/Data/2022pp/alignment-input/LHC22f_Standard/20231126_aligned_v20240129+CERN+ITSR+5.0um_inputs";
//TString dataSetDir="/run/media/root/Storage1/Data/2022pp/alignment-input/LHC22f_Standard/20240311_aligned_v20240129+CERN+ITSR+5.0um+coreAI6_inputs";

//TString dataSetDir="/run/media/root/Storage1/Data/2023pp/alignment-input/LHC23zt_Standard/20240405_aligned_v20240129+CERN+ITSR+5.0um_inputs";
//TString dataSetDir="/run/media/root/Storage1/Data/2023pp/alignment-input/LHC23zt_Standard/20240405_aligned_v20240129+CERN+ITSR+5.0um+DZ+AI6_inputs";
//TString dataSetDir="/run/media/root/Storage1/Data/2023pp/alignment-input/LHC23zt_Standard/20240405_aligned_v20240129+CERN+ITSR+5.0um+DZ+AI6+AI3_inputs";
//TString dataSetDir="/run/media/root/Storage1/Data/2023pp/alignment-input/LHC23zt_Standard/20240405_aligned_v20240129+CERN+ITSR+5.0um+DZ+AI6+AI3+gloXYZwMcorr.Revise1+AI3+ITS-x30um_inputs";
#include "DataSetConfig.h"   // generated from config/alignment.conf

// The commented history above records earlier campaigns; the directory in
// use is now the configured one.
TString dataSetDir = kDataSetDir;


std::vector<TString> dataSet;
TString masterDir="MasterData";

bool random(int seed, int maxSelection = 2){

   /*
   //dataSet.push_back("alignment-input-data_run00520296_1040.root");
   //dataSet.push_back("alignment-input-data_run00520296_1110.root");
   //dataSet.push_back("alignment-input-data_run00520296_1120.root");
   //dataSet.push_back("alignment-input-data_run00520296_1130.root");
   //dataSet.push_back("alignment-input-data_run00520296_1150.root");
   //dataSet.push_back("alignment-input-data_run00520296_1200.root");
   dataSet.push_back("alignment-input-data_run00520471_1450_0.root");
   dataSet.push_back("alignment-input-data_run00520471_1450_1.root");
   dataSet.push_back("alignment-input-data_run00520472_1510.root");
   dataSet.push_back("alignment-input-data_run00520472_1520.root");
   dataSet.push_back("alignment-input-data_run00520472_1530.root");
   dataSet.push_back("alignment-input-data_run00520472_1540.root");
   dataSet.push_back("alignment-input-data_run00520472_1550.root");
   //dataSet.push_back("alignment-input-data_run00520472_1600.root");
   dataSet.push_back("alignment-input-data_run00520472_1610.root");
   dataSet.push_back("alignment-input-data_run00520472_1620.root");
   dataSet.push_back("alignment-input-data_run00520472_1630.root");
   dataSet.push_back("alignment-input-data_run00520472_1640.root");
   */
   
   /*
   //dataSet.push_back("alignment-input-data_run00539884_0630_1_useDCA.root"); // verification sample
   dataSet.push_back("alignment-input-data_run00539884_0630_2_useDCA.root");
   dataSet.push_back("alignment-input-data_run00539884_0630_3_useDCA.root");
   dataSet.push_back("alignment-input-data_run00539884_0630_4_useDCA.root");
   dataSet.push_back("alignment-input-data_run00539884_0630_5_useDCA.root");

   dataSet.push_back("alignment-input-data_run00539884_0640_2_useDCA.root");
   dataSet.push_back("alignment-input-data_run00539884_0640_3_useDCA.root");
   dataSet.push_back("alignment-input-data_run00539884_0640_4_useDCA.root");
   //dataSet.push_back("alignment-input-data_run00539884_0640_5_useDCA.root"); // verification sample
   dataSet.push_back("alignment-input-data_run00539884_0650_2_useDCA.root");
   dataSet.push_back("alignment-input-data_run00539884_0650_3_useDCA.root");
   dataSet.push_back("alignment-input-data_run00539884_0650_4_useDCA.root");
   //dataSet.push_back("alignment-input-data_run00539884_0650_5_useDCA.root"); // verification sample
   
   */

   // The selection is configured in config/alignment.conf and reaches this
   // macro through the header config/alignctl.sh generates. Files that used
   // to be commented out here are simply left unticked there.
   for (int i = 0; i < kNDataFiles; i++) dataSet.push_back(kDataFiles[i]);

   int nDATASETS = dataSet.size();

   if (maxSelection <= 0 || maxSelection > nDATASETS) {
      Error("random()","asked for %d of %d configured files",maxSelection,nDATASETS);
      return false;
   }

   //shuffle
   int* index_DATASET = new int[nDATASETS];
   for (int i = 0; i < nDATASETS; i++) {
      index_DATASET[i] = i;
   }
   TRandom3 datalst_rndm(seed);
   Int_t j, k;
   for (int i = nDATASETS - 1; i > 0; i--) {
      j = (Int_t) (datalst_rndm.Rndm() * (i + 1));
      if (j > i) j = i;                      // Rndm() is [0,1); belt and braces
      k = index_DATASET[j];
      index_DATASET[j] = index_DATASET[i];
      index_DATASET[i] = k;
   }

   std::cout<<"LIST ORIGINAL"<<std::endl;
   for (int i = 0; i < nDATASETS; i++) {
      std::cout<<" "<<i<<" "<<dataSet[i]<<std::endl;
   };
   std::cout<<"LIST SHUFFLED"<<std::endl;
   // `cd <dir>;rm *` runs the rm in the macro's own directory whenever the cd
   // fails, which deletes the macros themselves. Make sure the directory is
   // there and bind the two commands so that cannot happen.
   gSystem->Exec(Form("mkdir -p %s",(const char*)masterDir));
   if (gSystem->AccessPathName(masterDir, kWritePermission)) {
      Error("random()","staging directory %s is missing or not writable",(const char*)masterDir);
      return false;
   }
   gSystem->Exec(Form("cd %s && rm -f *",(const char*)masterDir));

   int nmissing = 0;
   for (int i = 0; i < maxSelection; i++) {
      TString source = Form("%s/%s",(const char*)dataSetDir,(const char*)dataSet[index_DATASET[i]]);
      // ln -s succeeds for a target that does not exist, ls lists the dangling
      // link, and hadd then skips it with a message nobody reads -- the sample
      // silently shrinks. Check before linking instead.
      if (gSystem->AccessPathName(source, kReadPermission)) {
         Error("random()","input file missing or unreadable: %s",(const char*)source);
         nmissing++;
         continue;
      }
      TString cmd = Form("ln -s %s .",(const char*)source);
      std::cout<<" "<<i<<" "<<dataSet[index_DATASET[i]]<<" "<<cmd<<std::endl;
      gSystem->Exec(Form("cd %s && %s",(const char*)masterDir,(const char*)cmd));
   };
   gSystem->Exec(Form("cd %s && ls -al",(const char*)masterDir));

   if (nmissing > 0) {
      Error("random()","%d of %d selected input files are not available",nmissing,maxSelection);
      return false;
   }
   return true;
};

void DataRandomMerge(int seed = 1, int nMAXfiles = 20){

   std::clog<<"DataRandomMerge STEP 0"<<std::endl;
   if (!random(seed,kFilesPerBatch)) {
      Error("DataRandomMerge()","input selection failed for batch %d; nothing merged",seed);
      gSystem->Exit(1);           // so the caller's `|| die` actually fires
   }

   std::clog<<"DataRandomMerge STEP 1"<<std::endl;

   gSystem->Exec(Form("ls MasterData > MasterData_%d.lst",seed));

   TString TARGETFILE  = Form("alignment-input-data_%d.root",seed);
   TString SOURCEFILES = "";
   std::vector<TString> fSOURCEFILE;
   int nSOURCEFILES = 0;

   std::clog<<"DataRandomMerge STEP 2"<<std::endl;
   TString filen = Form("MasterData_%d.lst",seed);
   TString filename = "";
   if (filen == "") {
      Error("No MasterData List file","Invalid");
      return;
   }
   char *buff = new char[200];
   std::ifstream input(filen.Data());

   while (input) {
      input >> filename ;
      std::cout<<"#"<<nSOURCEFILES<<" "<<filename<<std::endl;
      if(filename!="") fSOURCEFILE.push_back(filename);
      filename="";
      nSOURCEFILES++;
      //if(nSOURCEFILES>=nMAXfiles) break;
   }
   nSOURCEFILES--;
   delete[] buff;

   if(nSOURCEFILES<nMAXfiles) nMAXfiles = nSOURCEFILES;

   //shuffle
   int* index = new int[nSOURCEFILES];
   for (Int_t i = 0; i < nSOURCEFILES; i++) {
      index[i] = i;
   }
   TRandom3 lst_rndm(seed);
   Int_t j, k;
   Int_t a = nSOURCEFILES - 1;
   for (Int_t i = 0; i < nSOURCEFILES; i++) {
      j = (Int_t) (lst_rndm.Rndm() * a);
      k = index[j];
      index[j] = index[i];
      index[i] = k;
   }

   for (Int_t i = 0; i < nMAXfiles; i++) {
      std::cout<<"SHUFLE "<<i<<" -> "<<index[i]<<" | TOTAL "<<nSOURCEFILES<<std::endl;
   }

   // original order
   std::cout<<" Loaded Files"<<std::endl;
   for(int f = 0; f < nMAXfiles; f++){
      std::cout<<"#"<<f<<" "<<fSOURCEFILE[f]<<std::endl;
   }

   std::cout<<" Shuffled Lists"<<std::endl;
   for(int f = 0; f < nMAXfiles; f++){
      std::cout<<"#"<<f<<" "<<fSOURCEFILE[index[f]]<<std::endl;
      SOURCEFILES += "MasterData/" + fSOURCEFILE[index[f]] + " ";
   }
 
   if (nMAXfiles <= 0 || SOURCEFILES == "") {
      Error("DataRandomMerge()","no input files staged for batch %d; nothing to merge",seed);
      gSystem->Exit(1);
   }

   std::cout<<"hadd merge START"<<std::endl;
   std::cout<<"TARGET : "<<TARGETFILE<<std::endl;
   std::cout<<"SOURCE : "<<SOURCEFILES<<std::endl;

   // -f matters: without it hadd refuses to overwrite an existing target and
   // exits, leaving a file from an earlier run in place. The status was also
   // being discarded, so that failure reached the next stage as a silently
   // stale input rather than as an error.
   int rc = gSystem->Exec(Form("hadd -f %s %s",(const char*)TARGETFILE, (const char*)SOURCEFILES));
   if (rc != 0) {
      Error("DataRandomMerge()","hadd failed (rc=%d) writing %s",rc,(const char*)TARGETFILE);
      gSystem->Exit(1);
   }
   if (gSystem->AccessPathName(TARGETFILE, kReadPermission)) {
      Error("DataRandomMerge()","hadd reported success but %s is not there",(const char*)TARGETFILE);
      gSystem->Exit(1);
   }
   std::cout<<"hadd merge DONE : "<<TARGETFILE<<std::endl;
   //storePlots("monitor");
}
