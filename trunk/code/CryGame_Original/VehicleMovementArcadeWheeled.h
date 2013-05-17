/*************************************************************************
Crytek Source File.
Copyright (C), Crytek Studios, 2001-2005.
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements wheel based vehicle movement

-------------------------------------------------------------------------
History:
- Created by stan fichele

*************************************************************************/
#ifndef __VEHICLEMOVEMENTARCADEWHEELED_H__
#define __VEHICLEMOVEMENTARCADEWHEELED_H__

#include <SharedParams/ISharedParams.h>

#include "Network/NetActionSync.h"
#include "IVehicleSystem.h"
#include "VehicleMovementBase.h"


class CVehicleMovementArcadeWheeled;
class CNetworkMovementArcadeWheeled
{
	public:
		CNetworkMovementArcadeWheeled();
		CNetworkMovementArcadeWheeled(CVehicleMovementArcadeWheeled *pMovement);

		typedef CVehicleMovementArcadeWheeled *UpdateObjectSink;

		bool operator==(const CNetworkMovementArcadeWheeled &rhs)
		{
			return (abs(m_pedal - rhs.m_pedal) < 0.001f) && (m_brake == rhs.m_brake) && (m_boost == rhs.m_boost);
		}

		bool operator!=(const CNetworkMovementArcadeWheeled &rhs)
		{
			return !this->operator==(rhs);
		}

		void UpdateObject(CVehicleMovementArcadeWheeled *pMovement);
		void Serialize(TSerialize ser, EEntityAspects aspects);

		static const NetworkAspectType CONTROLLED_ASPECT = eEA_GameClientDynamic;

	private:
		float m_pedal;	// Show white reverse movement lights
		bool m_brake;		// Show red brake lights
		bool m_boost;		// Show boost
};


/*
Wheel
*/
struct SVehicleWheel
{
	inline SVehicleWheel()
	{
		offset					.zero();
		frictionDir[0]	.zero();
		frictionDir[1]	.zero();
		worldOffset			.zero();
		contactNormal		.zero();
		wheelPart				= NULL;
		radius					= 0.f;
		mass						= 0.f;
		invMass					= 0.f;
		inertia					= 0.f;
		invInertia			= 0.f;
		bottomOffset		= 0.f;
		contact					= 0.f;
		lastW						= 0.f;
		w								= 0.f;
		slipSpeed				= 0.f;
		suspLen					= 0.f;
		compression			= 0.f;
		waterLevel			= WATER_LEVEL_UNKNOWN;
		axleIndex				= 0;
		bCanLock				= 0;
		locked					= false;
	}

	Vec3 offset;
	Vec3 frictionDir[2];			// [0] == inline friction direction, [1] == lateral friction direction
	Vec3 worldOffset;
	Vec3 contactNormal;

	IVehiclePart *wheelPart;

	float radius;
	float mass;
	float invMass;
	float inertia;
	float invInertia;
	float bottomOffset;				// offset + (zAxis * bottomOffset) == bottom point of wheel
	float contact;
	float lastW;
	float w;
	float slipSpeed;
	float suspLen;
	float compression;
	float	waterLevel;

	int axleIndex;						// 0 == back wheel, 1 == front wheel
	int bCanLock;							// 0 or 1

	bool locked;							// Handbraking
};


/*
Chassis
*/
struct SVehicleChassis
{
	float radius;
	float mass;
	float inertia;

	float invMass;
	float invInertia;

	Vec3 vel;
	Vec3 angVel;

	int contactIdx0, contactIdx1, contactIdx2, contactIdx3;		// These are used to calculate the base contact normal, and are indexes to the wheels that used

	bool collision1;
	bool collision2;
};

/*
Gears
*/
struct SVehicleGears
{
	enum { kMaxGears = 10 };

	enum { kReverse = 0, kNeutral, kFirst };

	float	averageWheelRadius;
	int		curGear;
	float	curRpm;
	int		accelerating;
	float	targetRpm;
	float	timer;
};

struct SSharedVehicleGears
{
	float	ratios[SVehicleGears::kMaxGears];
	float	invRatios[SVehicleGears::kMaxGears];
	int		numGears;
	float minChangeUpTime;
	float minChangeDownTime;
};

class CVehicleMovementArcadeWheeled
	: public CVehicleMovementBase
{
		friend class CNetworkMovementArcadeWheeled;
	public:
		enum {maxWheels = 10};
		enum {k_frictionNotSet, k_frictionUseLowLevel, k_frictionUseHiLevel}; // Friction State

	public:
		CVehicleMovementArcadeWheeled();
		~CVehicleMovementArcadeWheeled();

		// IVehicleMovement
		virtual bool Init(IVehicle *pVehicle, const CVehicleParams &table);
		virtual void PostInit();
		virtual void Reset();
		virtual void Release();
		virtual void Physicalize();
		virtual void PostPhysicalize();

		virtual EVehicleMovementType GetMovementType()
		{
			return eVMT_Land;
		}

		virtual bool StartEngine(EntityId driverId);
		virtual void StopEngine();
		virtual void OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams &params);
		virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value);

		virtual void ProcessAI(const float deltaTime);
		virtual void ProcessMovement(const float deltaTime);
		virtual void Update(const float deltaTime);
		virtual void UpdateSounds(const float deltaTime);

		virtual bool RequestMovement(CMovementRequest &movementRequest);
		virtual void GetMovementState(SMovementState &movementState);

		virtual pe_type GetPhysicalizationType() const
		{
			return PE_WHEELEDVEHICLE;
		};
		virtual bool UseDrivingProxy() const
		{
			return true;
		};
		virtual int GetWheelContacts() const
		{
			return m_wheelContacts;
		}
		virtual int GetBlownTires() const
		{
			return m_blownTires;
		}

		virtual void GetMemoryUsage(ICrySizer *pSizer) const;

		virtual void Serialize(TSerialize ser, EEntityAspects aspects);
		virtual void SetAuthority( bool auth )
		{
			m_netActionSync.CancelReceived();
		};
		// ~IVehicleMovement

		// IVehicleObject
		virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams &params);

	protected:

		virtual bool InitPhysics(const CVehicleParams &table);
		virtual void InitSurfaceEffects();

		virtual void UpdateSuspension(const float deltaTime);
		void UpdateSuspensionSound(const float deltaTime);
		void UpdateBrakes(const float deltaTime);

		virtual void UpdateSurfaceEffects(const float deltaTime);

		virtual void Boost(bool enable);
		virtual void ApplyBoost(float speed, float maxSpeed, float strength, float deltaTime);

		virtual bool DoGearSound();
		virtual float GetMinRPMSoundRatio()
		{
			return 0.f;
		}

#if ENABLE_VEHICLE_DEBUG
		virtual void DebugDrawMovement(const float deltaTime);
#endif

		float GetMaxSteer(float speedRel);
		float GetSteerSpeed(float speedRel);

		virtual float GetWheelCondition() const;
		void SetEngineRPMMult(float mult, int threadSafe = 0);

		float CalcSteering(float steer, float speedRel, float rotateYaw, float dt);
		void TickGears(float dt, float averageWheelSpeed, float throttle, float forwardSpeed);
		void EnableLowLevelPhysics(int state, int bThreadSafe);
		void InternalPhysicsTick(float dt);
		void GetCurrentWheelStatus(IPhysicalEntity *pPhysics);

#if ENABLE_VEHICLE_DEBUG
		void DebugCheat(float dt);
#endif

		void UpdateWaterLevels();
		void ResetWaterLevels();

	protected:

		pe_params_car m_carParams;
		pe_status_vehicle m_vehicleStatus;
		pe_action_drive m_action;

		float m_steerMax;	// max steering angle in deg
		float m_rpmTarget;
		float m_lastBump, m_compressionMax;
		float	m_rpmScalePrev;
		float m_brakeTimer;
		float m_reverseTimer;
		bool m_initialHandbreak;
		bool m_gearSoundPending;

		bool	m_stationaryHandbrake;
		float	m_stationaryHandbrakeResetTimer;

		float m_suspDamping;
		float m_stabi;
		float m_speedSuspUpdated;

		// Network related
		CNetActionSync<CNetworkMovementArcadeWheeled> m_netActionSync;

		float m_lostContactTimer;
		float m_tireBlownTimer;
		float m_forceSleepTimer;
		bool  m_bForceSleep;

		SVehicleGears m_gears;

		int8 m_wheelContacts;
		int8 m_passengerCount;
		int8 m_currentGear;
		int8 m_blownTires;
		int   m_lastDebugFrame;

		struct Handling
		{
			float  compressionScale;
			Vec3   contactNormal;
			float  lostContactOneSideTimer;
			float  viewRotate;
			Vec2   viewOffset;
		};

		struct SSharedHandling
		{
			float  acceleration, decceleration, topSpeed, reverseSpeed;
			float  reductionAmount, reductionRate;
			float  compressionBoost, compressionBoostHandBrake;
			float  backFriction, frontFriction, frictionOffset;
			float  grip1, grip2;       // Grip fraction at zero slip speed and grip fraction at high slip speed (usually 1.0f).
			float  gripK;              // 1.0f / slipSpeed.
			float  accelMultiplier1, accelMultiplier2;
			float  handBrakeDecceleration, handBrakeDeccelerationPowerLock;
			bool   handBrakeLockFront, handBrakeLockBack;
			float  handBrakeFrontFrictionScale, handBrakeBackFrictionScale;
			float  handBrakeAngCorrectionScale, handBrakeLateralCorrectionScale;
		};

		struct SPowerSlide
		{
			float lateralSpeedFraction[2];   // Percentage of forward speed to transfer as side speed (hack), [0] normal driving, [1] powerlock/handbrake turning
			float spring;                    // How quickly to apply that lerping
		};

		struct SStabilisation
		{
			float angDamping;          // Angular damping. Only affects tilt+roll but leaves the yaw speed unaffected
			float rollDamping;         // Roll damping. Only added when contact on one side is lost - reduces the chance of the vehicle rolling on its side
			float upDamping;           // This damps the *upwards* velocity of vehicle when ground contact is lost
			float rollfixAir;
			float sinMaxTiltAngleAir;  // Max forward tilt angle before rollfixAir is enforced
			float cosMaxTiltAngleAir;  // Max forward tilt angle before rollfixAir is enforced
		};

		struct Correction
		{
			float lateralSpring;
			float angSpring;
		};

		BEGIN_SHARED_PARAMS(SSharedParams)

		bool                 isBreakingOnIdle;
		float                steerSpeed, steerSpeedMin;     // Steer speed at vMaxSteerMax and steer speed at v = 0.
		float                kvSteerMax;                    // Reduce steer max at vMaxSteerMax.
		float                v0SteerMax;                    // Max steering angle in deg at v = 0.
		float                steerSpeedScaleMin;            // Scale for sens at zero vel.
		float                steerSpeedScale;               // Scale for sens at vMaxSteerMax.
		float                steerRelaxation;               // Relaxation speed to center in degrees.
		float                vMaxSteerMax;                  // Speed at which entire kvSteerMax is subtracted from v0SteerMax.
		float                pedalLimitMax;                 // At vMaxSteerMax pedal is clamped to 1 - pedalLimitMax.
		float                suspDampingMin, suspDampingMax, suspDampingMaxSpeed;
		float                stabiMin, stabiMax;
		float								rpmRelaxSpeed, rpmInterpSpeed, rpmGearShiftSpeed, airbrakeTime;
		float								bumpMinSusp, bumpMinSpeed, bumpIntensityMult;
		float                ackermanOffset;
		float                gravityInAirMult;
		SSharedVehicleGears  gears;
		SSharedHandling      handling;
		Correction           correction;
		SPowerSlide          powerSlide;
		SStabilisation       stabilisation;

		END_SHARED_PARAMS

		Handling m_handling;

		float m_damageRPMScale;

		SVehicleChassis m_chassis;

		typedef std::vector<SVehicleWheel> TWheelArray;
		typedef std::vector<pe_status_wheel> TPEStatusWheelArray;

		TWheelArray					m_wheels;
		TPEStatusWheelArray	m_wheelStatus;
		size_t							m_iWaterLevelUpdate;

		volatile int    m_wheelStatusLock;
		volatile int    m_frictionStateLock;
		int             m_numWheels;
		float           m_invNumWheels;

		float m_invTurningRadius;

		//------------------------------------------------------------------------------
		// AI related
		// PID controller for speed control.

		float	m_steering;

		CMovementRequest m_aiRequest;

		float m_submergedRatioMax;	// to avoid calling vehicle functions in ProcessMovement()

		int m_frictionState;

		SSharedParamsConstPtr	m_pSharedParams;
		float m_surfaceFriction[4]; // per wheel


	private:
		virtual void ProcessAI_Racing(const float deltaTime, float fCurrentSpeed, float fTargetSpeed, float degDesiredSteer);
};

#endif
