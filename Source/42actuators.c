/*    This file is distributed with 42,                               */
/*    the (mostly harmless) spacecraft dynamics simulation            */
/*    created by Eric Stoneking of NASA Goddard Space Flight Center   */

/*    Copyright 2010 United States Government                         */
/*    as represented by the Administrator                             */
/*    of the National Aeronautics and Space Administration.           */

/*    No copyright is claimed in the United States                    */
/*    under Title 17, U.S. Code.                                      */

/*    All Other Rights Reserved.                                      */


#include "42.h"

/* #ifdef __cplusplus
** namespace _42 {
** using namespace Kit;
** #endif
*/

/**********************************************************************/
double ViscousFriction(struct WhlType *W)
{
      return(-W->ViscCoef*W->w);
}
/**********************************************************************/
/* Ref: Lugre Friction Model.pdf                                      */
void WhlDrag(struct WhlType *W)
{
      double v0,g,m,vd,zdot;
      
      v0 = W->w/W->StribeckZone;
      g = W->CoulCoef + W->StribeckCoef*exp(-(v0*v0));
      m = W->LugreSpringCoef*fabs(W->w)/g;
      if (m*DTSIM > 1.0) {
         zdot = (g*signum(W->w)/W->LugreSpringCoef - W->z)/DTSIM;
         W->FricTrq = -g*signum(W->w) + ViscousFriction(W);
      }
      else {
         zdot = W->w - m*W->z;
         vd = W->w/W->LugreDampZone;
         W->FricTrq = -W->LugreSpringCoef*W->z 
            - W->LugreDampCoef*exp(-vd*vd)*zdot
            + ViscousFriction(W);
      }
      
      W->z += zdot*DTSIM;
}
/**********************************************************************/
void WhlModel(struct WhlType *W,struct SCType *S)
{
      struct BodyType *B;
      struct NodeType *N;
      long i;

      W->Trq = W->Tcmd;
      if (S->WhlDragActive) {
         WhlDrag(W);
         W->Trq += W->FricTrq;
      }
      if (W->Trq < -W->Tmax) W->Trq = -W->Tmax;
      if (W->Trq >  W->Tmax) W->Trq =  W->Tmax;
      if (W->Trq < 0.0 && W->H <= -W->Hmax) W->Trq = 0.0;
      if (W->Trq > 0.0 && W->H >=  W->Hmax) W->Trq = 0.0;

      if (S->FlexActive) {
         B = &S->B[W->Body];
         N = &B->Node[W->Node];
         for(i=0;i<3;i++) N->Trq[i] -= W->Trq*W->A[i];
      }

}
/**********************************************************************/
void MTBModel(struct MTBType *MTB,double bvb[3])
{

      MTB->M = MTB->Mcmd;
      if(MTB->M < -MTB->Mmax) MTB->M = -MTB->Mmax;
      if(MTB->M >  MTB->Mmax) MTB->M =  MTB->Mmax;

      MTB->Trq[0] = MTB->M * (MTB->A[1]*bvb[2]-MTB->A[2]*bvb[1]);
      MTB->Trq[1] = MTB->M * (MTB->A[2]*bvb[0]-MTB->A[0]*bvb[2]);
      MTB->Trq[2] = MTB->M * (MTB->A[0]*bvb[1]-MTB->A[1]*bvb[0]);

}
/**********************************************************************/
void ThrModel(struct ThrType *Thr,struct SCType *S,double DT)
{
      struct BodyType *B;
      struct NodeType *N;
      long i;

      if (Thr->Mode == THR_PULSED) {
         if (Thr->PulseWidthCmd > DT) {
            Thr->F = Thr->Fmax;
            Thr->PulseWidthCmd -= DT;
         }
         else {
            Thr->F = (Thr->PulseWidthCmd/DT)*Thr->Fmax;
            Thr->PulseWidthCmd = 0.0;
         }
      }
      else { /* THR_PROPORTIONAL */
         Thr->F = Thr->ThrustLevelCmd*Thr->Fmax;
      }

      if (Thr->F < 0.0) Thr->F = 0.0;
      if (Thr->F > Thr->Fmax) Thr->F = Thr->Fmax;

      Thr->Frc[0] = Thr->F*Thr->A[0];
      Thr->Frc[1] = Thr->F*Thr->A[1];
      Thr->Frc[2] = Thr->F*Thr->A[2];

      B = &S->B[Thr->Body];
      N = &B->Node[Thr->Node];

      VxV(N->PosCm,Thr->Frc,Thr->Trq);

      if (S->FlexActive) {
         for(i=0;i<3;i++) {
            N->Trq[i] += Thr->Trq[i];
            N->Frc[i] += Thr->Frc[i];
         }
      }
}
/**********************************************************************/
void ThrusterPlumeFrcTrq(struct SCType *S)
{
      /* Plume Parameters */
      double Temp = 100.0; /* WAG */
      double R = 8.134; /* J/(K*mol) */
      double Beta = 1.0/sqrt(2.0*R*Temp);
      double Ve = 100.0; /* WAG */
      double s = Beta*Ve;
      double y = s;
      double A1 = exp(-y*y)+sqrt(Pi)*y*(1.0+erf(y));
      double mdot = 0.1; /* WAG */
      double Coef = mdot/(Beta*A1*Pi);
      /* Other variables */
      struct ThrType *T;
      struct BodyType *B,*Bt;
      struct NodeType *Nt;
      struct MeshType *M;
      struct PolyType *P;
      double PosThrN[3],PosThrB[3],AxisN[3],CPB[3][3];
      double AoN,PosB[3],PosP[3],MagPos,Phat[3],cosphi;
      double w,w2,Wpoly,TotalCoef,FrcP[3],FrcB[3],r[3],TrqB[3],FrcN[3];
      long Ithr,Ipoly,Ib,i;

      for(Ithr=0;Ithr<S->Nthr;Ithr++) {
         T = &S->Thr[Ithr];
         Bt = &S->B[T->Body];
         Nt = &Bt->Node[T->Node];
         
         if (T->F > 0.0) { /* Check that this is legit */

            /* Find Force and Torque on each Body */
            for(Ib=0;Ib<S->Nb;Ib++) {
               B = &S->B[Ib];
               M = &Mesh[B->MeshTag];

               /* Find thruster location, axis in B */
               MTxV(Bt->CN,Nt->PosB,PosThrN);
               for(i=0;i<3;i++) PosThrN[i] += Bt->pn[i] - B->pn[i];
               MxV(B->CN,PosThrN,PosThrB);
               /* Note that plume axis is opposite T->A */
               /* CPB is DCM from B to Plume (P) frame */
               MTxV(Bt->CN,T->A,AxisN);
               MxV(B->CN,AxisN,CPB[0]);
               for(i=0;i<3;i++) CPB[0][i] = -CPB[0][i];
               PerpBasis(CPB[0],CPB[1],CPB[2]);

               /* Find force and torque on each illuminated polygon */
               for(Ipoly=0;Ipoly<M->Npoly;Ipoly++) {
                  P = &M->Poly[Ipoly];
                  if (strcmp(Matl[P->Matl].Label,"INTERIOR")) { /* Plume doesn't see interior polys */
                     AoN = VoV(CPB[0],P->Norm);
                     if (AoN < 0.0) {  /* Plume doesn't see polys facing away */
                        /* Find plume pressure (momentum flux) at poly centroid */
                        for(i=0;i<3;i++) PosB[i] = P->Centroid[i] - PosThrB[i];
                        MxV(CPB,PosB,PosP);
                        if (PosP[0] > 0.0) { /* Ignore backflow */
                           MagPos = CopyUnitV(PosP,Phat);
                           cosphi = Phat[0];
                           w = s*cosphi;
                           w2 = w*w;
                           Wpoly = (w2+2.5)*w*exp(-w2);
                           Wpoly += (0.75+3.0*w2+w2*w2)*sqrt(Pi)*(1.0+erf(w));
                           Wpoly *= exp(w2-s*s);
                           TotalCoef = P->Area*Coef*cosphi*Wpoly/(MagPos*MagPos);

                           for(i=0;i<3;i++) FrcP[i] = TotalCoef*Phat[i];
                           MTxV(CPB,FrcP,FrcB);

                           /* Find plume force in B frame */
                           for(i=0;i<3;i++) r[i] = P->Centroid[i]-B->cm[i];
                           VxV(r,FrcB,TrqB);
                           MTxV(B->CN,FrcB,FrcN);
                           for(i=0;i<3;i++) {
                              B->FrcN[i] += FrcN[i];
                              B->FrcB[i] += FrcB[i];
                              B->Trq[i] += TrqB[i];
                           }
                        }
                     }
                  }
               }
            }
         }
      }
}
/**********************************************************************/
/*  This function is called at the simulation rate.  Sub-sampling of  */
/*  actuators should be done on a case-by-case basis.                 */

void Actuators(struct SCType *S)
{

      struct NodeType *N;
      long i,j;
      double FrcN[3],FrcB[3];
      struct AcType *AC;
      struct JointType *G;
      struct AcJointType *AG;
      struct ThrType *Thr;
      struct ShakerType *Sh;
      struct WhlType *W;

      AC = &S->AC;

      /* Ideal Actuators */
      for(i=0;i<3;i++) FrcB[i] = S->IdealAct[i].Fcmd;
      MTxV(S->B[0].CN,FrcB,FrcN);
      for(i=0;i<3;i++) {
         S->B[0].FrcB[i] += FrcB[i];
         S->B[0].FrcN[i] += FrcN[i];
         S->B[0].Trq[i] += S->IdealAct[i].Tcmd;
      }
      if (S->FlexActive) {
         N = &S->B[0].Node[0]; /* Arbitrarily put ideal actuators at Node 0 */
         for(i=0;i<3;i++) {
            N->Trq[i] += S->IdealAct[i].Tcmd;
            N->Frc[i] += S->IdealAct[i].Fcmd;
         }
      }

      /* Wheels */
      for(i=0;i<S->Nw;i++) {
         WhlModel(&S->Whl[i],S);
      }
      /* MTBs */
      for(i=0;i<S->Nmtb;i++){
         MTBModel(&S->MTB[i],S->bvb);
         for(j=0;j<3;j++){
            S->B[0].Trq[j] += S->MTB[i].Trq[j];
         }
      }
      
      /* Gimbal Drives */
      for(i=0;i<AC->Ng;i++) {
         G = &S->G[i];
         AG = &AC->G[i];
         if (G->Type == ACTUATED_JOINT) {
            for(j=0;j<G->RotDOF;j++) {
               G->AngRateCmd[j] = AG->GCmd.AngRate[j];
            }
            for(j=0;j<G->TrnDOF;j++) {
               G->PosRateCmd[j] = AG->GCmd.PosRate[j];
            }
         }
      }

      /* Thrusters */
      for(i=0;i<S->Nthr;i++) {
         Thr = &S->Thr[i];
         ThrModel(Thr,S,DTSIM);
         MTxV(S->B[Thr->Body].CN,Thr->Frc,FrcN);
         for(j=0;j<3;j++) {
            S->B[Thr->Body].Trq[j] += Thr->Trq[j];
            S->B[Thr->Body].FrcN[j] += FrcN[j];
            S->B[Thr->Body].FrcB[j] += Thr->Frc[j];
         }
      }
      if (ThrusterPlumesActive) {
         ThrusterPlumeFrcTrq(S);
      }
      
      /* Wheel Jitter and Shakers only affect Flex */
      if (S->FlexActive) {
         for(i=0;i<S->Nsh;i++) {
            Sh = &S->Shaker[i];
            N = &S->B[Sh->Body].Node[Sh->Node];
            ShakerJitter(Sh,S);
            if (Sh->FrcTrq == FORCE) {
               for(j=0;j<3;j++) N->Frc[j] += Sh->Output*Sh->Axis[j];
            }
            else {
               for(j=0;j<3;j++) N->Trq[j] += Sh->Output*Sh->Axis[j];
            }
         }
         
         if (S->WhlJitterActive) {
            for(i=0;i<S->Nw;i++) {
               W = &S->Whl[i];
               N = &S->B[W->Body].Node[W->Node];
               WheelJitter(W,S);
               for(j=0;j<3;j++) {
                  N->Frc[j] += W->JitFrc[j];
                  N->Trq[j] += W->JitTrq[j];
               }
            }
         }
      }
            
}

/* #ifdef __cplusplus
** }
** #endif
*/

