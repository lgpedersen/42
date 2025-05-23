/*    This file is distributed with 42,                               */
/*    the (mostly harmless) spacecraft dynamics simulation            */
/*    created by Eric Stoneking of NASA Goddard Space Flight Center   */

/*    Copyright 2010 United States Government                         */
/*    as represented by the Administrator                             */
/*    of the National Aeronautics and Space Administration.           */

/*    No copyright is claimed in the United States                    */
/*    under Title 17, U.S. Code.                                      */

/*    All Other Rights Reserved.                                      */


/* Disable extern keyword to declare globals */
#ifdef DECLARE_GLOBALS
   #define EXTERN
#else
   #define EXTERN extern
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "42types.h"
#include "42defines.h"
#include "dcmkit.h"
#include "envkit.h"
#include "iokit.h"
#include "mathkit.h"
#include "meshkit.h"
#include "orbkit.h"
#include "sigkit.h"
#include "sphkit.h"
#include "timekit.h"
#include "fswkit.h"
#include "msis86kit.h"

/*
** #ifdef __cplusplus
** namespace _42 {
** using namespace Kit;
** #endif
*/

/* Number of Reference Orbits */
EXTERN long Norb;
/* Number of spacecraft */
EXTERN long Nsc;

/* Number of materials */
EXTERN long Nmatl;
/* Number of geometric meshes */
EXTERN long Nmesh;

EXTERN char InOutPath[512];
EXTERN char ModelPath[512];
EXTERN char CmdFileName[80];

/* Math Basics */
EXTERN double Pi, TwoPi, HalfPi, SqrtTwo, SqrtHalf, D2R, R2D, A2R, R2A, GoldenRatio;

/* Simulation Control */
EXTERN long TimeMode; /* FAST_TIME, REAL_TIME, EXTERNAL_SYNCH, NOS3_TIME */
EXTERN double SimTime,STOPTIME,DTSIM,DTOUT,DTOUTGL;
EXTERN long OutFlag,GLOutFlag,GLEnable,CleanUpFlag;

/* Environment */
EXTERN struct SphereHarmType MagModel;  /* -3,...,10 */
EXTERN long SurfaceModel;  /* 0=Brick, 1=CylPlate */
EXTERN struct SphereHarmType EarthGravModel;  /* Degree and Order of Earth gravitational model */
EXTERN struct SphereHarmType MarsGravModel;  /* Degree and Order of Mars gravitational model */
EXTERN struct SphereHarmType LunaGravModel;  /* Degree and Order of Luna gravitational model */
EXTERN long AeroActive;
EXTERN long AeroShadowsActive;
EXTERN long GGActive;
EXTERN long SolPressActive;
EXTERN long SolPressShadowsActive;
EXTERN long GravPertActive;
EXTERN long ThrusterPlumesActive;
EXTERN long ResidualDipoleActive;
EXTERN long ContactActive;
EXTERN long SloshActive;
EXTERN long AlbedoActive; /* Affects CSS measurements */
EXTERN long ComputeEnvTrq;
EXTERN long EphemOption; /* MEAN or DE430 */

/* Calendar Time is all based in Terrestrial Dynamical Time (TT or TDT) unless otherwise noted */
EXTERN double DynTime0; /* Time in sec since J2000 Epoch at Sim Start (TT) */
EXTERN double DynTime; /* Absolute Time (TT), sec since J2000 Epoch */
EXTERN double AtomicTime; /* TAI = TT - 32.184 sec, sec since J2000 */
EXTERN double LeapSec; /* Add to civil time (UTC) to synch with TAI */
EXTERN double CivilTime; /* UTC = TAI - LeapSec */
EXTERN double GpsTime; /* GPS Time = TAI - 19.0 sec */
EXTERN struct DateType TT; /* Terrestrial Dynamical Time */
EXTERN struct DateType UTC; /* Universal Time Coordinated */
EXTERN long GpsRollover,GpsWeek;
EXTERN double GpsSecond;

/* Parameters for environmental models  */
EXTERN long AtmoOption; /* TWOSIGMA_ATMO, NOMINAL_ATMO, USER_ATMO */
EXTERN double Flux10p7, GeomagIndex;
EXTERN double SchattenTable[5][410]; /* JD, +2sig F10.7, Nom F10.7, +2sig Kp, Nom Kp */

EXTERN struct WorldType World[NWORLD];
EXTERN struct LagrangeSystemType LagSys[3];

/* Galactic Coordinate Frame */
EXTERN double CGH[3][3];

/* J2000 to Heliocentric Ecliptic */
EXTERN double qjh[4];

/* Orb structure manages Reference Orbits */
EXTERN struct OrbitType *Orb;
/* SC structure manages attitude and translation wrt Reference Orbit */
EXTERN struct SCType *SC;
/* Frm structure describes a Formation of S/C's */
EXTERN struct FormationType *Frm;

/* Mesh structure manages geometric objects, used for display and    */
/* for surface force computation (e.g. aerodynamic)                  */
EXTERN struct MeshType *Mesh;

EXTERN struct POVType POV;

EXTERN struct TdrsType Tdrs[10];
EXTERN struct GroundStationType *GroundStation;
EXTERN long Ngnd;

/* For drawing Fields of View */
EXTERN long Nfov;
EXTERN struct FovType *FOV;

/* Materials for graphics and solar pressure forces.                  */
EXTERN struct MatlType *Matl;

/* Framebuffer Objects for Shadows and Surface Forces */
EXTERN struct ShadowFBOType ShadowMap;
EXTERN struct AlbedoFBOType AlbedoFBO;

/* Minor Bodies (Asteroids and Comets) */
EXTERN long Nmb;

EXTERN long Nrgn;
EXTERN struct RegionType *Rgn;

/* CFD execution control */
EXTERN long ExecuteCFDStep;
EXTERN long EndCFD;

/* Inter-Process Comm */
EXTERN long Nipc;
EXTERN struct IpcType *IPC;

/* Master Random Process */
EXTERN struct RandomProcessType *RNG;
EXTERN long RngSeed;

EXTERN double MapTime,JointTime,PathTime,PVelTime,FrcTrqTime;
EXTERN double AssembleTime,LockTime,TriangleTime,SubstTime,SolveTime;

EXTERN struct ConstellationType Constell[89];


long SimStep(void);
void Ephemerides(void);
void OrbitMotion(double Time);
void Environment(struct SCType *S);
void Perturbations(struct SCType *S);
void Sensors(struct SCType *S);
void SensorDriver(struct SCType *S);
void FlightSoftWare(struct SCType *S);
void ActuatorDriver(struct SCType *S);
void Actuators(struct SCType *S);
void CmdInterpreter(void);
void Report(void);
void DrawScene(void);
void ThreeBodyOrbitRK4(struct OrbitType *O);
void MotionConstraints(struct SCType *S);
void SCMassProps(struct SCType *S);
void MapJointStatesToStateVector(struct SCType *S);
void MapStateVectorToBodyStates(double *u, double *x, double *h, double *a,
   double *uf, double *xf, struct SCType *S);
void BodyStatesToNodeStates(struct SCType *S);
void PartitionForces(struct SCType *S);
void Dynamics(struct SCType *S);
void Cleanup(void);
void FindInterBodyDCMs(struct SCType *S);
void FindPathVectors(struct SCType *S);
void FindTotalAngMom(struct SCType *S);
double FindTotalKineticEnergy(struct SCType *S);
void UpdateScBoundingBox(struct SCType *S);
void FindUnshadedAreas(struct SCType *S, double DirVecN[3]);
void RadBelt(float RadiusKm, float MagLatDeg, int NumEnergies, 
      float *ElectronEnergy, float *ProtonEnergy, double **Flux); 
void InitAlbedo(void);
void FindCssAlbedo(struct SCType *S, struct CssType *CSS);
void FindFssAlbedo(struct SCType *S, struct FssType *FSS);
void JointFrcTrq(struct JointType *G, struct SCType *S);
void InitActuatedJoint(struct JointType *G, struct SCType *S);
void WheelJitter(struct WhlType *W, struct SCType *S);
void ShakerJitter(struct ShakerType *Sh, struct SCType *S);
long OpticalFieldPoint(double StarVecB[3], struct OpticsType *O, 
   double FldPntB[3], double FldDirB[3]);
long OpticalTrain(long FldSC, long FldBody, double FldPntB[3], double FldDirB[3], 
   long Nopt, struct OpticsType *Opt,
   long *OutSC, long *OutBody, double OutPntB[3], double OutDirB[3]);

/* Debug Function Prototypes */
void EchoPVel(struct SCType *S);
void EchoEOM(double **COEF, double *State, double *RHS, long Ns);
void EchoStates(int Nx, double *x, int Nu, double *u);
void EchoRemAcc(struct SCType *S);

void InitSim(int argc, char **argv);
void InitOrbits(void);
void InitSpacecraft(struct SCType *S);
void LoadPlanets(void);
long LoadJplEphems(char EphemPath[80],double JD);
long DecodeString(char *s);
void InitFSW(struct SCType *S);
void InitAC(struct SCType *S);
void InitLagrangePoints(void);

long LoadTRVfromFile(const char *Path, const char *TrvFileName,
   const char *ElemLabel, double DynTime, struct OrbitType *O);
void SplineToPosVel(struct OrbitType *O);

void CfdSlosh(struct SCType *S);
void FakeCfdSlosh(struct SCType *S);
void SendStatesToSpirent(void);

void NOS3Time(long *year, long *day_of_year, long *month, long *day,
              long *hour, long *minute, double *second);
                   
void InterProcessComm(void);
void InitInterProcessComm(void);

#undef EXTERN

/*
** #ifdef __cplusplus
** }
** #endif
*/





