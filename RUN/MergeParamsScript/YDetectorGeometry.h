// @(#)location
// Author: J.H.Kim

/*************************************************************************
 *   Yonsei Univ.                                                        *
 *                                                                       *
 *                                                                       *
 *                                                                       *
 *                                                                       *
 *************************************************************************/

#ifndef ROOT_YDetectorGeometry
#define ROOT_YDetectorGeometry

#include "TObject.h"
#include "TString.h"
#include <vector>

#if !defined(__CLING__) || defined(__ROOTCLING__)
//#define ENABLE_UPGRADES
#include "DetectorsCommonDataFormats/DetID.h"
#include "DetectorsCommonDataFormats/NameConf.h"
#include "DetectorsCommonDataFormats/AlignParam.h"
#include "DetectorsBase/GeometryManager.h"
#include "CCDB/CcdbApi.h"
#include "ITSBase/GeometryTGeo.h"
#include <TRandom.h>
#include <TFile.h>
#include <vector>
#include <fmt/format.h>
#endif
//____________________________________________________________________
//
// YDetectorGeometry
//
// Description
//
//--------------------------------------------------------------------
class YDetectorGeometry;

namespace GEOM {
namespace Internal {
   class YDetectorGeometryAllocator;

   YDetectorGeometry *GetGEOM2();

} 
} // End ROOT::Internal


class YDetectorGeometry {

friend YDetectorGeometry *GEOM::Internal::GetGEOM2();

private:
   YDetectorGeometry(const YDetectorGeometry&);                   			//Not implemented
   YDetectorGeometry& operator=(const YDetectorGeometry&);        			//Not implemented

protected:
   YDetectorGeometry(const char *name, const char *title);              				  
   friend class ::GEOM::Internal::YDetectorGeometryAllocator;

public:
   YDetectorGeometry();
   virtual ~YDetectorGeometry() {};  
      
   o2::its::GeometryTGeo* GetGeom() { return geom;}

   TVector3 GToS(int chipID, double gx, double gy, double gz);
   TVector3 GToL(int chipID, double gx, double gy, double gz);
   TVector3 SToL(int chipID, double s1, double s2, double s3);
   TVector3 SToG(int chipID, double s1, double s2, double s3);
   TVector3 LToG(int chipID, float lx, float ly);
   TVector3 LToS(int chipID, float lx, float ly); //not implemented LToG -> GToS
   TVector3 NormalVector(int chipID);				// Stave plane 
   TVector3 NormalVector_Deformation(int chipID, double ds1_0, double ds2_0, double *ds3);   			
   TVector3 Function_Deformation(int chipID, float lx, float ly, double ds1_0, double ds2_0, double *ds3);

   int GetLayer(int index) const;    /// Get chip layer, from 0
   int GetHalfBarrel(int index) const;   /// Get chip half barrel, from 0
   int GetStave(int index) const;   /// Get chip stave, from 0
   int GetHalfStave(int index) const;   /// Get chip substave id in stave, from 0
   int GetModule(int index) const;   /// Get chip module id in substave, from 0
   int GetChipIdInLayer(int index) const;   /// Get chip number within layer, from 0
   int GetChipIdInStave(int index) const;   /// Get chip number within stave, from 0
   int GetChipIdInHalfStave(int index) const;   /// Get chip number within stave, from 0
   int GetChipIdInModule(int index) const;   /// Get chip number within module, from 0


   
private:
   o2::its::GeometryTGeo* geom;

public:
   static constexpr int NCols = 1024;
   static constexpr int NRows = 512;
   static constexpr int NPixels = NRows * NCols;
   static constexpr float PitchCol = 29.24e-4;
   static constexpr float PitchRow = 26.88e-4;
   static constexpr float PassiveEdgeReadOut = 0.12f;              // width of the readout edge (Passive bottom)
   static constexpr float PassiveEdgeTop = 37.44e-4;               // Passive area on top
   static constexpr float PassiveEdgeSide = 29.12e-4;              // width of Passive area on left/right of the sensor
   static constexpr float ActiveMatrixSizeCols = PitchCol * NCols; // Active size along columns
   static constexpr float ActiveMatrixSizeRows = PitchRow * NRows; // Active size along rows

   // effective thickness of sensitive layer, accounting for charge collection non-unifoemity, https://alice.its.cern.ch/jira/browse/AOC-46
   static constexpr float SensorLayerThicknessEff = 28.e-4;
   static constexpr float SensorLayerThickness = 30.e-4;                                               // physical thickness of sensitive part
   static constexpr float SensorSizeCols = ActiveMatrixSizeCols + PassiveEdgeSide + PassiveEdgeSide;   // SensorSize along columns
   static constexpr float SensorSizeRows = ActiveMatrixSizeRows + PassiveEdgeTop + PassiveEdgeReadOut; // SensorSize along rows
   
  //detector information

   static constexpr int NLayer = 7;   //layer number in ITS detector
   static constexpr int NLayerIB = 3;

   static constexpr int NSubStave2[NLayer] = { 1, 1, 1, 2, 2, 2, 2 };
   const int NSubStave[NLayer] = { 1, 1, 1, 2, 2, 2, 2 };
   const int NStaves[NLayer] = { 12, 16, 20, 24, 30, 42, 48 };
   const int nHicPerStave[NLayer] = { 1, 1, 1, 8, 8, 14, 14 };
   const int nChipsPerHic[NLayer] = { 9, 9, 9, 14, 14, 14, 14 };
   const int ChipBoundary[NLayer + 1] = { 0, 108, 252, 432, 3120, 6480, 14712, 24120 };
   const int StaveBoundary[NLayer + 1] = { 0, 12, 28, 48, 72, 102, 144, 192 };
   const int ReduceFraction = 1;
   const float StartAngle[7] = { 16.997 / 360 * (TMath::Pi() * 2.), 17.504 / 360 * (TMath::Pi() * 2.), 17.337 / 360 * (TMath::Pi() * 2.), 8.75 / 360 * (TMath::Pi() * 2.), 7 / 360 * (TMath::Pi() * 2.), 5.27 / 360 * (TMath::Pi() * 2.), 4.61 / 360 * (TMath::Pi() * 2.) }; //start angle of first stave in each layer
   const float MidPointRad[7] = { 23.49, 31.586, 39.341, 197.598, 246.944, 345.348, 394.883 }; 

   ClassDef(YDetectorGeometry,0)  							//Top level (or geom) structure for all classes   
};

namespace GEOM {
   YDetectorGeometry *GetGEOM();
   namespace Internal {
   
      R__EXTERN YDetectorGeometry *yGEOMLocal;
   }
}

#define yGEOM (GEOM::GetGEOM())

#endif
