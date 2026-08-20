#define nLAYER 7
#define nLAYERIB 3
#define nW 11

#define nPARALLEL 200

#include "YDetectorGeometry.cxx"

YDetectorGeometry *yGEOM;

static constexpr int NSubStave2[nLAYER] = { 1, 1, 1, 2, 2, 2, 2 };
const int NSubStave[nLAYER] = { 1, 1, 1, 2, 2, 2, 2 };
const int NStaves[nLAYER] = { 12, 16, 20, 24, 30, 42, 48 };
const int nHicPerStave[nLAYER] = { 1, 1, 1, 8, 8, 14, 14 };
const int nChipsPerHic[nLAYER] = { 9, 9, 9, 14, 14, 14, 14 };
const int ChipBoundary[nLAYER + 1] = { 0, 108, 252, 432, 3120, 6480, 14712, 24120 };
const int StaveBoundary[nLAYER + 1] = { 0, 12, 28, 48, 72, 102, 144, 192 };

int nSensorsbyLayer[nLAYER] 		= {	ChipBoundary[1] - ChipBoundary[0],
						ChipBoundary[2] - ChipBoundary[1],
						ChipBoundary[3] - ChipBoundary[2],
						ChipBoundary[4] - ChipBoundary[3],
						ChipBoundary[5] - ChipBoundary[4],
						ChipBoundary[6] - ChipBoundary[5],
						ChipBoundary[7] - ChipBoundary[6]	};

struct Network {

   Network(short patch=-1) {
      fPatch        = patch;
      hWeights      = new TH2D(Form("hWeights_P%d",patch),Form("Network Weights P%d",patch), ChipBoundary[nLAYER], 0, ChipBoundary[nLAYER], nW+6, 0, nW+6);
   };

   void initialize(){
      fPatch = -1;
      hWeights->Reset();
      hWeights->SetEntries(0);
   }

   short fPatch;
   TH2D* hWeights;
};

Network BaseNetworkWeight(-1);
std::vector<Network> NetworkWeights;

struct YmWs {

   YmWs() {
      layer      = -9999;
      halfbarrel = -9999;
      stave      = -9999;
      halfstave  = -9999;
      module     = -9999;
      lchip      = -9999;
      schip      = -9999;
      hschip     = -9999;
      mchip      = -9999;
      chipID     = -9999;
      
      Sx         = -9999;
      Sy         = -9999;
      Sr         = -9999; 
      Sphi       = -9999; 
      Sz         = -9999; 
      Sdx        = -9999; 
      Sdy        = -9999; 
      Sdr        = -9999; 
      Sdphi      = -9999; 
      Sdz        = -9999;

      Mcs1     = -9999;
      Mcs2     = -9999;
      Mcs3     = -9999;  
      Malpha   = -9999;
      Mbeta    = -9999;
      Mgamma   = -9999;
      Mt1      = -9999;
      Mt2      = -9999;
      Mt3      = -9999;  

      for(int ps = 0; ps < nPARALLEL; ps++){
         pstvID[ps]  = -1;      
         cs1[ps]     = -9999;
         cs2[ps]     = -9999;
         cs3[ps]     = -9999;        
         alpha[ps]   = -9999;
         beta[ps]    = -9999;
         gamma[ps]   = -9999;
         t1[ps]      = -9999;
         t2[ps]      = -9999;
         t3[ps]      = -9999;
         active[ps]  = false;          
      };
   };

   void clear() {
      layer      = -9999;
      halfbarrel = -9999;
      stave      = -9999;
      halfstave  = -9999;
      module     = -9999;
      lchip      = -9999;
      schip      = -9999;
      hschip     = -9999;
      mchip      = -9999;
      chipID     = -9999;
      
      Sx         = -9999;
      Sy         = -9999;
      Sr         = -9999; 
      Sphi       = -9999; 
      Sz         = -9999; 
      Sdx        = -9999; 
      Sdy        = -9999; 
      Sdr        = -9999; 
      Sdphi      = -9999; 
      Sdz        = -9999;

      Mcs1     = -9999;
      Mcs2     = -9999;
      Mcs3     = -9999;   
      Malpha   = -9999;
      Mbeta    = -9999;
      Mgamma   = -9999;
      Mt1      = -9999;
      Mt2      = -9999;
      Mt3      = -9999;  
      for(int ps = 0; ps < nPARALLEL; ps++){
         pstvID[ps]  = -1;
         cs1[ps]     = -9999;
         cs2[ps]     = -9999;
         cs3[ps]     = -9999;       
         alpha[ps]   = -9999;
         beta[ps]    = -9999;
         gamma[ps]   = -9999;
         t1[ps]      = -9999;
         t2[ps]      = -9999;
         t3[ps]      = -9999;
         active[ps]  = false; 
      };
   };

   void registerGeneralInfo(int lay, int hb, int stv, int hs, int md, int lch, int sch, int hsch, int mch, int ch){
      layer      = lay;
      halfbarrel = hb;
      stave      = stv;
      halfstave  = hs;
      module     = md;
      lchip      = lch;
      schip      = sch;
      hschip     = hsch;
      mchip      = mch;
      chipID     = ch;
   };

   void registerGloPositionInfo(double sx, double sy, double sr, double sphi, double sz, double sdx, double sdy, double sdr, double sdphi, double sdz){
      Sx         = sx;
      Sy         = sy;
      Sr         = sr; 
      Sphi       = sphi; 
      Sz         = sz; 
      Sdx        = sdx; 
      Sdy        = sdy; 
      Sdr        = sdr; 
      Sdphi      = sdphi; 
      Sdz        = sdz;
   };

   void registerParamsMaster(double npr1, double npr2, double npr3, double apr1, double apr2, double apr3, double apt1, double apt2, double apt3){
      Mcs1     = npr1;
      Mcs2     = npr2;
      Mcs3     = npr3; 
      Malpha   = apr1;
      Mbeta    = apr2;
      Mgamma   = apr3;
      Mt1      = apt1;
      Mt2      = apt2;
      Mt3      = apt3; 
   };
   
   void registerParams(int pstv, double npr1, double npr2, double npr3, double apr1, double apr2, double apr3, double apt1, double apt2, double apt3){
      pstvID[pstv]  = pstv;
      cs1[pstv]     = npr1;
      cs2[pstv]     = npr2;
      cs3[pstv]     = npr3; 
      alpha[pstv]   = apr1;
      beta[pstv]    = apr2;
      gamma[pstv]   = apr3;
      t1[pstv]      = apt1;
      t2[pstv]      = apt2;
      t3[pstv]      = apt3; 
   };
   
   //TNtuple* fmWs = new TNtuple("fmWs", "fmWs", "layer:halfbarrel:stave:halfstave:module:lchip:schip:hschip:mchip:chipID:Sx:Sy:Sr:Sphi:Sz:Sdx:Sdy:Sdr:Sdphi:Sdz:alpha:beta:gamma:t1:t2:t3");

   int layer, halfbarrel, stave, halfstave, module, lchip, schip, hschip, mchip, chipID;
   double Sx, Sy, Sr, Sphi, Sz, Sdx, Sdy, Sdr, Sdphi, Sdz;
   
   double Mcs1, Mcs2, Mcs3, Malpha, Mbeta, Mgamma, Mt1, Mt2, Mt3;
   
   int pstvID[nPARALLEL];
   double cs1[nPARALLEL], cs2[nPARALLEL], cs3[nPARALLEL], alpha[nPARALLEL], beta[nPARALLEL], gamma[nPARALLEL], t1[nPARALLEL], t2[nPARALLEL], t3[nPARALLEL];
   bool active[nPARALLEL];
};


Bool_t LoadWeights(Option_t * filename, Network NetworkWeight, bool extended = true)
{
   TString filen = filename;
   Double_t w = 0;
   if (filen == "") {
      Error("LoadWeights()","Invalid file name");
      return kFALSE;
   }
   // Four of the getline calls below pass 200 while this was allocated as 100.
   const int kBuffLen = 256;
   char *buff = new char[kBuffLen];
   std::ifstream input(filen.Data());
   
   
   if (!input.is_open()) {
      delete[] buff;
      return kFALSE;
   }
   if (input.peek() == std::ifstream::traits_type::eof()) {
      delete[] buff;
      return kFALSE; //empty
   }
   
   // input normalzation
   input.getline(buff, kBuffLen);
   Float_t n1,n2;
   for(int lay=0; lay<nLAYER; lay++){
      input >> n1 >> n2;
      input >> n1 >> n2;         
   }
   input.getline(buff, kBuffLen);
   // output normalization
   input.getline(buff, kBuffLen);
   for(int lay=0; lay<nLAYER; lay++){
      input >> n1 >> n2;
      input >> n1 >> n2;         
      input >> n1 >> n2;               
   }
   input.getline(buff, kBuffLen);
   // neuron weights
   input.getline(buff, kBuffLen);

   int nentries = extended==false ? nW : nW + 6; 
   for(int ic = 0; ic < ChipBoundary[nLAYER]; ic++){
      input >> w;
      for (int iw=0; iw<nentries; iw++) { 
         input >> w;
         NetworkWeight.hWeights->SetBinContent(ic+1, iw+1, w);
         //NetworkWeight[ic][iw] = w;
      }
   }    
   // Nothing above checks the stream. A file truncated part-way through the
   // chip loop leaves every later read failing, and each failed read leaves w
   // unchanged -- so the last value parsed was written into all remaining
   // bins and the patch was still reported as good.
   if (input.fail()) {
      Error("LoadWeights()","%s is truncated or malformed (expected %d chips x %d columns)",
            filen.Data(), ChipBoundary[nLAYER], nentries);
      delete[] buff;
      return kFALSE;
   }
   delete[] buff;
   return kTRUE;
}

Bool_t DumpWeights(Option_t * filename, Network NetworkWeight, bool extended = true)
{
   TString filen = filename;
   std::ostream * output;
   if (filen == "") {
      Error("DumpWeights()","Invalid file name");
      return kFALSE;
   }
   if (filen == "-")
      output = &std::cout;
   else
      output = new std::ofstream(filen.Data());
   Int_t nentries = 0;  
   *output << "#input normalization" << std::endl;

   Int_t j=0;
   nentries = 2*nLAYER;
   for (j=0;j<nentries;j++) {
      *output << 1 << " "
              << 0 << std::endl;
   }
   *output << "#output normalization" << std::endl;
   nentries = 3*nLAYER;
   for (j=0;j<nentries;j++) {
      *output << 1 << " "
              << 0 << std::endl;
   }
   *output << "#neurons weights #synapses weights" << std::endl;
   nentries = extended==false ? nW : nW + 6; 
   for(int ic = 0; ic < ChipBoundary[nLAYER]; ic++){
      *output << ic <<" ";
      for (j=0;j<nentries;j++) { 
         *output << setprecision(10) << NetworkWeight.hWeights->GetBinContent(ic+1,j+1) <<" ";                   
      }               
      *output<<std::endl;                                        
   } 
   // Ask the stream before it is destroyed: a short write (a full disk) would
   // otherwise produce a truncated parameter file reported as a success.
   Bool_t writeFailed = kFALSE;
   if (filen != "-") {
      ((std::ofstream *) output)->close();
      writeFailed = output->fail();
      delete output;
   } else {
      writeFailed = output->fail();
   }
   if (writeFailed) {
      Error("DumpWeights()","writing %s failed; the merged parameters are incomplete", filen.Data());
      return kFALSE;
   }
   return kTRUE;
}

TVector3 check_BaseNW_LToG(int chipID, float row, float col){

   double gx = yGEOM->LToG(chipID, row, col).X();
   double gy = yGEOM->LToG(chipID, row, col).Y();
   double gz = yGEOM->LToG(chipID, row, col).Z();   
   
   double s1 = yGEOM->GToS(chipID, gx, gy, gz)(0);
   double s2 = yGEOM->GToS(chipID, gx, gy, gz)(1);
   double s3 = yGEOM->GToS(chipID, gx, gy, gz)(2);      


   double ip1 = yGEOM->GToS(chipID,yGEOM->LToG(chipID,0,0)(0),	
 				   yGEOM->LToG(chipID,0,0)(1),
                        	   yGEOM->LToG(chipID,0,0)(2))(0);
   double fp1 = yGEOM->GToS(chipID,yGEOM->LToG(chipID,511,1023)(0),
	                           yGEOM->LToG(chipID,511,1023)(1),
		     	           yGEOM->LToG(chipID,511,1023)(2))(0); 
   double ip2 = yGEOM->GToS(chipID,yGEOM->LToG(chipID,0,0)(0),
                                   yGEOM->LToG(chipID,0,0)(1),
                                   yGEOM->LToG(chipID,0,0)(2))(1);
   double fp2 = yGEOM->GToS(chipID,yGEOM->LToG(chipID,511,1023)(0),	
                                   yGEOM->LToG(chipID,511,1023)(1),
                                   yGEOM->LToG(chipID,511,1023)(2))(1); 
                                                 
   double input_Max[] = {std::max(ip1,fp1), std::max(ip2,fp2)};
   double input_Min[] = {std::min(ip1,fp1), std::min(ip2,fp2)};
   double norm[]      =  {input_Max[0] - input_Min[0], input_Max[1] - input_Min[1]};   

   double ns1 = (float)((s1-input_Min[0])/norm[0] - 0.5);
   double ns2 = (float)((s2-input_Min[1])/norm[1] - 0.5);
   double ns3 = (float)s3;  
   
   //NetworkWeight[ic][j] 11, 0 0 b1 b2 b3 w11 w21 w12 w22 w13 w23;
   double nw_b1 = BaseNetworkWeight.hWeights->GetBinContent(chipID+1,2+1);
   double nw_b2 = BaseNetworkWeight.hWeights->GetBinContent(chipID+1,3+1);
   double nw_b3 = BaseNetworkWeight.hWeights->GetBinContent(chipID+1,4+1);

   double nw_w11 = BaseNetworkWeight.hWeights->GetBinContent(chipID+1,5+1);
   double nw_w21 = BaseNetworkWeight.hWeights->GetBinContent(chipID+1,6+1);
   double nw_w12 = BaseNetworkWeight.hWeights->GetBinContent(chipID+1,7+1);
   double nw_w22 = BaseNetworkWeight.hWeights->GetBinContent(chipID+1,8+1);
   double nw_w13 = BaseNetworkWeight.hWeights->GetBinContent(chipID+1,9+1);
   double nw_w23 = BaseNetworkWeight.hWeights->GetBinContent(chipID+1,10+1);   
   
   double cs1 = nw_w11*ns1 + nw_w21*ns2 + nw_b1;
   double cs2 = nw_w12*ns1 + nw_w22*ns2 + nw_b2;
   double cs3 = nw_w13*ns1 + nw_w23*ns2 + nw_b3;
   
   double corrected_s1 = s1 + cs1;
   double corrected_s2 = s2 + cs2;
   double corrected_s3 = s3 + cs3;
   
   return yGEOM->SToG(chipID, corrected_s1, corrected_s2, corrected_s3);
}


Bool_t MergeWeights(int seed, bool extended = true){

   TFile* Monitor_mWs    = new TFile(Form("Monitor_MergeWeights_step%d.root",seed),"recreate");

   TTree* fmWs = new TTree("fmWs","fmWs");   
   YmWs* b_mWs = new YmWs();
   fmWs->Branch("mWs", &b_mWs);
   
   //void registerGeneralInfo(int lay, int hb, int stv, int hs, int md, int lch, int sch, int hsch, int mch, int ch)
   //void registerGloPositionInfo(double sx, double sy, double sr, double sphi, double sz, double sdx, double sdy, double sdr, double sdphi, double sdz)
   //void registerParamsMaster(double npr1, double npr2, double npr3, double apr1, double apr2, double apr3, double apt1, double apt2, double apt3, int ntr)
   //void registerParams(int pstv, double npr1, double npr2, double npr3, double apr1, double apr2, double apr3, double apt1, double apt2, double apt3, int ntr, bool act)
   
   int nentries = extended==false ? nW : nW + 6; 
   for(int ic = 0; ic < ChipBoundary[nLAYER]; ic++){
      b_mWs->clear();
      bool update = true;
      
      std::cout<<"[ Merge ChipID "<<ic<<" ]"<<std::endl;
      
      b_mWs->registerGeneralInfo(yGEOM->GetLayer(ic), 
                                 yGEOM->GetHalfBarrel(ic), 
                                 yGEOM->GetStave(ic), 
                                 yGEOM->GetHalfStave(ic), 
                                 yGEOM->GetModule(ic), 
                                 yGEOM->GetChipIdInLayer(ic), 
                                 yGEOM->GetChipIdInStave(ic), 
                                 yGEOM->GetChipIdInHalfStave(ic), 
                                 yGEOM->GetChipIdInModule(ic), 
                                 ic);
      
      
      for (int j=0; j<nentries; j++) { 
         double weight = 0;
         std::cout<<" p["<<j<<", "<<update<<"] ";
         for(int p=0; p< NetworkWeights.size(); p++){
            double buffer = NetworkWeights[p].hWeights->GetBinContent(ic+1,j+1) / (double)NetworkWeights.size();
            std::cout<<NetworkWeights[p].hWeights->GetBinContent(ic+1,j+1)<<"("<<NetworkWeights[p].fPatch<<") ";
            weight += buffer;
         }
         std::cout<<"= "<<weight<<std::endl;
         BaseNetworkWeight.hWeights->SetBinContent(ic+1,j+1, weight);
      } 
      //
      float pos_Sx   = yGEOM->LToG(ic,256,512).X();
      float pos_Sy   = yGEOM->LToG(ic,256,512).Y();
      float pos_Sr   = TMath::Sqrt(pos_Sx*pos_Sx + pos_Sy*pos_Sy);
      float pos_Sphi = ( TMath::ATan2(pos_Sy,pos_Sx) >= 0 ) ? TMath::ATan2(pos_Sy,pos_Sx) : 2*TMath::ATan2(0,-1) + TMath::ATan2(pos_Sy,pos_Sx);
      float pos_Sz   = yGEOM->LToG(ic,256,512).Z();         
      
      float pos_CSx   = check_BaseNW_LToG(ic,256,512).X();
      float pos_CSy   = check_BaseNW_LToG(ic,256,512).Y();
      float pos_CSr   = TMath::Sqrt(pos_CSx*pos_CSx + pos_CSy*pos_CSy);
      float pos_CSphi = ( TMath::ATan2(pos_CSy,pos_CSx) >= 0 ) ? TMath::ATan2(pos_CSy,pos_CSx) : 2*TMath::ATan2(0,-1) + TMath::ATan2(pos_CSy,pos_CSx);
      float pos_CSz   = check_BaseNW_LToG(ic,256,512).Z();    
      
      float pos_Sdx   = pos_CSx   - pos_Sx;
      float pos_Sdy   = pos_CSy   - pos_Sy;
      float pos_Sdr   = pos_CSr   - pos_Sr;
      float pos_Sdphi = pos_CSphi - pos_Sphi;
      float pos_Sdz   = pos_CSz   - pos_Sz;  
      
      b_mWs->registerGloPositionInfo(pos_Sx, 
                                     pos_Sy, 
                                     pos_Sr, 
                                     pos_Sphi, 
                                     pos_Sz, 
                                     pos_Sdx, 
                                     pos_Sdy, 
                                     pos_Sdr, 
                                     pos_Sdphi, 
                                     pos_Sdz);

      b_mWs->registerParamsMaster(BaseNetworkWeight.hWeights->GetBinContent(ic+1,2+1),
                                  BaseNetworkWeight.hWeights->GetBinContent(ic+1,3+1),
                                  BaseNetworkWeight.hWeights->GetBinContent(ic+1,4+1),
                                  BaseNetworkWeight.hWeights->GetBinContent(ic+1,11+1),
                                  BaseNetworkWeight.hWeights->GetBinContent(ic+1,12+1), 
                                  BaseNetworkWeight.hWeights->GetBinContent(ic+1,13+1), 
                                  BaseNetworkWeight.hWeights->GetBinContent(ic+1,14+1), 
                                  BaseNetworkWeight.hWeights->GetBinContent(ic+1,15+1), 
                                  BaseNetworkWeight.hWeights->GetBinContent(ic+1,16+1));

      for(int p = 0; p < NetworkWeights.size(); p++){
         b_mWs->registerParams(p,
                               NetworkWeights[p].hWeights->GetBinContent(ic+1,2+1),
                               NetworkWeights[p].hWeights->GetBinContent(ic+1,3+1),
                               NetworkWeights[p].hWeights->GetBinContent(ic+1,4+1),
                               NetworkWeights[p].hWeights->GetBinContent(ic+1,11+1),
                               NetworkWeights[p].hWeights->GetBinContent(ic+1,12+1),
                               NetworkWeights[p].hWeights->GetBinContent(ic+1,13+1),
                               NetworkWeights[p].hWeights->GetBinContent(ic+1,14+1),
                               NetworkWeights[p].hWeights->GetBinContent(ic+1,15+1),
                               NetworkWeights[p].hWeights->GetBinContent(ic+1,16+1));         
      };

      fmWs->Fill();
   } 

   Monitor_mWs->Write();

   delete fmWs;
   delete Monitor_mWs; 


   return kTRUE;
}


void WeightsMerge(int seed = 1){

   std::clog<<"WeightsMerge STEP 1"<<std::endl;

   gSystem->Exec(Form("ls weights_step%d > weights_merge_%d.lst",seed,seed));

   TString SOURCEFILES = "";
   std::vector<TString> fSOURCEFILE;
   std::vector<int> fSOURCEINDEX;
   int nSOURCEFILES = 0;

   std::clog<<"WeightsMerge STEP 2"<<std::endl;
   TString filen = Form("weights_merge_%d.lst",seed);
   TString filename = "";
   if (filen == "") {
      Error("No MasterData List file","Invalid");
      return;
   }
   char *buff = new char[200];
   std::ifstream input(filen.Data());

   while (input) {
      input >> filename ;
      // The last pass through this loop is the one that hits end of file and
      // leaves filename empty. Everything below must stay inside the guard:
      // an empty name reduces to index 0 and would mark patch 0 present even
      // when worker 0 produced nothing.
      if(filename!=""){
         std::cout<<"#"<<nSOURCEFILES<<" "<<filename<<std::endl;
         fSOURCEFILE.push_back(filename);
         TString getIndex = filename;
         getIndex.ReplaceAll("weights_n","");
         getIndex.ReplaceAll(".txt","");
         if(getIndex.IsDigit()){
            int weights_index = getIndex.Atoi();
            if(weights_index >= 0 && weights_index < nPARALLEL){
               fSOURCEINDEX.push_back(weights_index);
            } else {
               Error("WeightsMerge()","patch index %d from '%s' is outside 0..%d; ignored",
                     weights_index, filename.Data(), nPARALLEL-1);
            }
         } else {
            Error("WeightsMerge()","'%s' is not weights_nNNN.txt; ignored", filename.Data());
         }
         nSOURCEFILES++;
      }
      filename="";
      if(nSOURCEFILES>=nPARALLEL) break;
   }
   delete[] buff;

   if(nSOURCEFILES==0){
      Error("WeightsMerge()","weights_step%d/ holds no weights_nNNN.txt; nothing to merge", seed);
      gSystem->Exit(1);
   }

   // patch information
   bool PATCHLIST[nPARALLEL];
   for(int p = 0; p < nPARALLEL; p++){
      PATCHLIST[p]=false;
   }

   for(int idx = 0; idx < fSOURCEINDEX.size(); idx++){
      PATCHLIST[fSOURCEINDEX[idx]] = true;
   }

   for(int p = 0; p < nPARALLEL; p++){
      if(PATCHLIST[p]==true) std::cout<<"weights n#"<<p<<" attached"<<std::endl;
   }

   //mode
   // 0 : Patch To Master

   int mode = 0;

   if (mode==0) {
      std::cerr<<"Params Merging"<<std::endl;
      BaseNetworkWeight.initialize();
      for(int p = 0; p < nPARALLEL; p++){
         if(PATCHLIST[p]==false) continue;
         std::cerr<<" Patch "<<p<<" Selected"<<std::endl;
         Network mNetwork(p);
         if(!LoadWeights(Form("weights_step%d/weights_n%03d.txt",seed,p),mNetwork,true)){
            // Pushing it anyway would merge an all-zero weight set and divide
            // by a count that includes it.
            Error("WeightsMerge()","patch %d could not be read; it is left out of the merge", p);
            continue;
         }
         NetworkWeights.push_back(mNetwork);
      }
      if(NetworkWeights.size()==0){
         Error("WeightsMerge()","no patch could be read; refusing to write a merged file");
         gSystem->Exit(1);
      }
      std::cerr<<"Merging "<<NetworkWeights.size()<<" patch(es)"<<std::endl;
      MergeWeights(seed);
      if(!DumpWeights(Form("weights_merge_step%d.txt",seed),BaseNetworkWeight)){
         gSystem->Exit(1);
      }
      NetworkWeights.clear();
   }
}
