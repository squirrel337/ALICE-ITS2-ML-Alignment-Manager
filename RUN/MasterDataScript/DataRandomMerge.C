
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

void random(int seed, int maxSelection = 2){

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

   //shuffle
   int* index_DATASET = new int[nDATASETS];
   for (int i = 0; i < nDATASETS; i++) {
      index_DATASET[i] = i;
   }
   TRandom3 datalst_rndm(seed);
   Int_t j, k;
   Int_t a = nDATASETS - 1;
   for (int i = 0; i < nDATASETS; i++) {
      j = (Int_t) (datalst_rndm.Rndm() * a);
      k = index_DATASET[j];
      index_DATASET[j] = index_DATASET[i];
      index_DATASET[i] = k;
   }

   std::cout<<"LIST ORIGINAL"<<std::endl;
   for (int i = 0; i < nDATASETS; i++) {
      std::cout<<" "<<i<<" "<<dataSet[i]<<std::endl;
   };
   std::cout<<"LIST SHUFFLED"<<std::endl;
   TString init_cmd = Form("rm *");
   gSystem->Exec(Form("cd %s;%s",(const char*)masterDir,(const char*)init_cmd));
   for (int i = 0; i < maxSelection; i++) {
      //std::cout<<" "<<i<<" "<<dataSet[index_DATASET[i]]<<std::endl;
      TString cmd = Form("ln -s %s/%s .",(const char*)dataSetDir,(const char*)dataSet[index_DATASET[i]]);
      std::cout<<" "<<i<<" "<<dataSet[index_DATASET[i]]<<" "<<cmd<<std::endl;
      gSystem->Exec(Form("cd %s;%s",(const char*)masterDir,(const char*)cmd));
   };
   gSystem->Exec(Form("cd %s; ls -al",(const char*)masterDir));
};

void DataRandomMerge(int seed = 1, int nMAXfiles = 20){

   std::clog<<"DataRandomMerge STEP 0"<<std::endl;
   random(seed,kFilesPerBatch);

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
 
   std::cout<<"hadd merge START"<<std::endl;
   std::cout<<"TARGET : "<<TARGETFILE<<std::endl;
   std::cout<<"SOURCE : "<<SOURCEFILES<<std::endl;

   gSystem->Exec(Form("hadd %s %s",(const char*)TARGETFILE, (const char*)SOURCEFILES));
   //storePlots("monitor");
}
