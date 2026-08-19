#include "DataInputStructure.h"

static constexpr int NLayer = 7;
static constexpr int NLayerIB = 3;

const int ChipBoundary[NLayer + 1] = { 0, 108, 252, 432, 3120, 6480, 14712, 24120 };

TFile** fileOut_DataInput;
TH1D** hNClusterByTrack;
TH1D** hNTrackByEvent;
TTree** fDataInput;
EventData** b_eventdata;  
TrackData** b_trackdata;


int* nSPLITINDEX;

void splitData(TTree* tree, EventData* s_eventdata, o2::its::GeometryTGeo* gm, int seed = 0, int nsplit = 1, int bid = 0){ //seed 0 : time based , not reconstructable, -> start from 1

   TrackData *s_trackdata;

   TRandom3 rseed(seed);

   fileOut_DataInput = new TFile *[nsplit];
   hNClusterByTrack  = new TH1D  *[nsplit];
   hNTrackByEvent    = new TH1D  *[nsplit];
   fDataInput        = new TTree *[nsplit];
   b_eventdata       = new EventData *[nsplit];
   b_trackdata       = new TrackData *[nsplit];

   nSPLITINDEX       = new int [nsplit];

   gSystem->Exec(Form("mkdir step%d",seed));
   for(int s=0; s<nsplit; s++){
      fileOut_DataInput[s] = new TFile(Form("step%d/alignment-input-data-split%d.root",seed,s),"recreate");
      hNClusterByTrack[s]  = new TH1D("hNClusterByTrack","NClueters / Track;N_{Cluster};N_{Track}",10,0,10);
      hNTrackByEvent[s]    = new TH1D("hNTrackByEvent","NTrack / Event(ROF);N_{Track};N_{Track}",50,0,50);
      fDataInput[s]        = new TTree("DataInput","DataInput");
      b_eventdata[s]       = new EventData();  
      b_trackdata[s]       = 0;
      fDataInput[s]->Branch("event", &b_eventdata[s]);
      nSPLITINDEX[s] = 0;
   }

   int NeventsAll = 0;
   std::clog<<"Total # of Events that need to be selected : "<<tree->GetEntries()<<std::endl;
   for(int ievt = 0; ievt < tree->GetEntries(); ievt++){
      tree->GetEntry(ievt);
   
      //selection
      int splitIndex = (int)rseed.Uniform(0, nsplit);
      if(splitIndex<0 || splitIndex>=nsplit) continue;
     
      b_eventdata[splitIndex]->GetTrack()->Clear();   
      b_eventdata[splitIndex]->SetNtracks(0);  

      for(int itrk = 0; itrk < s_eventdata->GetNtracks(); itrk++) {
         s_trackdata = (TrackData *) s_eventdata->GetTrack()->At(itrk);
         for(int ilay = 0; ilay < nLAYER; ilay++){      
            if(s_trackdata->ChipID[ilay]<0) continue;
            int layer   = (int) gm->getLayer(s_trackdata->ChipID[ilay]);
            int hb      = (int) gm->getHalfBarrel(s_trackdata->ChipID[ilay]);
            int stv     = (int) gm->getStave(s_trackdata->ChipID[ilay]);
            int hs      = (int) gm->getHalfStave(s_trackdata->ChipID[ilay]);
            int md      = (int) gm->getModule(s_trackdata->ChipID[ilay]);
            int mChipID = (int) gm->getChipIdInModule(s_trackdata->ChipID[ilay]);
            
            int ChipID  = s_trackdata->ChipID[ilay];        
         }          
      }

      std::cout<<"Data Selection Event #"<<ievt<<" / "<<tree->GetEntries()<<" SPLIT "<<splitIndex<<std::endl;
      nSPLITINDEX[splitIndex]++;

      b_eventdata[splitIndex]->SetX1(s_eventdata->GetX1()); 
      b_eventdata[splitIndex]->SetX2(s_eventdata->GetX2());
      b_eventdata[splitIndex]->SetX3(s_eventdata->GetX3());      
      b_eventdata[splitIndex]->SetP1(0); 
      b_eventdata[splitIndex]->SetP2(0);
      b_eventdata[splitIndex]->SetP3(0);
      b_eventdata[splitIndex]->GetTrack()->Clear();   
      b_eventdata[splitIndex]->SetNtracks(0); 
         
      for(int itrk = 0; itrk < s_eventdata->GetNtracks(); itrk++) {
         s_trackdata = (TrackData *) s_eventdata->GetTrack()->At(itrk);
         
         b_eventdata[splitIndex]->AddOneTrack();  
         b_trackdata[splitIndex] = (TrackData *) b_eventdata[splitIndex]->GetTrack()->At(b_eventdata[splitIndex]->GetNtracks()-1);      
         b_trackdata[splitIndex]->p	       = s_trackdata->p;
         b_trackdata[splitIndex]->pt       = s_trackdata->pt;
         b_trackdata[splitIndex]->theta    = s_trackdata->theta;
         b_trackdata[splitIndex]->phi      = s_trackdata->phi;
         b_trackdata[splitIndex]->eta      = s_trackdata->eta;

         b_trackdata[splitIndex]->tv1      = s_trackdata->tv1;
         b_trackdata[splitIndex]->tv2      = s_trackdata->tv2;
         b_trackdata[splitIndex]->tv3      = s_trackdata->tv3;
         b_trackdata[splitIndex]->tv1_X0   = s_trackdata->tv1_X0;   
         b_trackdata[splitIndex]->tv2_X0   = s_trackdata->tv2_X0;   
         b_trackdata[splitIndex]->tv3_X0   = s_trackdata->tv3_X0;   
         b_trackdata[splitIndex]->tv1_DCA  = s_trackdata->tv1_DCA;   
         b_trackdata[splitIndex]->tv2_DCA  = s_trackdata->tv2_DCA;    	
         b_trackdata[splitIndex]->tv3_DCA  = s_trackdata->tv3_DCA;     
  
         for(int ilay = 0; ilay < nLAYER; ilay++){ 
            b_trackdata[splitIndex]->s1[ilay]		= s_trackdata->s1[ilay];
            b_trackdata[splitIndex]->s2[ilay]		= s_trackdata->s2[ilay];
            b_trackdata[splitIndex]->s3[ilay]		= s_trackdata->s3[ilay];
            b_trackdata[splitIndex]->row[ilay]		= s_trackdata->row[ilay];
            b_trackdata[splitIndex]->col[ilay]		= s_trackdata->col[ilay];
            b_trackdata[splitIndex]->Layer[ilay]	= s_trackdata->Layer[ilay];
            b_trackdata[splitIndex]->HalfBarrel[ilay]	= s_trackdata->HalfBarrel[ilay];
            b_trackdata[splitIndex]->Stave[ilay]	= s_trackdata->Stave[ilay];
            b_trackdata[splitIndex]->HalfStave[ilay]	= s_trackdata->HalfStave[ilay];
            b_trackdata[splitIndex]->Module[ilay] 	= s_trackdata->Module[ilay];
            b_trackdata[splitIndex]->Chip[ilay]		= s_trackdata->Chip[ilay];
            b_trackdata[splitIndex]->ChipID[ilay]	= s_trackdata->ChipID[ilay];
         }
         b_trackdata[splitIndex]->index = (int) s_trackdata->index;
         b_trackdata[splitIndex]->ncluster = (int) s_trackdata->ncluster;
         hNClusterByTrack[splitIndex]->Fill(s_trackdata->ncluster);
      }
      hNTrackByEvent[splitIndex]->Fill(s_eventdata->GetNtracks());

      b_eventdata[splitIndex]->SetEvno(NeventsAll);
      b_eventdata[splitIndex]->SetWE(0);
      b_eventdata[splitIndex]->SetNvtx(1);
      b_eventdata[splitIndex]->SetNtracks(s_eventdata->GetNtracks());  

      fDataInput[splitIndex]->Fill();  
      NeventsAll++;
   }

   std::cout<<" Alignment Input Format "<<std::endl; 

   int total_events = 0;
   for(int s=0; s<nsplit; s++){
      std::cout<<"Data Split Report #"<<s<<" nEVents "<<nSPLITINDEX[s]<<std::endl;
      fileOut_DataInput[s]->cd();
      fileOut_DataInput[s]->WriteObject(hNClusterByTrack[s],"hNClusterByTrack");
      fileOut_DataInput[s]->WriteObject(hNTrackByEvent[s],"hNTrackByEvent");
      fileOut_DataInput[s]->WriteObject(fDataInput[s],"DataInput");
      fileOut_DataInput[s]->Write();
      delete fileOut_DataInput[s];
      total_events += nSPLITINDEX[s];
   }
   std::cout<<"InputData : "<<tree->GetEntries()<<" # of Successfully splitted Data : "<<total_events<<std::endl;

}

void DataSplit(int seed = 1, int nsplit = 10, int bid = 0){
 
   std::clog<<"DataSplit STEP 1"<<std::endl;
   o2::base::GeometryManager::loadGeometry("", false, false);
   o2::its::GeometryTGeo* geom = o2::its::GeometryTGeo::Instance();
   geom->fillMatrixCache(o2::math_utils::bit2Mask(o2::math_utils::TransformType::L2G));// request cached transforms

   std::string inputfile = Form("alignment-input-data_%d.root",seed);

   std::clog<<"DataSplit STEP 2"<<std::endl;
   TFile* fInput = TFile::Open(inputfile.data());
   TTree* treeInput = (TTree*)gDirectory->Get("DataInput");

   std::clog<<"DataSplit STEP 3"<<std::endl;
   EventData* s_eventdata = nullptr;  
   treeInput->SetBranchAddress("event", &s_eventdata);

   std::clog<<"DataSplit STEP 4"<<std::endl;
   splitData(treeInput, s_eventdata, geom, seed, nsplit, bid);   

   //storePlots("monitor");
}
