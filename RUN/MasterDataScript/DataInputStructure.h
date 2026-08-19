#define nLAYER 		7

// Which fields the input tree carries. Generated from config/alignment.conf
// by config/alignctl.sh, because the 2025 module's tree has a per-track
// charge and the 2024 module's does not. Deliberately not defaulted here: a
// silent guess would produce a tree the module misreads rather than an error.
#include "DataSchema.h"

// TrackData Class //
class TrackData : public TObject {
   public:
  
      double s1[nLAYER], s2[nLAYER], s3[nLAYER];
      float row[nLAYER], col[nLAYER];
      int Layer[nLAYER], HalfBarrel[nLAYER], Stave[nLAYER], HalfStave[nLAYER], Module[nLAYER], Chip[nLAYER], ChipID[nLAYER];
      //TString Det[nLAYER];
      int index, ncluster;  
      double p, pt, theta, phi, eta;      
#if ALIGN_TRACK_HAS_CHARGE
      int charge;
#endif
      double tv1, 	tv2,		tv3;
      double tv1_X0,	tv2_X0, 	tv3_X0;
      double tv1_DCA, 	tv2_DCA, 	tv3_DCA;

   public:
      TrackData() = default;
      ~TrackData(){;}	
      TrackData(TrackData* TrackData);
            
   ClassDef(TrackData, 1);
};

ClassImp(TrackData);

TrackData::TrackData(TrackData* trackdata){
   //index = trackdata->GetIndex();
   for(int l = 0; l < nLAYER; l++){
      s1[l]		= trackdata->s1[l];
      s2[l]		= trackdata->s2[l];
      s3[l]		= trackdata->s3[l];
      row[l]		= trackdata->row[l];
      col[l]		= trackdata->col[l];

      HalfBarrel[l]	= trackdata->HalfBarrel[l];
      Stave[l]		= trackdata->Stave[l];
      HalfStave[l]	= trackdata->HalfStave[l];
      Module[l] 	= trackdata->Module[l];
      Chip[l]		= trackdata->Chip[l];
      ChipID[l]		= trackdata->ChipID[l];
      //Det[l]    	= trackdata->Det[l];
   }    
   index    = trackdata->index;
   ncluster = trackdata->ncluster;
   p        = trackdata->p;
   pt       = trackdata->pt;
   theta    = trackdata->theta;
   phi      = trackdata->phi;
   eta      = trackdata->eta;
#if ALIGN_TRACK_HAS_CHARGE
   charge   = trackdata->charge;
#endif
   tv1	    = trackdata->tv1;
   tv2      = trackdata->tv2;
   tv3      = trackdata->tv3;
   tv1_X0   = trackdata->tv1_X0;	
   tv2_X0   = trackdata->tv2_X0;	
   tv3_X0   = trackdata->tv3_X0;
   tv1_DCA  = trackdata->tv1_DCA;	
   tv2_DCA  = trackdata->tv2_DCA; 	
   tv3_DCA  = trackdata->tv3_DCA;  
      
}

// EventData Class //

class EventData : public TObject {
   private:
      TClonesArray *Track;   
      double X1;
      double X2;
      double X3;
      double P1;
      double P2;
      double P3;     
       
      int evno;
      double WE;
      int ntracks;
      int nvtx;      
   public:
      EventData();
      ~EventData(){;}	
      EventData(EventData* eventdata);

      TrackData* AddOneTrack();
      TrackData* AddOneTrack(TrackData *track_source);                  
      TrackData* CopyAllTracks(TClonesArray *track_source);   

      int GetEvno()    { return evno;}
      int GetWE()    { return WE;}
      int GetNtracks() { return ntracks;}
      double GetX1() { return X1;}
      double GetX2() { return X2;}
      double GetX3() { return X3;}   
      double GetP1() { return P1;}
      double GetP2() { return P2;}
      double GetP3() { return P3;}  
      double GetNvtx() {return nvtx;}                                  
      TClonesArray *GetTrack() {return Track;}
      
      void SetEvno(int i)    { evno = i;}
      void SetWE(double w)    { WE = w;}
      void SetNtracks(int h) { ntracks = h;}      
      void SetX1(double x) { X1 = x;} 
      void SetX2(double y) { X2 = y;} 
      void SetX3(double z) { X3 = z;}             
      void SetP1(double x) { P1 = x;} 
      void SetP2(double y) { P2 = y;} 
      void SetP3(double z) { P3 = z;} 
      void SetNvtx(int n) {  nvtx = n;}                   
              
   ClassDef(EventData, 1);
};

ClassImp(EventData);

EventData::EventData() {
   Track = new TClonesArray("TrackData",1);
}


EventData::EventData(EventData* eventdata){
   evno         = eventdata->GetEvno();
   WE		= eventdata->GetWE();
   ntracks	= eventdata->GetNtracks();
   nvtx	 	= eventdata->GetNvtx();   
   X1		= eventdata->GetX1();
   X2		= eventdata->GetX2();
   X3		= eventdata->GetX3();
   P1		= eventdata->GetP1();
   P2		= eventdata->GetP2();
   P3		= eventdata->GetP3();   
}

TrackData* EventData::AddOneTrack() {
   TClonesArray &track = *Track;
   TrackData *trackdata = new(track[ntracks++]) TrackData();
   return trackdata;
}

TrackData* EventData::AddOneTrack(TrackData *track_source) {
   TrackData *trackdata;
   TClonesArray &track = *Track;
   trackdata = new(track[track.GetEntriesFast()]) TrackData((TrackData *) track_source);                      
   return trackdata;
}
TrackData* EventData::CopyAllTracks(TClonesArray *track_source) {
   TrackData *trackdata;
   for(int a=0; a< track_source->GetEntriesFast(); a++ ){
      TClonesArray &track = *Track;
      trackdata = new(track[track.GetEntriesFast()]) TrackData((TrackData *) track_source->At(a));                      
   }      
   return trackdata;
}

