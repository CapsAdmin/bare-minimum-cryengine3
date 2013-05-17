--------------------------------------------------
--    Created By: Mikko
--   Description: Handle panicing during combat.
--------------------------
--

local Behavior = CreateAIBehavior("Cover2PanicST",
{
	Alertness = 2,
	exclusive = 1,

	-----------------------------------------------------
	Constructor = function(self,entity)

		entity:GettingAlerted();
  	AI.SetPFBlockerRadius(entity.id, eNB_ATT_TARGET, 0);
  	AI.SetPFBlockerRadius(entity.id, eNB_BEACON, 0);
  	AI.SetPFBlockerRadius(entity.id, eNB_DEAD_BODIES, 0);
  	AI.SetPFBlockerRadius(entity.id, eNB_EXPLOSIVES, 0);

		entity:SelectPipe(0,"sn_groupmember_mutilated_reaction");

	end,
	
	-----------------------------------------------------
	Destructor = function(self,entity)
	end,

	---------------------------------------------
	OnTargetApproaching	= function (self, entity)
	end,
	---------------------------------------------
	OnTargetFleeing	= function (self, entiTy)
	end,
	--------------------------------------------------
	OnCoverCompromised = function(self, entity, sender, data)
	end,
	---------------------------------------------
	OnNoTarget = function( self, entity )
	end,
	---------------------------------------------
	OnEnemySeen = function( self, entity, fDistance )
	end,
	---------------------------------------------
	OnEnemyMemory = function( self, entity )
	end,
	---------------------------------------------
	OnInterestingSoundHeard = function( self, entity )
	end,
	---------------------------------------------
	OnThreateningSoundHeard = function( self, entity )
	end,
	---------------------------------------------
	OnThreateningSeen = function( self, entity )
	end,
	---------------------------------------------
	OnSomethingSeen = function( self, entity )
	end,
	---------------------------------------------
	OnTargetDead = function( self, entity )
	end,
	---------------------------------------------
	OnReload = function( self, entity )
	end,
	---------------------------------------------
	OnGroupMemberDied = function( self, entity )
	end,
	--------------------------------------------------
	OnGroupMemberDiedNearest = function ( self, entity, sender, data )
		entity:Readibility("ai_down",1,1,0.3,0.6);
		AI.Signal(SIGNALFILTER_GROUPONLY, 1, "OnGroupMemberDied",entity.id, data );
	end,
	--------------------------------------------------
	OnBulletRain = function(self, entity, sender, data)
	end,
	--------------------------------------------------
	OnNearMiss = function(self, entity, sender)
	end,
	---------------------------------------------
	OnEnemyDamage = function (self, entity, sender, data)
	end,
	--------------------------------------------------
	OnClipNearlyEmpty = function ( self, entity, sender)
	end,
	---------------------------------------------
	INCOMING_FIRE = function (self, entity, sender)
	end,
	------------------------------------------------------------------------
	ENEMYSEEN_FIRST_CONTACT = function( self, entity )
	end,
	------------------------------------------------------------------------
	ENEMYSEEN_DURING_COMBAT = function(self,entity,sender)
	end,
	---------------------------------------------
	OnBadHideSpot = function ( self, entity, sender,data)
	end,
	--------------------------------------------------
	OnNoHidingPlace = function( self, entity, sender,data )
	end,	
	--------------------------------------------------
	OnNoPathFound = function( self, entity, sender,data )
	end,	
	------------------------------------------------------------------------
	OnReloadDone = function(self,entity)
	end,
	--------------------------------------------------
	OnFriendInWay = function(self, entity)
	end,
	--------------------------------------------------
	OnPlayerLooking = function(self, entity)
	end,
	--------------------------------------------------
	OnCloseContact = function ( self, entity, sender,data)
	end,

	--------------------------------------------------
	PANIC_DONE = function (self, entity)
		AI.SetBehaviorVariable(entity.id, "Panic", false);
		local target = AI.GetTargetType(entity.id);
		if (target == AITARGET_ENEMY) then
			AI.SetBehaviorVariable(entity.id, "Attack", true);
		elseif (target == AITARGET_MEMORY) then
			AI.SetBehaviorVariable(entity.id, "Seek", true);
		else
			if(AI_Utils:IsTargetOutsideStandbyRange(entity) == 1) then
				entity.AI.hurryInStandby = 0;
				AI.SetBehaviorVariable(entity.id, "ThreatenedStandby", true);
			else
				AI.SetBehaviorVariable(entity.id, "Threatened", true);
			end
		end
	end,

	---------------------------------------------
	OnExposedToExplosion = function(self, entity, data)
		--empty
	end,

	---------------------------------------------
	OnCloseCollision = function(self, entity, data)
		--empty
	end,
})
