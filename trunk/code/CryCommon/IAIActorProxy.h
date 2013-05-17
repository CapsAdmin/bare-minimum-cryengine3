#ifndef __IAIActorProxy_h__
#define __IAIActorProxy_h__

#pragma once


#include <IAIObject.h>


struct IAIActorProxyFactory
{
	virtual IAIActorProxy* CreateActorProxy(EntityId entityID) = 0;
	virtual ~IAIActorProxyFactory(){}
};


UNIQUE_IFACE struct IAIActorProxy :
public _reference_target_t
{
	// (MATT) These methods were merged from IUnknownProxy {2009/01/26}
	//----------------------------------------------------------------
	virtual int Update(SOBJECTSTATE &state, bool bFullUpdate) = 0;
	virtual bool CheckUpdateStatus() = 0;
	virtual void EnableUpdate(bool enable) = 0;
	virtual bool IsEnabled() const = 0;
	virtual int GetAlertnessState() const = 0;
	virtual bool IsCurrentBehaviorExclusive() const = 0;
	virtual bool SetCharacter( const char* character, const char* behaviour=NULL ) = 0;
	virtual const char* GetCharacter()  =0;
	virtual void QueryBodyInfo( SAIBodyInfo& bodyInfo ) = 0;
	virtual bool QueryBodyInfo( const SAIBodyInfoQuery& query, SAIBodyInfo& bodyInfo ) = 0;
	virtual	void QueryWeaponInfo( SAIWeaponInfo& weaponInfo ) = 0;
	virtual	EntityId GetLinkedDriverEntityId() = 0;
	virtual	bool IsDriver() = 0;
	virtual	EntityId GetLinkedVehicleEntityId() = 0;
	virtual bool GetLinkedVehicleVisionHelper(Vec3 &outHelperPos) const = 0;
	virtual void Reset(EObjectResetType type) = 0;
	virtual void Serialize( TSerialize ser ) = 0;

	virtual IAICommunicationHandler* GetCommunicationHandler() = 0;

	virtual bool BecomeAggressiveToAgent(EntityId agentID) = 0;

	virtual IPhysicalEntity* GetPhysics(bool wantCharacterPhysics=false) = 0;
	virtual void DebugDraw(int iParam = 0) = 0;
	virtual const char* GetCurrentReadibilityName() const = 0;

	// This will internally keep a counter to allow stacking of such commands
	virtual void SetForcedExecute(bool forced) = 0;
	virtual bool IsForcedExecute() const = 0;

	// Description:
	//	 Gets the corners of the tightest projected bounding rectangle in 2D world space coordinates.
	virtual void GetWorldBoundingRect(Vec3& FL, Vec3& FR, Vec3& BL, Vec3& BR, float extra=0) const = 0;
	virtual bool SetAGInput(EAIAGInput input, const char* value) = 0;
	virtual bool ResetAGInput(EAIAGInput input) = 0;
	virtual bool IsSignalAnimationPlayed( const char* value ) = 0;
	virtual bool IsActionAnimationStarted( const char* value ) = 0;
	virtual bool IsAnimationBlockingMovement() const = 0;
	virtual EActorTargetPhase GetActorTargetPhase() const = 0;
	virtual void PlayAnimationAction( const struct IAIAction* pAction, int goalPipeId ) = 0;
	virtual void AnimationActionDone( bool succeeded ) = 0;
	virtual bool IsPlayingSmartObjectAction() const = 0;

	// Description:
	//	 Returns the number of bullets shot since the last call to the method.
	virtual int GetAndResetShotBulletCount() = 0;

	virtual void	EnableWeaponListener(bool needsSignal) = 0;
	virtual void UpdateMind(SOBJECTSTATE &state) = 0;

	virtual bool IsDead() const = 0;
	virtual float GetActorHealth() const = 0;
	virtual float GetActorMaxHealth() const = 0;
	virtual int GetActorArmor() const = 0;
	virtual int GetActorMaxArmor() const = 0;
	virtual bool GetActorIsFallen() const = 0;

	virtual IWeapon *QueryCurrentWeapon(EntityId& currentWeaponId) = 0;
	virtual IWeapon *GetCurrentWeapon(EntityId& currentWeaponId) const = 0;
	virtual const AIWeaponDescriptor& GetCurrentWeaponDescriptor() const = 0;
	virtual IWeapon *GetSecWeapon( ERequestedGrenadeType prefGrenadeType=eRGT_ANY ) const = 0;
	virtual bool GetSecWeaponDescriptor(AIWeaponDescriptor &outDescriptor, ERequestedGrenadeType prefGrenadeType=eRGT_ANY) const = 0;

	// Description:
	//	 Sets if the AI should use the secondary weapon when gunning a vehicle
	virtual void SetUseSecondaryVehicleWeapon(bool bUseSecondary) = 0;
	virtual bool IsUsingSecondaryVehicleWeapon() const = 0;

	virtual IEntity* GetGrabbedEntity() const = 0;

	virtual const char* GetVoiceLibraryName(const bool useForcedDefaultName = false) const = 0;
	virtual const char* GetCommunicationConfigName() const = 0;
	virtual const char* GetBehaviorSelectionTreeName() const = 0;
	virtual const char* GetNavigationTypeName() const = 0;

	// Description:
	//	 Needed for debug drawing.
	virtual bool IsUpdateAlways() const = 0;
	virtual bool IfShouldUpdate() = 0;
	virtual bool IsAutoDeactivated() const = 0;
	virtual void NotifyAutoDeactivated() = 0;

	// Description:
	//	 Predict the flight path of a projectile launched with the primary weapon
	virtual bool PredictProjectileHit(float vel, Vec3& posOut, Vec3& dirOut, float& speedOut, Vec3* pTrajectory = 0, unsigned int* trajectorySizeInOut = 0) = 0;
	// Description:
	//   Predict the flight path of a grenade thrown (secondary weapon)
	virtual bool PredictProjectileHit(const Vec3& throwDir, float vel, Vec3& posOut, float& speedOut, ERequestedGrenadeType prefGrenadeType = eRGT_ANY, 
		Vec3* pTrajectory = 0, unsigned int* trajectorySizeInOut = 0) = 0;

	// Description:
	//	 Plays readability sound.
	virtual int PlayReadabilitySound(const char* szReadability, bool stopPreviousSound, float responseDelay = 0.0f) = 0;
	// Description:
	//	 Toggles readability testing on/off (called from a console command).
	virtual void TestReadabilityPack(bool start, const char* szReadability) = 0;
	// Description:
	//	 Returns the amount of time to block the readabilities of same name in a group.
	virtual void GetReadabilityBlockingParams(const char* text, float& time, int& id) = 0;
	// Description:
	//	 Informs CryAction Readability Manager about external system (like CryAISystem for example) playing
	//	 another readability.
	virtual void SetReadabilitySound(tSoundID nSoundId, bool bStopPrevious) = 0;

	virtual bool QueryCurrentAnimationSpeedRange(float& smin, float& smax) = 0;

	// Description:
	//	 Gets name of the running behavior.
	virtual const char* GetCurrentBehaviorName() const { return NULL; }

	// Description:
	//	 Gets name of the previous behavior.
	virtual const char* GetPreviousBehaviorName() const { return NULL; }

	virtual void UpdateMeAlways(bool doUpdateMeAlways) = 0;

	virtual void SetBehaviour(const char* szBehavior, const IAISignalExtraData* pData = 0) = 0;
};

#endif
