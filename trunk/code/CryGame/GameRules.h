/*************************************************************************
	Crytek Source File.
	Copyright (C), Crytek Studios, 2001-2004.
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description:

	-------------------------------------------------------------------------
	History:
	- 7:2:2006   15:38 : Created by Marcio Martins

*************************************************************************/
#ifndef __GAMERULES_H__
#define __GAMERULES_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "Game.h"
#include <IGameObject.h>
#include <IGameRulesSystem.h>
#include "Actor.h"
#include "SynchedStorage.h"
#include <queue>
#include "IViewSystem.h"

class CActor;
class CPlayer;

struct IGameObject;
struct IActorSystem;

class CRadio;
class CMPTutorial;




#define GAMERULES_INVOKE_ON_TEAM(team, rmi, params)	\
	{ \
		TPlayerTeamIdMap::const_iterator _team=m_playerteams.find(team); \
		if (_team!=m_playerteams.end()) \
		{ \
			const TPlayers &_players=_team->second; \
			for (TPlayers::const_iterator _player=_players.begin();_player!=_players.end(); ++_player) \
				GetGameObject()->InvokeRMI(rmi, params, eRMI_ToClientChannel, GetChannelId(*_player)); \
		} \
	} \
	 
#define GAMERULES_INVOKE_ON_TEAM_NOLOCAL(team, rmi, params)	\
	{ \
		TPlayerTeamIdMap::const_iterator _team=m_playerteams.find(team); \
		if (_team!=m_playerteams.end()) \
		{ \
			const TPlayers &_players=_team->second; \
			for (TPlayers::const_iterator _player=_players.begin();_player!=_players.end(); ++_player) \
				GetGameObject()->InvokeRMI(rmi, params, eRMI_ToClientChannel|eRMI_NoLocalCalls, GetChannelId(*_player)); \
		} \
	} \
	 

#define ACTOR_INVOKE_ON_TEAM(team, rmi, params)	\
	{ \
		TPlayerTeamIdMap::const_iterator _team=m_playerteams.find(team); \
		if (_team!=playerteams.end()) \
		{ \
			const TPlayers &_players=_team.second; \
			for (TPlayers::const_iterator _player=_players.begin();_player!=_players.end(); ++_player) \
			{ \
				CActor *pActor=GetActorByEntityId(*_player); \
				if (pActor) \
					pActor->GetGameObject()->InvokeRMI(rmi, params, eRMI_ToClientChannel, GetChannelId(*_player)); \
			} \
		} \
	} \
	 

#define ACTOR_INVOKE_ON_TEAM_NOLOCAL(team, rmi, params)	\
	{ \
		TPlayerTeamIdMap::const_iterator _team=m_playerteams.find(team); \
		if (_team!=playerteams.end()) \
		{ \
			const TPlayers &_players=_team.second; \
			for (TPlayers::const_iterator _player=_players.begin();_player!=_players.end(); ++_player) \
			{ \
				CActor *pActor=GetActorByEntityId(*_player); \
				if (pActor) \
					pActor->GetGameObject()->InvokeRMI(rmi, params, eRMI_ToClientChannel|eRMI_NoLocalCalls, GetChannelId(*_player)); \
			} \
		} \
	} \
	 
class IGameRulesClientConnectionListener
{
	public:
		virtual ~IGameRulesClientConnectionListener() {}

		virtual void OnClientConnect(int channelId, bool isReset, EntityId playerId) = 0;
		virtual void OnClientDisconnect(int channelId, EntityId playerId) = 0;
		virtual void OnClientEnteredGame(int channelId, bool isReset, EntityId playerId) = 0;
		virtual void OnOwnClientEnteredGame() = 0;
};

class CGameRules :	public CGameObjectExtensionHelper<CGameRules, IGameRules, 64>,
	public IActionListener,
	public IViewSystemListener,
	public IHostMigrationEventListener,
	public IEntityEventListener
{
	public:

		typedef std::vector<EntityId>								TPlayers;
		typedef std::vector<EntityId>								TSpawnLocations;
		typedef std::vector<EntityId>								TSpawnGroups;
		typedef std::map<EntityId, TSpawnLocations>	TSpawnGroupMap;
		typedef std::map<EntityId, int>							TBuildings;
		typedef std::map<EntityId, CTimeValue>			TFrozenEntities;
		typedef std::vector<EntityId>								TEntityIdVec;
		typedef std::set<CryUserID>									TCryUserIdSet;

		typedef struct SMinimapEntity
		{
			SMinimapEntity() {};
			SMinimapEntity(EntityId id, int typ, float time)
				: entityId(id),
				  type(typ),
				  lifetime(time)
			{
			}

			bool operator==(const SMinimapEntity &rhs)
			{
				return (entityId == rhs.entityId);
			}

			EntityId		entityId;
			int					type;
			float				lifetime;
		} SMinimapEntity;
		typedef std::vector<SMinimapEntity>				TMinimap;

		enum EMissionObjectiveState
		{
			eMOS_Deactivated,
			eMOS_Completed,
			eMOS_Failed,
			eMOS_Activated,
		};

		typedef struct TObjective
		{
			TObjective(): status(eMOS_Deactivated), entityId(0) {};
			TObjective(EMissionObjectiveState sts, EntityId eid): status(sts), entityId(eid) {};

			EMissionObjectiveState				status;
			EntityId	entityId;
		} TObjective;

		typedef std::map<string, TObjective> TObjectiveMap;
		typedef std::map<int, TObjectiveMap> TTeamObjectiveMap;

		struct SGameRulesListener
		{
			virtual ~SGameRulesListener() {}
			virtual void GameOver(int localWinner) = 0;
			virtual void EnteredGame() = 0;
			virtual void EndGameNear(EntityId id) = 0;
			virtual void ClientEnteredGame( EntityId clientId ) {}
			virtual void ClientDisconnect( EntityId clientId ) {}
		};
		typedef std::vector<SGameRulesListener *> TGameRulesListenerVec;

		typedef std::map<IEntity *, float> TExplosionAffectedEntities;

		// This structure contains the necessary information to create a new player
		// actor from a migrating one (a new player actor is created for each
		// reconnecting client and needs to be identical to the original actor on
		// the original server, or at least as close as possible)
		struct SMigratingPlayerInfo
		{
			CryFixedStringT<HOST_MIGRATION_MAX_PLAYER_NAME_SIZE>	m_originalName;
			Vec3					m_pos;
			Ang3					m_ori;
			EntityId			m_originalEntityId;
			int						m_team;
			float					m_health;
			TNetChannelID	m_channelID;
			bool					m_inUse;

			SMigratingPlayerInfo() : m_inUse(false), m_channelID(0) {}

			void SetChannelID(uint16 id)
			{
				assert(id > 0);
				m_channelID = id;
			}

			void SetData(const char *inOriginalName, EntityId inOriginalEntityId, int inTeam, const Vec3 &inPos, const Ang3 &inOri, float inHealth)
			{
				m_originalName = inOriginalName;
				m_originalEntityId = inOriginalEntityId;
				m_team = inTeam;
				m_pos = inPos;
				m_ori = inOri;
				m_health = inHealth;
				m_inUse = true;
			}

			void Reset()
			{
				m_inUse = false;
				m_channelID = 0;
			}

			bool InUse()
			{
				return m_inUse;
			}
		};

		struct SHostMigrationItemInfo
		{
			SHostMigrationItemInfo()
			{
				Reset();
			}

			void Reset()
			{
				m_inUse = false;
			}

			void Set(EntityId itemId, EntityId ownerId, bool isUsed, bool isSelected)
			{
				m_itemId = itemId;
				m_ownerId = ownerId;
				m_isUsed = isUsed;
				m_isSelected = isSelected;
				m_inUse = true;
			}

			EntityId m_itemId;
			EntityId m_ownerId;
			bool m_isUsed;
			bool m_isSelected;

			bool m_inUse;
		};

		struct SHostMigrationClientRequestParams
		{
			SHostMigrationClientRequestParams()
			{
				m_hasSentLoadout = false;
				m_timeToAutoRevive = 0.f;
			}

			void SerializeWith(TSerialize ser)
			{
				//m_loadoutParams.SerializeWith(ser);
				ser.Value("hasSentLoadout", m_hasSentLoadout, 'bool');
				ser.Value("timeToAutoRevive", m_timeToAutoRevive, 'fsec');
			}

			//CGameRules::EquipmentLoadoutParams m_loadoutParams;
			float m_timeToAutoRevive;
			bool m_hasSentLoadout;
		};

		struct SHostMigrationClientControlledParams
		{
			SHostMigrationClientControlledParams()
			{
				m_pAmmoParams = NULL;
				m_doneEnteredGame = false;
				m_doneSetAmmo = false;
				m_pHolsteredItemClass = NULL;
				m_pSelectedItemClass = NULL;
				m_hasValidVelocity = false;
				m_bInVisorMode = false;
			}

			~SHostMigrationClientControlledParams()
			{
				SAFE_DELETE_ARRAY(m_pAmmoParams);
			}

			bool IsDone()
			{
				return (m_doneEnteredGame && m_doneSetAmmo);
			}

			struct SAmmoParams
			{
				IEntityClass *m_pAmmoClass;
				int m_count;
			};

			Quat m_viewQuat;
			Vec3 m_position;		// Save this since the new server may not have it stored correctly (lag dependent)
			Vec3 m_velocity;
			Vec3 m_aimDirection;

			SAmmoParams *m_pAmmoParams;
			IEntityClass *m_pHolsteredItemClass;
			IEntityClass *m_pSelectedItemClass;

			int m_numAmmoParams;
			int m_numExpectedItems;

			bool m_hasValidVelocity;
			bool m_bInVisorMode;

			bool m_doneEnteredGame;
			bool m_doneSetAmmo;
		};

		struct SMidMigrationJoinParams
		{
			SMidMigrationJoinParams() : m_state(0), m_timeSinceStateChanged(0.f) {}
			SMidMigrationJoinParams(int state, float timeSinceStateChanged) : m_state(state), m_timeSinceStateChanged(timeSinceStateChanged) {}

			void SerializeWith(TSerialize ser)
			{
				ser.Value("state", m_state, 'ui2');
				ser.Value("timeSinceStateChanged", m_timeSinceStateChanged, 'fsec');
			}

			int m_state;
			float m_timeSinceStateChanged;
		};

		CGameRules();
		virtual ~CGameRules();
		//IGameObjectExtension
		virtual bool Init( IGameObject *pGameObject );
		virtual void PostInit( IGameObject *pGameObject );
		virtual void InitClient(int channelId);
		virtual void PostInitClient(int channelId);
		virtual bool ReloadExtension( IGameObject *pGameObject, const SEntitySpawnParams &params )
		{
			return false;
		}
		virtual void PostReloadExtension( IGameObject *pGameObject, const SEntitySpawnParams &params ) {}
		virtual bool GetEntityPoolSignature( TSerialize signature )
		{
			return false;
		}
		virtual void Release();
		virtual void FullSerialize( TSerialize ser );
		virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );
		virtual void PostSerialize();
		virtual void SerializeSpawnInfo( TSerialize ser ) {}
		virtual ISerializableInfoPtr GetSpawnInfo()
		{
			return 0;
		}
		virtual void Update( SEntityUpdateContext &ctx, int updateSlot );
		virtual void HandleEvent( const SGameObjectEvent & );
		virtual void ProcessEvent( SEntityEvent & );
		virtual void SetChannelId(uint16 id) {};
		virtual void SetAuthority( bool auth );
		virtual void PostUpdate( float frameTime );
		virtual void PostRemoteSpawn() {};
		virtual void GetMemoryUsage(ICrySizer *s) const;
		//~IGameObjectExtension

		// IViewSystemListener
		virtual bool OnBeginCutScene(IAnimSequence *pSeq, bool bResetFX);
		virtual bool OnEndCutScene(IAnimSequence *pSeq);
		virtual void OnPlayCutSceneSound(IAnimSequence *pSeq, ISound *pSound) {};
		virtual bool OnCameraChange(const SCameraParams &cameraParams)
		{
			return true;
		};
		// ~IViewSystemListener

		// IHostMigrationEventListener
		virtual bool OnInitiate(SHostMigrationInfo &hostMigrationInfo, uint32 &state);
		virtual bool OnDisconnectClient(SHostMigrationInfo &hostMigrationInfo, uint32 &state)
		{
			return true;
		}
		virtual bool OnDemoteToClient(SHostMigrationInfo &hostMigrationInfo, uint32 &state);
		virtual bool OnPromoteToServer(SHostMigrationInfo &hostMigrationInfo, uint32 &state);
		virtual bool OnReconnectClient(SHostMigrationInfo &hostMigrationInfo, uint32 &state);
		virtual bool OnFinalise(SHostMigrationInfo &hostMigrationInfo, uint32 &state);
		virtual bool OnTerminate(SHostMigrationInfo &hostMigrationInfo, uint32 &state)
		{
			return true;
		}
		virtual bool OnReset(SHostMigrationInfo &hostMigrationInfo, uint32 &state)
		{
			return true;
		}
		// ~IHostMigrationEventListener

		//IGameRules
		virtual bool ShouldKeepClient(int channelId, EDisconnectionCause cause, const char *desc) const;
		virtual void PrecacheLevel();
		virtual void PrecacheLevelResource(const char *resourceName, EGameResourceType resourceType) {};

		virtual XmlNodeRef FindPrecachedXmlFile(const char *sFilename)
		{
			return 0;
		}
		virtual void OnConnect(struct INetChannel *pNetChannel);
		virtual void OnDisconnect(EDisconnectionCause cause, const char *desc); // notification to the client that he has been disconnected

		virtual bool OnClientConnect(int channelId, bool isReset);
		virtual void OnClientDisconnect(int channelId, EDisconnectionCause cause, const char *desc, bool keepClient);
		virtual bool OnClientEnteredGame(int channelId, bool isReset);

		virtual void OnEntitySpawn(IEntity *pEntity);
		virtual void OnEntityRemoved(IEntity *pEntity);

		virtual void SendTextMessage(ETextMessageType type, const char *msg, uint32 to = eRMI_ToAllClients, int channelId = -1,
									 const char *p0 = 0, const char *p1 = 0, const char *p2 = 0, const char *p3 = 0);
		virtual void SendChatMessage(EChatMessageType type, EntityId sourceId, EntityId targetId, const char *msg);
		virtual bool CanReceiveChatMessage(EChatMessageType type, EntityId sourceId, EntityId targetId) const;

		virtual void ForbiddenAreaWarning(bool active, int timer, EntityId targetId);

		virtual void ResetGameTime();
		virtual float GetRemainingGameTime() const;
		virtual void SetRemainingGameTime(float seconds);

		virtual void ClearAllMigratingPlayers(void);
		virtual EntityId SetChannelForMigratingPlayer(const char *name, uint16 channelID);
		virtual void StoreMigratingPlayer(IActor *pActor);

		// Summary
		// Determines if a projectile spawned by the client is hitting a friendly AI
		virtual bool IsClientFriendlyProjectile(const EntityId projectileId, const EntityId targetEntityId) ;
		virtual bool IsTimeLimited() const;

		virtual void ResetRoundTime();
		virtual float GetRemainingRoundTime() const;
		virtual bool IsRoundTimeLimited() const;

		virtual void ResetPreRoundTime();
		virtual float GetRemainingPreRoundTime() const;

		virtual void ResetReviveCycleTime();
		virtual float GetRemainingReviveCycleTime() const;

		virtual void ResetGameStartTimer(float time = -1);
		virtual float GetRemainingStartTimer() const;

		virtual bool OnCollision(const SGameCollision &event);
		virtual void OnCollision_NotifyAI( const EventPhys *pEvent ) {}
		virtual void OnEntityReused(IEntity *pEntity, SEntitySpawnParams &params, EntityId prevId) {};
		//~IGameRules

		virtual void RegisterConsoleCommands(IConsole *pConsole);
		virtual void UnregisterConsoleCommands(IConsole *pConsole);
		virtual void RegisterConsoleVars(IConsole *pConsole);

		virtual void OnRevive(CActor *pActor, const Vec3 &pos, const Quat &rot, int teamId);
		virtual void OnKill(CActor *pActor, EntityId shooterId, const char *weaponClassName, int damage, int material, int hit_type);
		virtual void OnTextMessage(ETextMessageType type, const char *msg,
								   const char *p0 = 0, const char *p1 = 0, const char *p2 = 0, const char *p3 = 0);
		virtual void OnChatMessage(EChatMessageType type, EntityId sourceId, EntityId targetId, const char *msg, bool teamChatOnly);
		virtual void OnKillMessage(EntityId targetId, EntityId shooterId, const char *weaponClassName, float damage, int material, int hit_type);

		CActor *GetActorByChannelId(int channelId) const;
		bool IsRealActor(EntityId actorId) const;
		CActor *GetActorByEntityId(EntityId entityId) const;
		ILINE const char *GetActorNameByEntityId(EntityId entityId) const
		{
			CActor *pActor = GetActorByEntityId(entityId);

			if (pActor)
			{
				return pActor->GetEntity()->GetName();
			}

			return 0;
		}
		ILINE const char *GetActorName(CActor *pActor) const
		{
			return pActor->GetEntity()->GetName();
		};

		int GetChannelId(EntityId entityId) const;
		bool IsDead(EntityId entityId) const;
		bool IsSpectator(EntityId entityId) const;
		void EnableUpdateScores(bool show);
		void KnockActorDown( EntityId actorEntityId );

		//------------------------------------------------------------------------
		// player
		virtual CActor *SpawnPlayer(int channelId, const char *name, const char *className, const Vec3 &pos, const Ang3 &angles);
		virtual CActor *ChangePlayerClass(int channelId, const char *className);
		virtual void RevivePlayer(CActor *pActor, const Vec3 &pos, const Ang3 &angles, int teamId = 0, bool clearInventory = true);
		virtual void RenamePlayer(CActor *pActor, const char *name);
		virtual string VerifyName(const char *name, IEntity *pEntity = 0);
		virtual bool IsNameTaken(const char *name, IEntity *pEntity = 0);
		virtual void KillPlayer(IActor *pTarget, const bool inDropItem, const bool inDoRagdoll, const HitInfo &inHitInfo);
		virtual void MovePlayer(CActor *pActor, const Vec3 &pos, const Ang3 &angles);
		virtual void ChangeSpectatorMode(CActor *pActor, uint8 mode, EntityId target, bool resetAll);
		virtual void RequestNextSpectatorTarget(CActor *pActor, int change);
		virtual void ChangeTeam(CActor *pActor, int teamId);
		virtual void ChangeTeam(CActor *pActor, const char *teamName);
		//tagging time serialization limited to 0-60sec
		virtual void AddTaggedEntity(EntityId shooter, EntityId targetId, bool temporary = false, float time = 15.0f);
		virtual int GetPlayerCount(bool inGame = false) const;
		virtual int GetSpectatorCount(bool inGame = false) const;
		virtual EntityId GetPlayer(int idx);
		virtual void GetPlayers(TPlayers &players);
		virtual bool IsPlayerInGame(EntityId playerId) const;
		virtual bool IsPlayerActivelyPlaying(EntityId playerId) const;	// [playing / dead / waiting to respawn (inc spectating while dead): true] [not yet joined game / selected Spectate: false]
		virtual bool IsChannelInGame(int channelId) const;

		//------------------------------------------------------------------------
		// teams
		virtual int CreateTeam(const char *name);
		virtual void RemoveTeam(int teamId);
		virtual const char *GetTeamName(int teamId) const;
		virtual int GetTeamId(const char *name) const;
		virtual int GetTeamCount() const;
		virtual int GetTeamPlayerCount(int teamId, bool inGame = false) const;
		virtual int GetTeamChannelCount(int teamId, bool inGame = false) const;
		virtual EntityId GetTeamPlayer(int teamId, int idx);

		virtual void GetTeamPlayers(int teamId, TPlayers &players);

		virtual void SetTeam(int teamId, EntityId entityId);
		virtual int GetTeam(EntityId entityId) const;
		virtual int GetChannelTeam(int channelId) const;

		//------------------------------------------------------------------------
		// objectives
		virtual void AddObjective(int teamId, const char *objective, EMissionObjectiveState status, EntityId entityId);
		virtual void SetObjectiveStatus(int teamId, const char *objective, EMissionObjectiveState status);
		virtual EMissionObjectiveState GetObjectiveStatus(int teamId, const char *objective);
		virtual void SetObjectiveEntity(int teamId, const char *objective, EntityId entityId);
		virtual void RemoveObjective(int teamId, const char *objective);
		virtual void ResetObjectives();
		virtual TObjectiveMap *GetTeamObjectives(int teamId);
		virtual TObjective *GetObjective(int teamId, const char *objective);
		virtual void UpdateObjectivesForPlayer(int channelId, int teamId);

		//------------------------------------------------------------------------
		// materials
		virtual int RegisterHitMaterial(const char *materialName);
		virtual int GetHitMaterialId(const char *materialName) const;
		virtual ISurfaceType *GetHitMaterial(int id) const;
		virtual int GetHitMaterialIdFromSurfaceId(int surfaceId) const;
		virtual void ResetHitMaterials();

		//------------------------------------------------------------------------
		// hit type
		virtual int RegisterHitType(const char *type);
		virtual int GetHitTypeId(const char *type) const;
		virtual const char *GetHitType(int id) const;
		virtual void ResetHitTypes();

		//------------------------------------------------------------------------
		// freezing
		virtual bool IsFrozen(EntityId entityId) const;
		virtual void ResetFrozen();
		virtual void FreezeEntity(EntityId entityId, bool freeze, bool vapor, bool force = false);
		virtual void ShatterEntity(EntityId entityId, const Vec3 &pos, const Vec3 &impulse);

		//------------------------------------------------------------------------
		// spawn
		virtual void AddSpawnLocation(EntityId location);
		virtual void RemoveSpawnLocation(EntityId id);
		virtual int GetSpawnLocationCount() const;
		virtual EntityId GetSpawnLocation(int idx) const;
		virtual void GetSpawnLocations(TSpawnLocations &locations) const;
		virtual bool IsSpawnLocationSafe(EntityId playerId, EntityId spawnLocationId, float safeDistance, bool ignoreTeam, float zoffset) const;
		virtual bool IsSpawnLocationFarEnough(EntityId spawnLocationId, float minDistance, const Vec3 &testPosition) const;
		virtual bool TestSpawnLocationWithEnvironment(EntityId spawnLocationId, EntityId playerId, float offset = 0.0f, float height = 0.0f) const;
		virtual EntityId GetSpawnLocation(EntityId playerId, bool ignoreTeam, bool includeNeutral, EntityId groupId = 0, float minDistToDeath = 0.0f, const Vec3 &deathPos = Vec3(0, 0, 0), float *pZOffset = 0) const;
		virtual EntityId GetFirstSpawnLocation(int teamId = 0, EntityId groupId = 0) const;

		//------------------------------------------------------------------------
		// spawn groups
		virtual void AddSpawnGroup(EntityId groupId);
		virtual void AddSpawnLocationToSpawnGroup(EntityId groupId, EntityId location);
		virtual void RemoveSpawnLocationFromSpawnGroup(EntityId groupId, EntityId location);
		virtual void RemoveSpawnGroup(EntityId groupId);
		virtual EntityId GetSpawnLocationGroup(EntityId spawnId) const;
		virtual int GetSpawnGroupCount() const;
		virtual EntityId GetSpawnGroup(int idx) const;
		virtual void GetSpawnGroups(TSpawnLocations &groups) const;
		virtual bool IsSpawnGroup(EntityId id) const;

		virtual void RequestSpawnGroup(EntityId spawnGroupId);
		virtual void SetPlayerSpawnGroup(EntityId playerId, EntityId spawnGroupId);
		virtual EntityId GetPlayerSpawnGroup(CActor *pActor);

		virtual void SetTeamDefaultSpawnGroup(int teamId, EntityId spawnGroupId);
		virtual EntityId GetTeamDefaultSpawnGroup(int teamId);
		virtual void CheckSpawnGroupValidity(EntityId spawnGroupId);

		//------------------------------------------------------------------------
		// spectator
		virtual void AddSpectatorLocation(EntityId location);
		virtual void RemoveSpectatorLocation(EntityId id);
		virtual int GetSpectatorLocationCount() const;
		virtual EntityId GetSpectatorLocation(int idx) const;
		virtual void GetSpectatorLocations(TSpawnLocations &locations) const;
		virtual EntityId GetRandomSpectatorLocation() const;
		virtual EntityId GetInterestingSpectatorLocation() const;

		void CreateScriptHitInfo(SmartScriptTable &scriptHitInfo, const HitInfo &hitInfo);
		static void CreateHitInfoFromScript(const SmartScriptTable &scriptHitInfo, HitInfo &hitInfo);

		//------------------------------------------------------------------------
		// map
		virtual void ResetMinimap();
		virtual void UpdateMinimap(float frameTime);
		virtual void AddMinimapEntity(EntityId entityId, int type, float lifetime = 0.0f);
		virtual void RemoveMinimapEntity(EntityId entityId);
		virtual const TMinimap &GetMinimapEntities() const;

		//------------------------------------------------------------------------
		// game
		virtual void Restart();
		virtual void NextLevel();
		virtual void ResetEntities();
		virtual void OnEndGame();
		virtual void EnteredGame();
		virtual void UpdateScoreBoardItem(EntityId id, const string name, int kills, int deaths);
		virtual void GameOver(int localWinner);
		virtual void EndGameNear(EntityId id);
		void ClientDisconnect_NotifyListeners( EntityId clientId );
		void ClientEnteredGame_NotifyListeners( EntityId clientId );

		virtual void ClientSimpleHit(const SimpleHitInfo &simpleHitInfo);
		virtual void ServerSimpleHit(const SimpleHitInfo &simpleHitInfo);

		virtual void ClientHit(const HitInfo &hitInfo);
		virtual void ServerHit(const HitInfo &hitInfo);
		virtual void ProcessServerHit(const HitInfo &hitInfo);
		void ProcessLocalHit(const HitInfo &hitInfo, float fCausedDamage = 0.0f);

		void CullEntitiesInExplosion(const ExplosionInfo &explosionInfo);
		virtual void ServerExplosion(const ExplosionInfo &explosionInfo);
		virtual void ClientExplosion(const ExplosionInfo &explosionInfo);

		virtual void CreateEntityRespawnData(EntityId entityId);
		virtual bool HasEntityRespawnData(EntityId entityId) const;
		virtual void ScheduleEntityRespawn(EntityId entityId, bool unique, float timer);
		virtual void AbortEntityRespawn(EntityId entityId, bool destroyData);

		virtual void ScheduleEntityRemoval(EntityId entityId, float timer, bool visibility);
		virtual void AbortEntityRemoval(EntityId entityId);

		virtual void UpdateEntitySchedules(float frameTime);
		virtual void ProcessQueuedExplosions();
		virtual void ProcessServerExplosion(const ExplosionInfo &explosionInfo);

		virtual void ForceScoreboard(bool force);
		virtual void FreezeInput(bool freeze);

		virtual void ShowStatus();

		void SendRadioMessage(const EntityId sourceId, const int);
		void OnRadioMessage(const EntityId sourceId, const int);
		virtual void OnAction(const ActionId &actionId, int activationMode, float value);

		void ReconfigureVoiceGroups(EntityId id, int old_team, int new_team);

		int GetCurrentStateId() const
		{
			return m_currentStateId;
		}

		//misc
		// Next time CGameRules::OnCollision is called, it will skip this entity and return false
		// This will prevent squad mates to be hit by the player
		void SetEntityToIgnore(EntityId id)
		{
			m_ignoreEntityNextCollision = id;
		}

		template<typename T>
		void SetSynchedGlobalValue(TSynchedKey key, const T &value)
		{
			assert(gEnv->bServer);
			g_pGame->GetSynchedStorage()->SetGlobalValue(key, value);
		};

		template<typename T>
		bool GetSynchedGlobalValue(TSynchedKey key, T &value)
		{
			if (!g_pGame->GetSynchedStorage())
			{
				return false;
			}

			return g_pGame->GetSynchedStorage()->GetGlobalValue(key, value);
		}

		int GetSynchedGlobalValueType(TSynchedKey key) const
		{
			if (!g_pGame->GetSynchedStorage())
			{
				return eSVT_None;
			}

			return g_pGame->GetSynchedStorage()->GetGlobalValueType(key);
		}

		template<typename T>
		void SetSynchedEntityValue(EntityId id, TSynchedKey key, const T &value)
		{
			assert(gEnv->bServer);
			g_pGame->GetSynchedStorage()->SetEntityValue(id, key, value);
		}
		template<typename T>
		bool GetSynchedEntityValue(EntityId id, TSynchedKey key, T &value)
		{
			return g_pGame->GetSynchedStorage()->GetEntityValue(id, key, value);
		}

		int GetSynchedEntityValueType(EntityId id, TSynchedKey key) const
		{
			return g_pGame->GetSynchedStorage()->GetEntityValueType(id, key);
		}

		void ResetSynchedStorage()
		{
			g_pGame->GetSynchedStorage()->Reset();
		}

		void ForceSynchedStorageSynch(int channel);


		void PlayerPosForRespawn(CPlayer *pPlayer, bool save);
		void SPNotifyPlayerKill(EntityId targetId, EntityId weaponId, bool bHeadShot);

		string GetPlayerName(int channelId, bool bVerifyName = false);

		struct ChatMessageParams
		{
			uint8 type;
			EntityId sourceId;
			EntityId targetId;
			string msg;
			bool onlyTeam;

			ChatMessageParams() {};
			ChatMessageParams(EChatMessageType _type, EntityId src, EntityId trg, const char *_msg, bool _onlyTeam)
				: type(_type),
				  sourceId(src),
				  targetId(trg),
				  msg(_msg),
				  onlyTeam(_onlyTeam)
			{
			}

			void SerializeWith(TSerialize ser)
			{
				ser.Value("type", type, 'ui3');
				ser.Value("source", sourceId, 'eid');

				if (type == eChatToTarget)
				{
					ser.Value("target", targetId, 'eid');
				}

				ser.Value("message", msg);
				ser.Value("onlyTeam", onlyTeam, 'bool');
			}
		};

		struct ForbiddenAreaWarningParams
		{
			int timer;
			bool active;
			ForbiddenAreaWarningParams() {};
			ForbiddenAreaWarningParams(bool act, int time) : active(act), timer(time)
			{}

			void SerializeWith(TSerialize ser)
			{
				ser.Value("active", active, 'bool');
				ser.Value("timer", timer, 'ui5');
			}
		};

		struct BoolParam
		{
			bool success;
			void SerializeWith(TSerialize ser)
			{
				ser.Value("success", success, 'bool');
			}
		};

		struct RadioMessageParams
		{
			EntityId			sourceId;
			uint8					msg;

			RadioMessageParams() {};
			RadioMessageParams(EntityId src, int _msg):
				sourceId(src),
				msg(_msg)
			{
			};
			void SerializeWith(TSerialize ser);
		};
		struct TextMessageParams
		{
			uint8	type;
			string msg;

			uint8 nparams;
			string params[4];

			TextMessageParams() {};
			TextMessageParams(ETextMessageType _type, const char *_msg)
				: type(_type),
				  msg(_msg),
				  nparams(0)
			{
			};
			TextMessageParams(ETextMessageType _type, const char *_msg,
							  const char *p0 = 0, const char *p1 = 0, const char *p2 = 0, const char *p3 = 0)
				: type(_type),
				  msg(_msg),
				  nparams(0)
			{
				if (!AddParam(p0))
				{
					return;
				}

				if (!AddParam(p1))
				{
					return;
				}

				if (!AddParam(p2))
				{
					return;
				}

				if (!AddParam(p3))
				{
					return;
				}
			}

			void SerializeWith(TSerialize ser)
			{
				ser.Value("type", type, 'ui3');
				ser.Value("message", msg);
				ser.Value("nparams", nparams, 'ui3');

				for (int i = 0; i < nparams; ++i)
				{
					ser.Value("param", params[i]);
				}
			}

			bool AddParam(const char *param)
			{
				if (!param || nparams > 3)
				{
					return false;
				}

				params[nparams++] = param;
				return true;
			}
		};

		struct SetTeamParams
		{
			int				teamId;
			EntityId	entityId;

			SetTeamParams() {};
			SetTeamParams(EntityId _entityId, int _teamId)
				: entityId(_entityId),
				  teamId(_teamId)
			{
			}

			void SerializeWith(TSerialize ser)
			{
				ser.Value("entityId", entityId, 'eid');
				ser.Value("teamId", teamId, 'team');
			}
		};

		struct ChangeTeamParams
		{
			EntityId	entityId;
			int				teamId;

			ChangeTeamParams() {};
			ChangeTeamParams(EntityId _entityId, int _teamId)
				: entityId(_entityId),
				  teamId(_teamId)
			{
			}

			void SerializeWith(TSerialize ser)
			{
				ser.Value("entityId", entityId, 'eid');
				ser.Value("teamId", teamId, 'team');
			}
		};

		struct SpectatorModeParams
		{
			EntityId	entityId;
			uint8			mode;
			EntityId	targetId;
			bool			resetAll;

			SpectatorModeParams() {};
			SpectatorModeParams(EntityId _entityId, uint8 _mode, EntityId _target, bool _reset)
				: entityId(_entityId),
				  mode(_mode),
				  targetId(_target),
				  resetAll(_reset)
			{
			}

			void SerializeWith(TSerialize ser)
			{
				ser.Value("entityId", entityId, 'eid');
				ser.Value("mode", mode, 'ui3');
				ser.Value("targetId", targetId, 'eid');
				ser.Value("resetAll", resetAll, 'bool');
			}
		};

		struct RenameEntityParams
		{
			EntityId	entityId;
			string		name;

			RenameEntityParams() {};
			RenameEntityParams(EntityId _entityId, const char *name)
				: entityId(_entityId),
				  name(name)
			{
			}

			void SerializeWith(TSerialize ser)
			{
				ser.Value("entityId", entityId, 'eid');
				ser.Value("name", name);
			}
		};

		struct SetGameTimeParams
		{
			CTimeValue endTime;

			SetGameTimeParams() {};
			SetGameTimeParams(CTimeValue _endTime)
				: endTime(_endTime)
			{
			}

			void SerializeWith(TSerialize ser)
			{
				ser.Value("endTime", endTime);
			}
		};

		struct AddMinimapEntityParams
		{
			EntityId entityId;
			float	lifetime;
			int	type;
			AddMinimapEntityParams() {};
			AddMinimapEntityParams(EntityId entId, float ltime, int typ)
				: entityId(entId),
				  lifetime(ltime),
				  type(typ)
			{
			}

			void SerializeWith(TSerialize ser)
			{
				ser.Value("entityId", entityId, 'eid');
				ser.Value("lifetime", lifetime, 'fsec');
				ser.Value("type", type, 'i8');
			}
		};

		struct EntityParams
		{
			EntityId entityId;
			EntityParams() {};
			EntityParams(EntityId entId)
				: entityId(entId)
			{
			}

			void SerializeWith(TSerialize ser)
			{
				ser.Value("entityId", entityId, 'eid');
			}
		};

		struct NoParams
		{
			NoParams() {};
			void SerializeWith(TSerialize ser) {};
		};


		struct SpawnGroupParams
		{
			EntityId entityId;
			SpawnGroupParams() {};
			SpawnGroupParams(EntityId entId)
				: entityId(entId)
			{
			}

			void SerializeWith(TSerialize ser)
			{
				ser.Value("entityId", entityId, 'eid');
			}
		};

		struct SetObjectiveParams
		{
			SetObjectiveParams(): status(0), entityId(0) {};
			SetObjectiveParams(const char *nm, int st, EntityId id): name(nm), status(st), entityId(id) {};

			EntityId entityId;
			int status;
			string name;
			void SerializeWith(TSerialize ser)
			{
				ser.Value("name", name);
				ser.Value("status", status, 'hSts');
				ser.Value("entityId", entityId, 'eid');
			}
		};

		struct TempRadarTaggingParams
		{
			EntityId  entityId;
			float			m_time;
			TempRadarTaggingParams() : entityId(0), m_time(0.0f) {};
			TempRadarTaggingParams(EntityId entId, float time = 15.0f)
				: entityId(entId), m_time(time)
			{
			}

			void SerializeWith(TSerialize ser)
			{
				ser.Value("entityId", entityId, 'eid');
				ser.Value("time", m_time, 'tm63');
			}
		};

		struct SetObjectiveStatusParams
		{
			SetObjectiveStatusParams(): status( eMOS_Deactivated ) {};
			SetObjectiveStatusParams(const char *nm, EMissionObjectiveState st): name(nm), status((int) st) {};
			int status;
			string name;
			void SerializeWith(TSerialize ser)
			{
				ser.Value("name", name);
				ser.Value("status", status, 'hSts');
			}
		};

		struct SetObjectiveEntityParams
		{
			SetObjectiveEntityParams(): entityId(0) {};
			SetObjectiveEntityParams(const char *nm, EntityId id): name(nm), entityId(id) {};

			EntityId entityId;
			string name;
			void SerializeWith(TSerialize ser)
			{
				ser.Value("name", name);
				ser.Value("entityId", entityId, 'eid');
			}
		};

		struct RemoveObjectiveParams
		{
			RemoveObjectiveParams() {};
			RemoveObjectiveParams(const char *nm): name(nm) {};

			string name;
			void SerializeWith(TSerialize ser)
			{
				ser.Value("name", name);
			}
		};

		struct FreezeEntityParams
		{
			FreezeEntityParams() {};
			FreezeEntityParams(EntityId id, bool doit, bool smoke): entityId(id), freeze(doit), vapor(smoke) {};
			EntityId entityId;
			bool freeze;
			bool vapor;

			void SerializeWith(TSerialize ser)
			{
				ser.Value("entityId", entityId, 'eid');
				ser.Value("freeze", freeze, 'bool');
				ser.Value("vapor", vapor, 'bool');
			}
		};

		struct ShatterEntityParams
		{
			ShatterEntityParams() {};
			ShatterEntityParams(EntityId id, const Vec3 &p, const Vec3 &imp): entityId(id), pos(p), impulse(imp) {};
			EntityId entityId;
			Vec3 pos;
			Vec3 impulse;

			void SerializeWith(TSerialize ser)
			{
				ser.Value("entityId", entityId, 'eid');
				ser.Value("pos", pos, 'sHit');
				ser.Value("impulse", impulse, 'vHPs'); // temp policy
			}
		};

		struct DamageIndicatorParams
		{
			DamageIndicatorParams() {};
			DamageIndicatorParams(EntityId shtId, EntityId wpnId): shooterId(shtId), weaponId(wpnId) {};

			EntityId shooterId;
			EntityId weaponId;

			void SerializeWith(TSerialize ser)
			{
				ser.Value("shooterId", shooterId, 'eid');
				ser.Value("weaponId", weaponId, 'eid');
			}
		};

		DECLARE_SERVER_RMI_NOATTACH(SvRequestSimpleHit, SimpleHitInfo, eNRT_ReliableUnordered);
		DECLARE_SERVER_RMI_NOATTACH(SvRequestHit, HitInfo, eNRT_ReliableUnordered);
		DECLARE_CLIENT_RMI_NOATTACH(ClExplosion, ExplosionInfo, eNRT_ReliableUnordered);
		DECLARE_CLIENT_RMI_NOATTACH(ClFreezeEntity, FreezeEntityParams, eNRT_ReliableUnordered);
		DECLARE_CLIENT_RMI_NOATTACH(ClShatterEntity, ShatterEntityParams, eNRT_ReliableUnordered);

		DECLARE_SERVER_RMI_NOATTACH(SvRequestChatMessage, ChatMessageParams, eNRT_ReliableUnordered);
		DECLARE_CLIENT_RMI_NOATTACH(ClChatMessage, ChatMessageParams, eNRT_ReliableUnordered);

		DECLARE_SERVER_RMI_NOATTACH(SvRequestRadioMessage, RadioMessageParams, eNRT_ReliableUnordered);
		DECLARE_CLIENT_RMI_NOATTACH(ClRadioMessage, RadioMessageParams, eNRT_ReliableUnordered);

		DECLARE_CLIENT_RMI_NOATTACH(ClTaggedEntity, EntityParams, eNRT_ReliableUnordered);
		DECLARE_CLIENT_RMI_NOATTACH(ClTempRadarEntity, TempRadarTaggingParams, eNRT_ReliableUnordered);
		DECLARE_CLIENT_RMI_NOATTACH(ClBoughtItem, BoolParam, eNRT_ReliableUnordered);

		DECLARE_SERVER_RMI_NOATTACH(SvRequestRename, RenameEntityParams, eNRT_ReliableOrdered);
		DECLARE_CLIENT_RMI_NOATTACH(ClRenameEntity, RenameEntityParams, eNRT_ReliableOrdered);

		DECLARE_SERVER_RMI_NOATTACH(SvRequestChangeTeam, ChangeTeamParams, eNRT_ReliableOrdered);
		DECLARE_SERVER_RMI_NOATTACH(SvRequestSpectatorMode, SpectatorModeParams, eNRT_ReliableOrdered);
		DECLARE_CLIENT_RMI_NOATTACH(ClSetTeam, SetTeamParams, eNRT_ReliableOrdered);
		DECLARE_CLIENT_RMI_NOATTACH(ClTextMessage, TextMessageParams, eNRT_ReliableUnordered);

		DECLARE_CLIENT_RMI_NOATTACH(ClAddSpawnGroup, SpawnGroupParams, eNRT_ReliableOrdered);
		DECLARE_CLIENT_RMI_NOATTACH(ClRemoveSpawnGroup, SpawnGroupParams, eNRT_ReliableOrdered);

		DECLARE_CLIENT_RMI_NOATTACH(ClAddMinimapEntity, AddMinimapEntityParams, eNRT_ReliableOrdered);
		DECLARE_CLIENT_RMI_NOATTACH(ClRemoveMinimapEntity, EntityParams, eNRT_ReliableOrdered);
		DECLARE_CLIENT_RMI_NOATTACH(ClResetMinimap, NoParams, eNRT_ReliableOrdered);

		DECLARE_CLIENT_RMI_NOATTACH(ClSetObjective, SetObjectiveParams, eNRT_ReliableOrdered);
		DECLARE_CLIENT_RMI_NOATTACH(ClSetObjectiveStatus, SetObjectiveStatusParams, eNRT_ReliableOrdered);
		DECLARE_CLIENT_RMI_NOATTACH(ClSetObjectiveEntity, SetObjectiveEntityParams, eNRT_ReliableOrdered);
		DECLARE_CLIENT_RMI_NOATTACH(ClResetObjectives, NoParams, eNRT_ReliableOrdered);

		DECLARE_CLIENT_RMI_NOATTACH(ClHitIndicator, NoParams, eNRT_UnreliableUnordered);
		DECLARE_CLIENT_RMI_NOATTACH(ClDamageIndicator, DamageIndicatorParams, eNRT_UnreliableUnordered);

		DECLARE_CLIENT_RMI_NOATTACH(ClForbiddenAreaWarning, ForbiddenAreaWarningParams, eNRT_ReliableOrdered); // needs to be ordered to respect enter->leave->enter transitions

		DECLARE_CLIENT_RMI_NOATTACH(ClSetGameTime, SetGameTimeParams, eNRT_ReliableUnordered);
		DECLARE_CLIENT_RMI_NOATTACH(ClSetRoundTime, SetGameTimeParams, eNRT_ReliableUnordered);
		DECLARE_CLIENT_RMI_NOATTACH(ClSetPreRoundTime, SetGameTimeParams, eNRT_ReliableUnordered);
		DECLARE_CLIENT_RMI_NOATTACH(ClSetReviveCycleTime, SetGameTimeParams, eNRT_ReliableUnordered);
		DECLARE_CLIENT_RMI_NOATTACH(ClSetGameStartTimer, SetGameTimeParams, eNRT_ReliableUnordered);

		DECLARE_SERVER_RMI_NOATTACH(SvVote, NoParams, eNRT_ReliableUnordered);
		DECLARE_SERVER_RMI_NOATTACH(SvVoteNo, NoParams, eNRT_ReliableUnordered);

		DECLARE_CLIENT_RMI_NOATTACH(ClEnteredGame, NoParams, eNRT_ReliableUnordered);

		DECLARE_CLIENT_RMI_NOATTACH(ClPlayerJoined, RenameEntityParams, eNRT_ReliableUnordered);
		DECLARE_CLIENT_RMI_NOATTACH(ClPlayerLeft, RenameEntityParams, eNRT_ReliableUnordered);

		DECLARE_SERVER_RMI_NOATTACH(SvHostMigrationRequestSetup, SHostMigrationClientRequestParams, eNRT_ReliableUnordered);
		DECLARE_CLIENT_RMI_NOATTACH(ClHostMigrationFinished, NoParams, eNRT_ReliableOrdered);
		DECLARE_CLIENT_RMI_NOATTACH(ClMidMigrationJoin, SMidMigrationJoinParams, eNRT_ReliableOrdered);
		DECLARE_CLIENT_RMI_NOATTACH(ClHostMigrationPlayerJoined, EntityParams, eNRT_ReliableOrdered);

		virtual void AddGameRulesListener(SGameRulesListener *pRulesListener);
		virtual void RemoveGameRulesListener(SGameRulesListener *pRulesListener);

		void OnHostMigrationGotLocalPlayer(CPlayer *pPlayer);
		void OnHostMigrationStateChanged();
		int GetMigratingPlayerIndex(TNetChannelID channelID);
		void FinishMigrationForPlayer(int migratingIndex);
		void FakeDisconnectPlayer(EntityId playerId);

		void HostMigrationFindDynamicEntities(TEntityIdVec &results);
		void HostMigrationRemoveDuplicateDynamicEntities();

		ILINE void	HostMigrationStopAddingPlayers()
		{
			m_bBlockPlayerAddition = true;
		}
		void	HostMigrationResumeAddingPlayers()
		{
			m_bBlockPlayerAddition  = false;
		}

		void OnUserLeftLobby( int channelId );

		virtual void AddHitListener(IHitListener *pHitListener);
		virtual void RemoveHitListener(IHitListener *pHitListener);

		// IEntityEventListener
		virtual void OnEntityEvent( IEntity *pEntity, SEntityEvent &event );
		// ~IEntityEventListener

		typedef std::map<int, EntityId>				TTeamIdEntityIdMap;
		typedef std::map<EntityId, int>				TEntityTeamIdMap;
		typedef std::map<int, TPlayers>				TPlayerTeamIdMap;
		typedef std::map<int, EntityId>				TChannelTeamIdMap;
		typedef std::map<string, int>					TTeamIdMap;

		typedef std::map<int, int>						THitMaterialMap;
		typedef std::map<int, string>					THitTypeMap;

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
		typedef std::map<int, _smart_ptr<IVoiceGroup> >		TTeamIdVoiceGroupMap;
#endif

		typedef struct SEntityRespawnData
		{
			SmartScriptTable	properties;
			Vec3							position;
			Quat							rotation;
			Vec3							scale;
			int								flags;
			IEntityClass			*pClass;

			EntityId					m_currentEntityId;
			bool							m_bHasRespawned;

#ifdef _DEBUG
			string						name;
#endif
		} SEntityRespawnData;

		typedef struct SEntityRespawn
		{
			bool							unique;
			float							timer;
		} SEntityRespawn;

		typedef struct SEntityRemovalData
		{
			float							timer;
			float							time;
			bool							visibility;
		} SEntityRemovalData;

		typedef std::map<EntityId, SEntityRespawnData>	TEntityRespawnDataMap;
		typedef std::map<EntityId, SEntityRespawn>			TEntityRespawnMap;
		typedef std::map<EntityId, SEntityRemovalData>	TEntityRemovalMap;

		typedef std::vector<IHitListener *> THitListenerVec;

	protected:
		static void CmdDebugSpawns(IConsoleCmdArgs *pArgs);
		static void CmdDebugMinimap(IConsoleCmdArgs *pArgs);
		static void CmdDebugTeams(IConsoleCmdArgs *pArgs);
		static void CmdDebugObjectives(IConsoleCmdArgs *pArgs);

		void CreateScriptExplosionInfo(SmartScriptTable &scriptExplosionInfo, const ExplosionInfo &explosionInfo);
		void UpdateAffectedEntitiesSet(TExplosionAffectedEntities &affectedEnts, const pe_explosion *pExplosion);
		void AddOrUpdateAffectedEntity(TExplosionAffectedEntities &affectedEnts, IEntity *pEntity, float affected);
		void CommitAffectedEntitiesSet(SmartScriptTable &scriptExplosionInfo, TExplosionAffectedEntities &affectedEnts);
		void ChatLog(EChatMessageType type, EntityId sourceId, EntityId targetId, const char *msg);

		// Some explosion processing
		void ProcessClientExplosionScreenFX(const ExplosionInfo &explosionInfo);
		void ProcessExplosionMaterialFX(const ExplosionInfo &explosionInfo);

		// fill source/target dependent params in m_collisionTable
		void PrepCollision(int src, int trg, const SGameCollision &event, IEntity *pTarget);

		void CallOnForbiddenAreas(const char *pFuncName) {}

		void AddEntityEventDoneListener(EntityId id);
		void RemoveEntityEventDoneListener(EntityId id);
		void ClearRemoveEntityEventListeners();

		void CallScript(IScriptTable *pScript, const char *name)
		{
			if (!pScript || pScript->GetValueType(name) != svtFunction)
			{
				return;
			}

			m_pScriptSystem->BeginCall(pScript, name);
			m_pScriptSystem->PushFuncParam(m_script);
			m_pScriptSystem->EndCall();
		};
		template<typename P1>
		void CallScript(IScriptTable *pScript, const char *name, const P1 &p1)
		{
			if (!pScript || pScript->GetValueType(name) != svtFunction)
			{
				return;
			}

			m_pScriptSystem->BeginCall(pScript, name);
			m_pScriptSystem->PushFuncParam(m_script);
			m_pScriptSystem->PushFuncParam(p1);
			m_pScriptSystem->EndCall();
		};
		template<typename P1, typename P2>
		void CallScript(IScriptTable *pScript, const char *name, const P1 &p1, const P2 &p2)
		{
			if (!pScript || pScript->GetValueType(name) != svtFunction)
			{
				return;
			}

			m_pScriptSystem->BeginCall(pScript, name);
			m_pScriptSystem->PushFuncParam(m_script);
			m_pScriptSystem->PushFuncParam(p1);
			m_pScriptSystem->PushFuncParam(p2);
			m_pScriptSystem->EndCall();
		};
		template<typename P1, typename P2, typename P3>
		void CallScript(IScriptTable *pScript, const char *name, const P1 &p1, const P2 &p2, const P3 &p3)
		{
			if (!pScript || pScript->GetValueType(name) != svtFunction)
			{
				return;
			}

			m_pScriptSystem->BeginCall(pScript, name);
			m_pScriptSystem->PushFuncParam(m_script);
			m_pScriptSystem->PushFuncParam(p1);
			m_pScriptSystem->PushFuncParam(p2);
			m_pScriptSystem->PushFuncParam(p3);
			m_pScriptSystem->EndCall();
		};
		template<typename P1, typename P2, typename P3, typename P4>
		void CallScript(IScriptTable *pScript, const char *name, const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4)
		{
			if (!pScript || pScript->GetValueType(name) != svtFunction)
			{
				return;
			}

			m_pScriptSystem->BeginCall(pScript, name);
			m_pScriptSystem->PushFuncParam(m_script);
			m_pScriptSystem->PushFuncParam(p1);
			m_pScriptSystem->PushFuncParam(p2);
			m_pScriptSystem->PushFuncParam(p3);
			m_pScriptSystem->PushFuncParam(p4);
			m_pScriptSystem->EndCall();
		};
		template<typename P1, typename P2, typename P3, typename P4, typename P5>
		void CallScript(IScriptTable *pScript, const char *name, const P1 &p1, const P2 &p2, const P3 &p3, const P4 &p4, const P5 &p5)
		{
			if (!pScript || pScript->GetValueType(name) != svtFunction)
			{
				return;
			}

			m_pScriptSystem->BeginCall(pScript, name);
			m_pScriptSystem->PushFuncParam(m_script);
			m_pScriptSystem->PushFuncParam(p1);
			m_pScriptSystem->PushFuncParam(p2);
			m_pScriptSystem->PushFuncParam(p3);
			m_pScriptSystem->PushFuncParam(p4);
			m_pScriptSystem->PushFuncParam(p5);
			m_pScriptSystem->EndCall();
		};
		template<typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
		void CallScript(IScriptTable *pScript, const char *name, P1 &p1, P2 &p2, P3 &p3, P4 &p4, P5 &p5, P6 &p6)
		{
			if (!pScript || pScript->GetValueType(name) != svtFunction)
			{
				return;
			}

			m_pScriptSystem->BeginCall(pScript, name);
			m_pScriptSystem->PushFuncParam(m_script);
			m_pScriptSystem->PushFuncParam(p1);
			m_pScriptSystem->PushFuncParam(p2);
			m_pScriptSystem->PushFuncParam(p3);
			m_pScriptSystem->PushFuncParam(p4);
			m_pScriptSystem->PushFuncParam(p5);
			m_pScriptSystem->PushFuncParam(p6);
			m_pScriptSystem->EndCall();
		};

		IGameFramework			*m_pGameFramework;
		IGameplayRecorder		*m_pGameplayRecorder;
		ISystem							*m_pSystem;
		IActorSystem				*m_pActorSystem;
		IEntitySystem				*m_pEntitySystem;
		IScriptSystem				*m_pScriptSystem;
		IMaterialManager		*m_pMaterialManager;
		SmartScriptTable		m_script;
		SmartScriptTable		m_clientScript;
		SmartScriptTable		m_serverScript;
		SmartScriptTable		m_clientStateScript;
		SmartScriptTable		m_serverStateScript;
		HSCRIPTFUNCTION			m_onCollisionFunc;
		SmartScriptTable		m_collisionTable;
		SmartScriptTable		m_collisionTableSource;
		SmartScriptTable		m_collisionTableTarget;

		INetChannel					*m_pClientNetChannel;

		std::vector<int>		m_channelIds;
		TFrozenEntities			m_frozen;

		TTeamIdMap					m_teams;
		TEntityTeamIdMap		m_entityteams;
		TTeamIdEntityIdMap	m_teamdefaultspawns;
		TPlayerTeamIdMap		m_playerteams;
		TChannelTeamIdMap		m_channelteams;
		int									m_teamIdGen;

		THitMaterialMap			m_hitMaterials;
		int									m_hitMaterialIdGen;

		THitTypeMap					m_hitTypes;
		int									m_hitTypeIdGen;

		SmartScriptTable		m_scriptHitInfo;
		SmartScriptTable		m_scriptExplosionInfo;

		typedef std::queue<ExplosionInfo> TExplosionQueue;
		TExplosionQueue     m_queuedExplosions;

		typedef std::queue<HitInfo> THitQueue;
		THitQueue						m_queuedHits;
		int									m_processingHit;

		TEntityRespawnDataMap	m_respawndata;
		TEntityRespawnMap			m_respawns;
		TEntityRemovalMap			m_removals;

		TMinimap						m_minimap;
		TTeamObjectiveMap		m_objectives;

		TSpawnLocations			m_spawnLocations;
		TSpawnGroupMap			m_spawnGroups;

		TSpawnLocations			m_spectatorLocations;

		bool	m_bBlockPlayerAddition;

		int									m_currentStateId;

		THitListenerVec     m_hitListeners;

		CTimeValue					m_endTime;	// time the game will end. 0 for unlimited
		CTimeValue					m_roundEndTime;	// time the round will end. 0 for unlimited
		CTimeValue					m_preRoundEndTime;	// time the pre round will end. 0 for no preround
		CTimeValue					m_reviveCycleEndTime; // time for reinforcements.
		CTimeValue					m_gameStartTime; // time for game start, <= 0 means game started already
		CTimeValue					m_gameStartedTime;	// time the game started at.
		CTimeValue					m_cachedServerTime; // server time as of the last call to CGameRules::Update(...)
		CTimeValue					m_hostMigrationTimeSinceGameStarted;
		float						m_timeLimit;

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
		TTeamIdVoiceGroupMap	m_teamVoiceGroups;
#endif

		TGameRulesListenerVec	m_rulesListeners;
		static int					s_invulnID;
		static int          s_barbWireID;

		EntityId					  m_ignoreEntityNextCollision;

		bool                m_timeOfDayInitialized;

		bool                m_explosionScreenFX;

		typedef std::vector<IGameRulesClientConnectionListener *> TClientConnectionListenersVec;
		TClientConnectionListenersVec m_clientConnectionListeners;

		// Used to store the pertinent details of migrating player entities so they
		// can be reconstructed as close as possible to their state prior to migration
		SMigratingPlayerInfo *m_pMigratingPlayerInfo;
		uint32 m_migratingPlayerMaxCount;

		static const int MAX_PLAYERS = MAX_PLAYER_LIMIT;
		TNetChannelID m_migratedPlayerChannels[MAX_PLAYERS];

		SHostMigrationClientRequestParams *m_pHostMigrationParams;
		SHostMigrationClientControlledParams *m_pHostMigrationClientParams;

		SHostMigrationItemInfo *m_pHostMigrationItemInfo;
		uint32 m_hostMigrationItemMaxCount;

		bool m_hostMigrationClientHasRejoined;

		TEntityIdVec m_hostMigrationCachedEntities;
		TEntityIdVec m_entityEventDoneListeners;

		TCryUserIdSet m_participatingUsers;

		// vehicles are disabled but these are needed
	public:
		void IGameRules::OnVehicleDestroyed(EntityId) {};
		void IGameRules::OnVehicleSubmerged(EntityId, float) {};
		void IGameRules::OnVehicleFlipped(EntityId) {};


};

#define NOTIFY_UI_MP( fct ) { \
		CUIMultiPlayer* pUIEvt = UIEvents::Get<CUIMultiPlayer>(); \
		if (pUIEvt) pUIEvt->fct; } \
	 
#define NOTIFY_UI_LOBBY( fct ) { \
		CUILobbyMP* pUIEvt = UIEvents::Get<CUILobbyMP>(); \
		if (pUIEvt) pUIEvt->fct; } \
	 
#define NOTIFY_UI_OBJECTIVES( fct ) { \
		CUIObjectives* pUIEvt = UIEvents::Get<CUIObjectives>(); \
		if (pUIEvt) pUIEvt->fct; } \
	 
#endif //__GAMERULES_H__
