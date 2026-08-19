// @(#)location
// Author: J.H.Kim

/*************************************************************************
 *   Yonsei Univ.                                                        *
 *                                                                       *
 *                                                                       *
 *                                                                       *
 *                                                                       *
 *************************************************************************/

///////////////////////////////////////////////////////////////////////////
//
// YDetectorGeometry
//
//
///////////////////////////////////////////////////////////////////////////

#include "YDetectorGeometry.h"

static void at_exit_of_YDetectorGeometry() {
   if (GEOM::Internal::yGEOMLocal)
      GEOM::Internal::yGEOMLocal->~YDetectorGeometry();
}

// This local static object initializes the GEOM system
namespace GEOM {
namespace Internal {
   class YDetectorGeometryAllocator {

      char fHolder[sizeof(YDetectorGeometry)];
   public:
      YDetectorGeometryAllocator() {
         new(&(fHolder[0])) YDetectorGeometry("geom","The GEOM of EVERYTHING");
      }

      ~YDetectorGeometryAllocator() {
         if (yGEOMLocal) {
            yGEOMLocal->~YDetectorGeometry();
         }
      }
   };

   extern YDetectorGeometry *yGEOMLocal;

   YDetectorGeometry *GetGEOM1() {
      if (yGEOMLocal)
         return yGEOMLocal;
      static YDetectorGeometryAllocator alloc;
      return yGEOMLocal;
   }

   YDetectorGeometry *GetGEOM2() {
      static Bool_t initInterpreter = kFALSE;
      if (!initInterpreter) {
         initInterpreter = kTRUE;
      }
      return yGEOMLocal;
   }
   typedef YDetectorGeometry *(*GetGEOMFun_t)();

   static GetGEOMFun_t yGetGEOM = &GetGEOM1;


} // end of Internal sub namespace
// back to GEOM namespace

   YDetectorGeometry *GetGEOM() {
      return (*Internal::yGetGEOM)();
   }
}

YDetectorGeometry *GEOM::Internal::yGEOMLocal = GEOM::GetGEOM();

ClassImp(YDetectorGeometry);

////////////////////////////////////////////////////////////////////////////////
/// Default Constructor YSensorSet

YDetectorGeometry::YDetectorGeometry(const char *name, const char *title) {
   std::cout<<"Default Constructor YDetectorGeometry "<<std::endl;
   //These are supposed to be defined as constant header.

   o2::base::GeometryManager::loadGeometry("",false, false);
   //o2::detectors::DetID detITS("ITS");

   geom = o2::its::GeometryTGeo::Instance();

   geom->fillMatrixCache(o2::math_utils::bit2Mask(o2::math_utils::TransformType::T2L, o2::math_utils::TransformType::T2GRot, o2::math_utils::TransformType::L2G));

   GEOM::Internal::yGEOMLocal = this;
   GEOM::Internal::yGetGEOM = &GEOM::Internal::GetGEOM2;   
}

////////////////////////////////////////////////////////////////////////////////
/// GToS

TVector3 YDetectorGeometry::GToS(int chipID, double gx, double gy, double gz)
{
   if(chipID<0){
      TVector3 v3(-9999,-9999,-9999);
      return v3;     
   }
   
   o2::math_utils::Point3D<float> gloC(gx, gy, gz);
   auto loc = geom->getMatrixL2G(chipID) ^ gloC; // convert global coordinates to local.
   float l1= loc.X(); //xrow s1
   float l2= loc.Y(); //s3
   float l3= loc.Z(); //zcol s2

   
   int Layer = GetLayer(chipID);

   if(Layer<NLayerIB){
      TVector3 v3(-l3,-l1,l2);   
      return v3;
   } else {
      //  for OB (chipID in Module)
      //   6  5  4  3  2  1  0    	 (+)          
      //   7  8  9 10 11 12 13     	 (-)  -> Z
  
      //                  ///////////////////////
      //                  //                   //
      // 6  5  4  3  2  1 //         0         //
      //                  //                   //
      //                  /////////////////////## (0,0)
      
      //            (0,0) ##/////////////////////
      //                  //                   //
      // 7  8  9 10 11 12 //         13        //
      //                  //                   //
      //                  ///////////////////////
      short dir        = (GetChipIdInModule(chipID)<7) ? +1 : -1;
      TVector3 v3(-dir*l3,-dir*l1,l2);   
      return v3;
   }
}

////////////////////////////////////////////////////////////////////////////////
/// GToL

TVector3 YDetectorGeometry::GToL(int chipID, double gx, double gy, double gz)
{
   if(chipID<0){
      TVector3 v3(-9999,-9999,-9999);
      return v3;     
   }
   
   TVector3 gtos = GToS(chipID, gx, gy, gz);

   float l1= -gtos(1);
   float l2= gtos(2);
   float l3= -gtos(0);
   //TVector3 v3(l3,l1,l2);
   float frow, fcol;
   //o2::itsmft::SegmentationAlpide::localToDetector(l1, l3 ,row, col);
   //
   // convert to row/col w/o over/underflow check
   l1 = 0.5 * (ActiveMatrixSizeRows - PassiveEdgeTop + PassiveEdgeReadOut) - l1; // coordinate wrt top edge of Active matrix
   l3 += 0.5 * ActiveMatrixSizeCols;                                               // coordinate wrt left edge of Active matrix
   frow = float(l1 / PitchRow) - 0.5;
   fcol = float(l3 / PitchCol) - 0.5;
   if (l1 < 0) {
     frow -= 1;
   }
   if (l3 < 0) {
     fcol -= 1;
   }
   //
   TVector3 v3(frow,fcol,0);
   return v3;
}

////////////////////////////////////////////////////////////////////////////////
/// SToL

TVector3 YDetectorGeometry::SToL(int chipID, double s1, double s2, double s3){

   if(chipID<0){
      TVector3 v3(-9999,-9999,-9999);
      return v3;     
   }

   int Layer = GetLayer(chipID);

   if(Layer<NLayerIB){
      s1 = -s1;
      s2 = -s2;
   } else {
      //  for OB (chipID in Module)
      //   6  5  4  3  2  1  0    	 (+)          
      //   7  8  9 10 11 12 13     	 (-)  -> Z
  
      //                  ///////////////////////
      //                  //                   //
      // 6  5  4  3  2  1 //         0         //
      //                  //                   //
      //                  /////////////////////## (0,0)
      
      //            (0,0) ##/////////////////////
      //                  //                   //
      // 7  8  9 10 11 12 //         13        //
      //                  //                   //
      //                  ///////////////////////
      short dir        = (GetChipIdInModule(chipID)<7) ? +1 : -1;      
      s1 = -dir*s1;
      s2 = -dir*s2;
   }

   s2 = 0.5 * (ActiveMatrixSizeRows - PassiveEdgeTop + PassiveEdgeReadOut) - s2;   // coordinate wrt top edge of Active matrix
   s1 += 0.5 * ActiveMatrixSizeCols;                                               // coordinate wrt left edge of Active matrix
   float frow = float(s2 / PitchRow) - 0.5;
   float fcol = float(s1 / PitchCol) - 0.5;
   TVector3 v3(frow,fcol,0);
   return v3;

}

////////////////////////////////////////////////////////////////////////////////
/// SToG

TVector3 YDetectorGeometry::SToG(int chipID, double s1, double s2, double s3){

   if(chipID<0){
      TVector3 v3(-9999,-9999,-9999);
      return v3;     
   }

   TVector3 stol = SToL(chipID, s1, s2, s3);
   float row = stol(0);
   float col = stol(1);

   TVector3 ltog = LToG(chipID, row, col);

   float gx = ltog.X();
   float gy = ltog.Y();
   float gz = ltog.Z();

   TVector3 normV = NormalVector(chipID);
   float dgx = s3*normV(0);
   float dgy = s3*normV(1);
/* 
   o2::math_utils::Point3D<float> sensorC(s1,s2,s3);
   auto gloC = geom->getMatrixL2G(chipID) * sensorC;
   float gx = gloC.X();
   float gy = gloC.Y();
   float gz = gloC.Z();
   TVector3 v3(gx,gy,gz);
*/
   TVector3 v3(gx+dgx,gy+dgy,gz);
   return v3;
}

////////////////////////////////////////////////////////////////////////////////
/// LToG

TVector3 YDetectorGeometry::LToG(int chipID, float row, float col)
{
   if(chipID<0){
      TVector3 v3(-9999,-9999,-9999);
      return v3;     
   }

   o2::math_utils::Point3D<float> locC;
   //o2::itsmft::SegmentationAlpide::detectorToLocal(row, col, locC); // local coordinates
   o2::itsmft::SegmentationAlpide::detectorToLocalUnchecked(row, col, locC); // local coordinates
   auto gloC = geom->getMatrixL2G(chipID) * locC;

   float gx = gloC.X();
   float gy = gloC.Y();
   float gz = gloC.Z();

   TVector3 v3(gx,gy,gz);
   return v3;
}

TVector3 YDetectorGeometry::NormalVector(int chipID){				// Stave plane 
   if(chipID<0){
      TVector3 e3(-9999,-9999,-9999);
      return e3;     
   }
   
      //  for OB (chipID in Module)
      //   6  5  4  3  2  1  0    	 (+)          
      //   7  8  9 10 11 12 13     	 (-)  -> Z
  
      //                  ///////////////////////
      //                  //                   //
      // 6  5  4  3  2  1 //   v1    0         //
      //                  //        v2         //
      //                  /////////////////////## (0,0)
      
      //            (0,0) ##/////////////////////
      //                  //         v2        //
      // 7  8  9 10 11 12 //         13  v1    //
      //                  //                   //
      //                  ///////////////////////
      
   TVector3 v1(LToG(chipID,256,512).X()-LToG(chipID,256,0).X(),
               LToG(chipID,256,512).Y()-LToG(chipID,256,0).Y(),
               LToG(chipID,256,512).Z()-LToG(chipID,256,0).Z());
   TVector3 v2(LToG(chipID,0,0).X()-LToG(chipID,256,0).X(),
               LToG(chipID,0,0).Y()-LToG(chipID,256,0).Y(),
               LToG(chipID,0,0).Z()-LToG(chipID,256,0).Z());           
   TVector3 v2v1 = v2.Cross(v1);
   double m2m1 = TMath::Sqrt((v2v1.X()*v2v1.X())+(v2v1.Y()*v2v1.Y())+(v2v1.Z()*v2v1.Z()));
   TVector3 v3(v2v1.X()/m2m1,v2v1.Y()/m2m1,v2v1.Z()/m2m1);
   return v3;

}

int YDetectorGeometry::GetLayer(int index) const {
   if(index<0) return -9999;
   return geom->getLayer(index);
}

int YDetectorGeometry::GetHalfBarrel(int index) const {
   if(index<0) return -9999;
   return geom->getHalfBarrel(index);
}

int YDetectorGeometry::GetStave(int index) const {
   if(index<0) return -9999;
   return geom->getStave(index);
}

int YDetectorGeometry::GetHalfStave(int index) const {
   if(index<0) return -9999;
   return geom->getHalfStave(index);
}

int YDetectorGeometry::GetModule(int index) const {
   if(index<0) return -9999;
   return geom->getModule(index);
}

int YDetectorGeometry::GetChipIdInLayer(int index) const {
   if(index<0) return -9999;
   return geom->getChipIdInLayer(index);
}

int YDetectorGeometry::GetChipIdInStave(int index) const {
   if(index<0) return -9999;
   return geom->getChipIdInStave(index);
}

int YDetectorGeometry::GetChipIdInHalfStave(int index) const {
   if(index<0) return -9999;
   return geom->getChipIdInHalfStave(index);
}

int YDetectorGeometry::GetChipIdInModule(int index) const {
   if(index<0) return -9999;
   return geom->getChipIdInModule(index);
}
