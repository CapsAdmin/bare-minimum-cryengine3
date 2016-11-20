#include "ParticleParams.h"

STRUCT_INFO_BEGIN(RandomColor)
	BASE_INFO(UnitFloat8)
	VAR_INFO(m_bRandomHue)
STRUCT_INFO_END(RandomColor)

STRUCT_INFO_T_BEGIN(TVarParam, class, S)
	BASE_INFO(S)
	VAR_INFO(m_Random)
STRUCT_INFO_T_END(TVarParam, class, S)

STRUCT_INFO_T_BEGIN(TVarEParam, class, S)
	BASE_INFO(TVarParam<S>)
	VAR_INFO(m_EmitterStrength)
STRUCT_INFO_T_END(TVarEParam, class, S)

STRUCT_INFO_T_BEGIN(TVarEPParam, class, S)
	BASE_INFO(TVarEParam<S>)
	VAR_INFO(m_ParticleLife)
STRUCT_INFO_T_END(TVarEPParam, class, S)

STRUCT_INFO_BEGIN(STextureTiling)
	VAR_INFO_ATTRS(nTilesX, "<Min=1><Max=256>")
	VAR_INFO_ATTRS(nTilesY, "<Min=1><Max=256>")
	VAR_INFO(nFirstTile)
	VAR_INFO_ATTRS(nVariantCount, "<Min=1>" )
	VAR_INFO_ATTRS(nAnimFramesCount, "<Min=1>")
	VAR_INFO_ATTRS(fAnimFramerate, "<SoftMax=60>")
	VAR_INFO(eAnimCycle)
	VAR_INFO(bAnimBlend)
STRUCT_INFO_END(STextureTiling)

ENUM_INFO_BEGIN(EParticleSpawnType)
	ENUM_ELEM_INFO(, ParticleSpawn_Direct)
	ENUM_ELEM_INFO(, ParticleSpawn_ParentStart)
	ENUM_ELEM_INFO(, ParticleSpawn_ParentCollide)
	ENUM_ELEM_INFO(, ParticleSpawn_ParentDeath)
ENUM_INFO_END(EParticleSpawnType)

ENUM_INFO_BEGIN(EParticleBlendType)
	ENUM_ELEM_INFO(, ParticleBlendType_AlphaBased)
	ENUM_ELEM_INFO(, ParticleBlendType_Additive)
	ENUM_ELEM_INFO(, ParticleBlendType_Multiplicative)
	ENUM_ELEM_INFO(, ParticleBlendType_Opaque)
ENUM_INFO_END(EParticleBlendType)

ENUM_INFO_BEGIN(EParticleFacing)
	ENUM_ELEM_INFO(, ParticleFacing_Camera)
	ENUM_ELEM_INFO(, ParticleFacing_Free)
	ENUM_ELEM_INFO(, ParticleFacing_Velocity)
	ENUM_ELEM_INFO(, ParticleFacing_Water)
	ENUM_ELEM_INFO(, ParticleFacing_Terrain)
	ENUM_ELEM_INFO(, ParticleFacing_Decal)
ENUM_INFO_END(EParticleFacing)

ENUM_INFO_BEGIN(EParticlePhysicsType)
	ENUM_ELEM_INFO(, ParticlePhysics_None)
	ENUM_ELEM_INFO(, ParticlePhysics_SimpleCollision)
	ENUM_ELEM_INFO(, ParticlePhysics_SimplePhysics)
	ENUM_ELEM_INFO(, ParticlePhysics_RigidBody)
ENUM_INFO_END(EParticlePhysicsType)

ENUM_INFO_BEGIN(EParticleForceType)
	ENUM_ELEM_INFO(, ParticleForce_None)
	ENUM_ELEM_INFO(, ParticleForce_Wind)
	ENUM_ELEM_INFO(, ParticleForce_Gravity)
	ENUM_ELEM_INFO(, ParticleForce_Target)
ENUM_INFO_END(EParticleForceType)

ENUM_INFO_BEGIN(EAnimationCycle)
	ENUM_ELEM_INFO(, AnimationCycle_Once)
	ENUM_ELEM_INFO(, AnimationCycle_Loop)
	ENUM_ELEM_INFO(, AnimationCycle_Mirror)
ENUM_INFO_END(EAnimationCycle)

ENUM_INFO_BEGIN(ETextureMapping)
	ENUM_ELEM_INFO(, TextureMapping_PerParticle)
	ENUM_ELEM_INFO(, TextureMapping_PerStream)
ENUM_INFO_END(ETextureMapping)

ENUM_INFO_BEGIN(EMoveRelative)
	ENUM_ELEM_INFO(, MoveRelative_No)
	ENUM_ELEM_INFO(, MoveRelative_Yes)
	ENUM_ELEM_INFO(, MoveRelative_YesWithTail)
ENUM_INFO_END(EMoveRelative)

ENUM_INFO_BEGIN(EParticleHalfResFlag)
	ENUM_ELEM_INFO(, ParticleHalfResFlag_NotAllowed)
	ENUM_ELEM_INFO(, ParticleHalfResFlag_Allowed)
	ENUM_ELEM_INFO(, ParticleHalfResFlag_Forced)
ENUM_INFO_END(EParticleHalfResFlag)

ENUM_INFO_BEGIN(ETrinary)
	ENUM_ELEM_INFO(, Trinary_Both)
	ENUM_ELEM_INFO(, Trinary_If_True)
	ENUM_ELEM_INFO(, Trinary_If_False)
ENUM_INFO_END(ETrinary)

ENUM_INFO_BEGIN(EConfigSpecBrief)
	ENUM_ELEM_INFO(, ConfigSpec_Low)
	ENUM_ELEM_INFO(, ConfigSpec_Medium)
	ENUM_ELEM_INFO(, ConfigSpec_High)
	ENUM_ELEM_INFO(, ConfigSpec_VeryHigh)
ENUM_INFO_END(EConfigSpecBrief)

ENUM_INFO_BEGIN(ESoundControlTime)
	ENUM_ELEM_INFO(, SoundControlTime_EmitterLifeTime)
	ENUM_ELEM_INFO(, SoundControlTime_EmitterExtendedLifeTime)
	ENUM_ELEM_INFO(, SoundControlTime_EmitterPulsePeriod)
ENUM_INFO_END(ESoundControlTime)

STRUCT_INFO_BEGIN(ParticleParams)
	ATTRS_INFO("<Group=Emitter>")
	VAR_INFO(bEnabled)
	VAR_INFO(bContinuous)
	VAR_INFO(eSpawnIndirection)
	VAR_INFO(fCount)
	VAR_INFO(fSpawnDelay)
	VAR_INFO(fEmitterLifeTime)
	VAR_INFO(fPulsePeriod)
	VAR_INFO(fParticleLifeTime)
	VAR_INFO(bRemainWhileVisible)

	ATTRS_INFO("<Group=Location>")
	VAR_INFO(eAttachType)
	VAR_INFO(eAttachForm)
	VAR_INFO(vPositionOffset)
	VAR_INFO(vRandomOffset)
	VAR_INFO(fOffsetRoundness)
	VAR_INFO(fOffsetInnerScale)

	ATTRS_INFO("<Group=Angles>")
	VAR_INFO(fFocusAngle)
	VAR_INFO_ATTRS(fFocusAzimuth, "<SoftMax=360>")
	VAR_INFO(bFocusGravityDir)
	VAR_INFO(bFocusRotatesEmitter)
	VAR_INFO(bEmitOffsetDir)
	VAR_INFO(fEmitAngle)

	ATTRS_INFO("<Group=Appearance>")
	VAR_INFO(eFacing)
	VAR_INFO(bOrientToVelocity)
	VAR_INFO(eBlendType)
	VAR_INFO(sTexture)
	VAR_INFO(TextureTiling)
	VAR_INFO(bOctagonalShape)
	VAR_INFO(sMaterial)
	VAR_INFO(sGeometry)
	VAR_INFO(bGeometryInPieces)
	VAR_INFO_ATTRS(fAlpha, "<SoftMax=2>")
	VAR_INFO(fAlphaTest)
	VAR_INFO(cColor)
	VAR_INFO(bSoftParticle)

	ATTRS_INFO("<Group=Lighting>")
	VAR_INFO_ATTRS(fDiffuseLighting, "<SoftMax=1>")
	VAR_INFO(fDiffuseBacklighting)
	VAR_INFO_ATTRS(fEmissiveLighting, "<SoftMax=1>")
	VAR_INFO_ATTRS(fEmissiveHDRDynamic, "<SoftMin=-2><SoftMax=2>")
	VAR_INFO(bReceiveShadows)
	VAR_INFO(bCastShadows)
	VAR_INFO(bNotAffectedByFog)
	VAR_INFO(bGlobalIllumination)
	VAR_INFO(bDiffuseCubemap)
	VAR_INFO(fHeatScale)
	VAR_INFO(LightSource)

	ATTRS_INFO("<Group=Sound>")
	VAR_INFO(sSound)
	VAR_INFO(fSoundFXParam)
	VAR_INFO(eSoundControlTime)

	ATTRS_INFO("<Group=Size>")
	VAR_INFO_ATTRS(fSize, "<SoftMax=10>")
	VAR_INFO_ATTRS(fStretch, "<SoftMax=1>")
	VAR_INFO_ATTRS(fTailLength, "<SoftMax=10>")
	VAR_INFO_ATTRS(fMinPixels, "<SoftMax=10>")
	VAR_INFO(Connection)

	ATTRS_INFO("<Group=Movement>")
	VAR_INFO(fSpeed)
	VAR_INFO_ATTRS(fInheritVelocity, "<SoftMin=0><SoftMax=1>")
	VAR_INFO_ATTRS(fAirResistance, "<SoftMax=10>")
	VAR_INFO_ATTRS(fRotationalDragScale, "<SoftMax=1>")
	VAR_INFO_ATTRS(fGravityScale, "<SoftMin=-1><SoftMax=1>")
	VAR_INFO(vAcceleration)
	VAR_INFO_ATTRS(fTurbulence3DSpeed, "<SoftMax=10>")
	VAR_INFO_ATTRS(fTurbulenceSize, "<SoftMax=10>")
	VAR_INFO_ATTRS(fTurbulenceSpeed, "<SoftMin=-360><SoftMax=360>")
	VAR_INFO(eMoveRelEmitter)
	VAR_INFO(bBindEmitterToCamera)
	VAR_INFO(bSpaceLoop)
	VAR_INFO(TargetAttraction)

	ATTRS_INFO("<Group=Rotation>")
	VAR_INFO(vInitAngles)
	VAR_INFO(vRandomAngles)
	VAR_INFO_ATTRS(vRotationRate, "<SoftMin=-360><SoftMax=360>")
	VAR_INFO_ATTRS(vRandomRotationRate, "<SoftMax=360>")

	ATTRS_INFO("<Group=Collision>")
	VAR_INFO(ePhysicsType)
	VAR_INFO(bCollideTerrain)
	VAR_INFO(bCollideStaticObjects)
	VAR_INFO(bCollideDynamicObjects)
	VAR_INFO(sSurfaceType)
	VAR_INFO_ATTRS(fBounciness, "<SoftMin=0><SoftMax=1>")
	VAR_INFO_ATTRS(fDynamicFriction, "<SoftMax=10>")
	VAR_INFO_ATTRS(fThickness, "<SoftMax=1>")
	VAR_INFO_ATTRS(fDensity, "<SoftMax=2000>")
	VAR_INFO_ATTRS(nMaxCollisionEvents, "<SoftMax=8>")

	ATTRS_INFO("<Group=Visibility>")
	VAR_INFO_ATTRS(fCameraMaxDistance, "<SoftMax=100>")
	VAR_INFO_ATTRS(fCameraMinDistance, "<SoftMax=100>")
	VAR_INFO_ATTRS(fViewDistanceAdjust, "<SoftMax=1>")
	VAR_INFO(nDrawLast)
	VAR_INFO(bDrawNear)
	VAR_INFO(tVisibleIndoors)
	VAR_INFO(tVisibleUnderwater)

	ATTRS_INFO("<Group=Advanced>")
	VAR_INFO(eForceGeneration)
	VAR_INFO_ATTRS(fFillRateCost, "<SoftMax=10>")
#if PARTICLE_MOTION_BLUR
	VAR_INFO(fMotionBlurScale)
	VAR_INFO(fMotionBlurStretchScale)
	VAR_INFO(fMotionBlurCamStretchScale)
#endif
	VAR_INFO(bDrawOnTop)
	VAR_INFO(bNoOffset)
	VAR_INFO(bAllowMerging)
	VAR_INFO(eAllowHalfRes)
	VAR_INFO(bStreamable)

	ATTRS_INFO("<Group=Configuration>")
	VAR_INFO(eConfigMin)
	VAR_INFO(eConfigMax)
	VAR_INFO(tDX11)
STRUCT_INFO_END(ParticleParams)

STRUCT_INFO_BEGIN(ParticleParams::SLightSource)
	VAR_INFO_ATTRS(fRadius, "<SoftMax=10>")
	VAR_INFO_ATTRS(fIntensity, "<SoftMax=10>")
	VAR_INFO_ATTRS(fHDRDynamic, "<SoftMin=-2><SoftMax=2>")
STRUCT_INFO_END(ParticleParams::SLightSource)

STRUCT_INFO_BEGIN(ParticleParams::SConnection)
	BASE_INFO(TSmallBool)
	VAR_INFO(bConnectToOrigin)
	VAR_INFO(eTextureMapping)
	VAR_INFO(fTextureFrequency)
STRUCT_INFO_END(ParticleParams::SConnection)

STRUCT_INFO_BEGIN(ParticleParams::STargetAttraction)
	VAR_INFO(bIgnore)
	VAR_INFO(bExtendSpeed)
	VAR_INFO(bShrink)
	VAR_INFO(bOrbit)
	VAR_INFO(fOrbitDistance)
STRUCT_INFO_END(ParticleParams::STargetAttraction)

STRUCT_INFO_BEGIN(ParticleParams::SStretch)
	BASE_INFO_ATTRS(TVarEPParam<UFloat>, "<SoftMax=1>")
	VAR_INFO_ATTRS(fOffsetRatio, "<SoftMin=-1><SoftMax=1>")
STRUCT_INFO_END(ParticleParams::SStretch)

STRUCT_INFO_BEGIN(ParticleParams::STailLength)
	BASE_INFO_ATTRS(TVarEPParam<UFloat>, "<SoftMax=10>")
	VAR_INFO_ATTRS(nTailSteps, "<SoftMax=16>")
STRUCT_INFO_END(ParticleParams::STailLength)