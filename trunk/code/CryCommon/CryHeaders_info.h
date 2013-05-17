#include "CryHeaders.h"

ENUM_INFO_BEGIN(MtlTypes)
	ENUM_ELEM_INFO(, MTL_UNKNOWN)
	ENUM_ELEM_INFO(, MTL_STANDARD)
	ENUM_ELEM_INFO(, MTL_MULTI)
	ENUM_ELEM_INFO(, MTL_2SIDED)
ENUM_INFO_END(MtlTypes)

STRUCT_INFO_BEGIN(CryVertex)
	STRUCT_VAR_INFO(p, TYPE_INFO(Vec3))
	STRUCT_VAR_INFO(n, TYPE_INFO(Vec3))
STRUCT_INFO_END(CryVertex)

STRUCT_INFO_BEGIN(CryFace)
	STRUCT_VAR_INFO(v0, TYPE_INFO(int))
	STRUCT_VAR_INFO(v1, TYPE_INFO(int))
	STRUCT_VAR_INFO(v2, TYPE_INFO(int))
	STRUCT_VAR_INFO(MatID, TYPE_INFO(int))
	STRUCT_VAR_INFO(SmGroup, TYPE_INFO(int))
STRUCT_INFO_END(CryFace)

STRUCT_INFO_BEGIN(CryUV)
	STRUCT_VAR_INFO(u, TYPE_INFO(float))
	STRUCT_VAR_INFO(v, TYPE_INFO(float))
STRUCT_INFO_END(CryUV)

STRUCT_INFO_BEGIN(CryTexFace)
	STRUCT_VAR_INFO(t0, TYPE_INFO(int))
	STRUCT_VAR_INFO(t1, TYPE_INFO(int))
	STRUCT_VAR_INFO(t2, TYPE_INFO(int))
STRUCT_INFO_END(CryTexFace)

STRUCT_INFO_BEGIN(CrySkinVtx)
	STRUCT_VAR_INFO(bVolumetric, TYPE_INFO(int))
	STRUCT_VAR_INFO(idx, TYPE_INFO_ARRAY(4, TYPE_INFO(int)))
	STRUCT_VAR_INFO(w, TYPE_INFO_ARRAY(4, TYPE_INFO(float)))
	STRUCT_VAR_INFO(M, TYPE_INFO(Matrix33))
STRUCT_INFO_END(CrySkinVtx)

STRUCT_INFO_BEGIN(TextureMap2)
	STRUCT_VAR_INFO(name, TYPE_ARRAY(32, TYPE_INFO(char)))
	STRUCT_VAR_INFO(type, TYPE_INFO(unsigned char))
	STRUCT_VAR_INFO(flags, TYPE_INFO(unsigned char))
	STRUCT_VAR_INFO(Amount, TYPE_INFO(unsigned char))
	STRUCT_VAR_INFO(Reserved, TYPE_ARRAY(1, TYPE_INFO(unsigned char)))
	STRUCT_VAR_INFO(utile, TYPE_INFO(bool))
	STRUCT_VAR_INFO(umirror, TYPE_INFO(bool))
	STRUCT_VAR_INFO(vtile, TYPE_INFO(bool))
	STRUCT_VAR_INFO(vmirror, TYPE_INFO(bool))
	STRUCT_VAR_INFO(nthFrame, TYPE_INFO(int))
	STRUCT_VAR_INFO(refSize, TYPE_INFO(int))
	STRUCT_VAR_INFO(refBlur, TYPE_INFO(float))
	STRUCT_VAR_INFO(uoff_val, TYPE_INFO(float))
	STRUCT_VAR_INFO(uscl_val, TYPE_INFO(float))
	STRUCT_VAR_INFO(urot_val, TYPE_INFO(float))
	STRUCT_VAR_INFO(voff_val, TYPE_INFO(float))
	STRUCT_VAR_INFO(vscl_val, TYPE_INFO(float))
	STRUCT_VAR_INFO(vrot_val, TYPE_INFO(float))
	STRUCT_VAR_INFO(wrot_val, TYPE_INFO(float))
	STRUCT_VAR_INFO(uoff_ctrlID, TYPE_INFO(int))
	STRUCT_VAR_INFO(uscl_ctrlID, TYPE_INFO(int))
	STRUCT_VAR_INFO(urot_ctrlID, TYPE_INFO(int))
	STRUCT_VAR_INFO(voff_ctrlID, TYPE_INFO(int))
	STRUCT_VAR_INFO(vscl_ctrlID, TYPE_INFO(int))
	STRUCT_VAR_INFO(vrot_ctrlID, TYPE_INFO(int))
	STRUCT_VAR_INFO(wrot_ctrlID, TYPE_INFO(int))
STRUCT_INFO_END(TextureMap2)

STRUCT_INFO_BEGIN(TextureMap3)
	STRUCT_VAR_INFO(name, TYPE_ARRAY(128, TYPE_INFO(char)))
	STRUCT_VAR_INFO(type, TYPE_INFO(unsigned char))
	STRUCT_VAR_INFO(flags, TYPE_INFO(unsigned char))
	STRUCT_VAR_INFO(Amount, TYPE_INFO(unsigned char))
	STRUCT_VAR_INFO(Reserved, TYPE_ARRAY(32, TYPE_INFO(unsigned char)))
	STRUCT_VAR_INFO(utile, TYPE_INFO(bool))
	STRUCT_VAR_INFO(umirror, TYPE_INFO(bool))
	STRUCT_VAR_INFO(vtile, TYPE_INFO(bool))
	STRUCT_VAR_INFO(vmirror, TYPE_INFO(bool))
	STRUCT_VAR_INFO(nthFrame, TYPE_INFO(int))
	STRUCT_VAR_INFO(refSize, TYPE_INFO(int))
	STRUCT_VAR_INFO(refBlur, TYPE_INFO(float))
	STRUCT_VAR_INFO(uoff_val, TYPE_INFO(float))
	STRUCT_VAR_INFO(uscl_val, TYPE_INFO(float))
	STRUCT_VAR_INFO(urot_val, TYPE_INFO(float))
	STRUCT_VAR_INFO(voff_val, TYPE_INFO(float))
	STRUCT_VAR_INFO(vscl_val, TYPE_INFO(float))
	STRUCT_VAR_INFO(vrot_val, TYPE_INFO(float))
	STRUCT_VAR_INFO(wrot_val, TYPE_INFO(float))
	STRUCT_VAR_INFO(uoff_ctrlID, TYPE_INFO(int))
	STRUCT_VAR_INFO(uscl_ctrlID, TYPE_INFO(int))
	STRUCT_VAR_INFO(urot_ctrlID, TYPE_INFO(int))
	STRUCT_VAR_INFO(voff_ctrlID, TYPE_INFO(int))
	STRUCT_VAR_INFO(vscl_ctrlID, TYPE_INFO(int))
	STRUCT_VAR_INFO(vrot_ctrlID, TYPE_INFO(int))
	STRUCT_VAR_INFO(wrot_ctrlID, TYPE_INFO(int))
STRUCT_INFO_END(TextureMap3)

STRUCT_INFO_BEGIN(CryLink)
	STRUCT_VAR_INFO(BoneID, TYPE_INFO(int))
	STRUCT_VAR_INFO(offset, TYPE_INFO(Vec3))
	STRUCT_VAR_INFO(Blending, TYPE_INFO(float))
STRUCT_INFO_END(CryLink)

STRUCT_INFO_BEGIN(CryIRGB)
	STRUCT_VAR_INFO(r, TYPE_INFO(unsigned char))
	STRUCT_VAR_INFO(g, TYPE_INFO(unsigned char))
	STRUCT_VAR_INFO(b, TYPE_INFO(unsigned char))
STRUCT_INFO_END(CryIRGB)

STRUCT_INFO_BEGIN(CryBonePhysics_Comp)
	STRUCT_VAR_INFO(nPhysGeom, TYPE_INFO(int))
	STRUCT_VAR_INFO(flags, TYPE_INFO(int))
	STRUCT_VAR_INFO(min, TYPE_ARRAY(3, TYPE_INFO(float)))
	STRUCT_VAR_INFO(max, TYPE_ARRAY(3, TYPE_INFO(float)))
	STRUCT_VAR_INFO(spring_angle, TYPE_ARRAY(3, TYPE_INFO(float)))
	STRUCT_VAR_INFO(spring_tension, TYPE_ARRAY(3, TYPE_INFO(float)))
	STRUCT_VAR_INFO(damping, TYPE_ARRAY(3, TYPE_INFO(float)))
	STRUCT_VAR_INFO(framemtx, TYPE_ARRAY(3, TYPE_ARRAY(3, TYPE_INFO(float))))
STRUCT_INFO_END(CryBonePhysics_Comp)

STRUCT_INFO_BEGIN(CryBoneDescData_Comp)
	STRUCT_VAR_INFO(m_nControllerID, TYPE_INFO(unsigned int))
	STRUCT_VAR_INFO(m_PhysInfo, TYPE_ARRAY(2, TYPE_INFO(BONE_PHYSICS_COMP)))
	STRUCT_VAR_INFO(m_fMass, TYPE_INFO(float))
	STRUCT_VAR_INFO(m_DefaultW2B, TYPE_INFO(Matrix34))
	STRUCT_VAR_INFO(m_DefaultB2W, TYPE_INFO(Matrix34))
	STRUCT_VAR_INFO(m_arrBoneName, TYPE_ARRAY(256, TYPE_INFO(char)))
	STRUCT_VAR_INFO(m_nLimbId, TYPE_INFO(int))
	STRUCT_VAR_INFO(m_nOffsetParent, TYPE_INFO(int))
	STRUCT_VAR_INFO(m_numChildren, TYPE_INFO(unsigned int))
	STRUCT_VAR_INFO(m_nOffsetChildren, TYPE_INFO(int))
STRUCT_INFO_END(CryBoneDescData_Comp)

STRUCT_INFO_BEGIN(BONE_ENTITY)
	STRUCT_VAR_INFO(BoneID, TYPE_INFO(int))
	STRUCT_VAR_INFO(ParentID, TYPE_INFO(int))
	STRUCT_VAR_INFO(nChildren, TYPE_INFO(int))
	STRUCT_VAR_INFO(ControllerID, TYPE_INFO(unsigned int))
	STRUCT_VAR_INFO(prop, TYPE_ARRAY(32, TYPE_INFO(char)))
	STRUCT_VAR_INFO(phys, TYPE_INFO(BONE_PHYSICS_COMP))
STRUCT_INFO_END(BONE_ENTITY)

STRUCT_INFO_BEGIN(FILE_HEADER)
	STRUCT_VAR_INFO(Signature, TYPE_ARRAY(7, TYPE_INFO(char)))
	STRUCT_VAR_INFO(_Pad_, TYPE_ARRAY(1, TYPE_INFO(char)))
	STRUCT_VAR_INFO(FileType, TYPE_INFO(int))
	STRUCT_VAR_INFO(Version, TYPE_INFO(int))
	STRUCT_VAR_INFO(ChunkTableOffset, TYPE_INFO(int))
STRUCT_INFO_END(FILE_HEADER)

ENUM_INFO_BEGIN(ChunkTypes)
	ENUM_ELEM_INFO(, ChunkType_ANY)
	ENUM_ELEM_INFO(, ChunkType_Mesh)
	ENUM_ELEM_INFO(, ChunkType_Helper)
	ENUM_ELEM_INFO(, ChunkType_VertAnim)
	ENUM_ELEM_INFO(, ChunkType_BoneAnim)
	ENUM_ELEM_INFO(, ChunkType_GeomNameList)
	ENUM_ELEM_INFO(, ChunkType_BoneNameList)
	ENUM_ELEM_INFO(, ChunkType_MtlList)
	ENUM_ELEM_INFO(, ChunkType_MRM)
	ENUM_ELEM_INFO(, ChunkType_SceneProps)
	ENUM_ELEM_INFO(, ChunkType_Light)
	ENUM_ELEM_INFO(, ChunkType_PatchMesh)
	ENUM_ELEM_INFO(, ChunkType_Node)
	ENUM_ELEM_INFO(, ChunkType_Mtl)
	ENUM_ELEM_INFO(, ChunkType_Controller)
	ENUM_ELEM_INFO(, ChunkType_Timing)
	ENUM_ELEM_INFO(, ChunkType_BoneMesh)
	ENUM_ELEM_INFO(, ChunkType_BoneLightBinding)
	ENUM_ELEM_INFO(, ChunkType_MeshMorphTarget)
	ENUM_ELEM_INFO(, ChunkType_BoneInitialPos)
	ENUM_ELEM_INFO(, ChunkType_SourceInfo)
	ENUM_ELEM_INFO(, ChunkType_MtlName)
	ENUM_ELEM_INFO(, ChunkType_ExportFlags)
	ENUM_ELEM_INFO(, ChunkType_DataStream)
	ENUM_ELEM_INFO(, ChunkType_MeshSubsets)
	ENUM_ELEM_INFO(, ChunkType_MeshPhysicsData)
	ENUM_ELEM_INFO(, ChunkType_CompiledBones)
	ENUM_ELEM_INFO(, ChunkType_CompiledPhysicalBones)
	ENUM_ELEM_INFO(, ChunkType_CompiledMorphTargets)
	ENUM_ELEM_INFO(, ChunkType_CompiledPhysicalProxies)
	ENUM_ELEM_INFO(, ChunkType_CompiledIntFaces)
	ENUM_ELEM_INFO(, ChunkType_CompiledIntSkinVertices)
	ENUM_ELEM_INFO(, ChunkType_CompiledExt2IntMap)
	ENUM_ELEM_INFO(, ChunkType_BreakablePhysics)
	ENUM_ELEM_INFO(, ChunkType_FaceMap)
	ENUM_ELEM_INFO(, ChunkType_MotionParameters)
	ENUM_ELEM_INFO(, ChunkType_FootPlantInfo)
	ENUM_ELEM_INFO(, ChunkType_BonesBoxes)
	ENUM_ELEM_INFO(, ChunkType_FoliageInfo)
ENUM_INFO_END(ChunkTypes)

STRUCT_INFO_BEGIN(CHUNK_HEADER_0744)
	STRUCT_VAR_INFO(ChunkType, TYPE_INFO(ChunkTypes))
	STRUCT_VAR_INFO(ChunkVersion, TYPE_INFO(int))
	STRUCT_VAR_INFO(FileOffset, TYPE_INFO(int))
	STRUCT_VAR_INFO(ChunkID, TYPE_INFO(int))
STRUCT_INFO_END(CHUNK_HEADER_0744)

STRUCT_INFO_BEGIN(CHUNK_TABLE_ENTRY_0745)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(ChunkSize, TYPE_INFO(int))
STRUCT_INFO_END(CHUNK_TABLE_ENTRY_0745)

STRUCT_INFO_BEGIN(RANGE_ENTITY)
	STRUCT_VAR_INFO(name, TYPE_ARRAY(32, TYPE_INFO(char)))
	STRUCT_VAR_INFO(start, TYPE_INFO(int))
	STRUCT_VAR_INFO(end, TYPE_INFO(int))
STRUCT_INFO_END(RANGE_ENTITY)

STRUCT_INFO_BEGIN(TIMING_CHUNK_DESC_0918)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(m_SecsPerTick, TYPE_INFO(float))
	STRUCT_VAR_INFO(m_TicksPerFrame, TYPE_INFO(int))
	STRUCT_VAR_INFO(global_range, TYPE_INFO(RANGE_ENTITY))
	STRUCT_VAR_INFO(qqqqnSubRanges, TYPE_INFO(int))
STRUCT_INFO_END(TIMING_CHUNK_DESC_0918)


STRUCT_INFO_BEGIN(SPEED_CHUNK_DESC_2)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(Speed, TYPE_INFO(float))
	STRUCT_VAR_INFO(Distance, TYPE_INFO(float))
	STRUCT_VAR_INFO(Slope, TYPE_INFO(float))
	STRUCT_VAR_INFO(AnimFlags, TYPE_INFO(int))
	STRUCT_VAR_INFO(MoveDir, TYPE_ARRAY(3, TYPE_INFO(f32)))
	STRUCT_VAR_INFO(StartPosition, TYPE_INFO(QuatT))
STRUCT_INFO_END(SPEED_CHUNK_DESC_2)

STRUCT_INFO_BEGIN(FOOTPLANT_INFO)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(nPoses, TYPE_INFO(int))
	STRUCT_VAR_INFO(m_LHeelStart, TYPE_INFO(f32))
	STRUCT_VAR_INFO(m_LHeelEnd, TYPE_INFO(f32))
	STRUCT_VAR_INFO(m_LToe0Start, TYPE_INFO(f32))
	STRUCT_VAR_INFO(m_LToe0End, TYPE_INFO(f32))
	STRUCT_VAR_INFO(m_RHeelStart, TYPE_INFO(f32))
	STRUCT_VAR_INFO(m_RHeelEnd, TYPE_INFO(f32))
	STRUCT_VAR_INFO(m_RToe0Start, TYPE_INFO(f32))
	STRUCT_VAR_INFO(m_RToe0End, TYPE_INFO(f32))
STRUCT_INFO_END(FOOTPLANT_INFO)

STRUCT_INFO_BEGIN(MTL_CHUNK_DESC_0745)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(name, TYPE_ARRAY(64, TYPE_INFO(char)))
	STRUCT_VAR_INFO(MtlType, TYPE_INFO(MtlTypes))
	STRUCT_VAR_INFO(col_d, TYPE_INFO(CryIRGB))
	STRUCT_VAR_INFO(col_s, TYPE_INFO(CryIRGB))
	STRUCT_VAR_INFO(col_a, TYPE_INFO(CryIRGB))
	STRUCT_VAR_INFO(specLevel, TYPE_INFO(float))
	STRUCT_VAR_INFO(specShininess, TYPE_INFO(float))
	STRUCT_VAR_INFO(selfIllum, TYPE_INFO(float))
	STRUCT_VAR_INFO(opacity, TYPE_INFO(float))
	STRUCT_VAR_INFO(tex_a, TYPE_INFO(TextureMap2))
	STRUCT_VAR_INFO(tex_d, TYPE_INFO(TextureMap2))
	STRUCT_VAR_INFO(tex_s, TYPE_INFO(TextureMap2))
	STRUCT_VAR_INFO(tex_o, TYPE_INFO(TextureMap2))
	STRUCT_VAR_INFO(tex_b, TYPE_INFO(TextureMap2))
	STRUCT_VAR_INFO(tex_g, TYPE_INFO(TextureMap2))
	STRUCT_VAR_INFO(tex_c, TYPE_INFO(TextureMap2))
	STRUCT_VAR_INFO(tex_rl, TYPE_INFO(TextureMap2))
	STRUCT_VAR_INFO(tex_subsurf, TYPE_INFO(TextureMap2))
	STRUCT_VAR_INFO(tex_det, TYPE_INFO(TextureMap2))
	STRUCT_VAR_INFO(flags, TYPE_INFO(int))
	STRUCT_VAR_INFO(Dyn_Bounce, TYPE_INFO(float))
	STRUCT_VAR_INFO(Dyn_StaticFriction, TYPE_INFO(float))
	STRUCT_VAR_INFO(Dyn_SlidingFriction, TYPE_INFO(float))
	STRUCT_VAR_INFO(nChildren, TYPE_INFO(int))
STRUCT_INFO_END(MTL_CHUNK_DESC_0745)

STRUCT_INFO_BEGIN(MTL_CHUNK_DESC_0746)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(name, TYPE_ARRAY(64, TYPE_INFO(char)))
	STRUCT_VAR_INFO(Reserved, TYPE_ARRAY(60, TYPE_INFO(char)))
	STRUCT_VAR_INFO(alphaTest, TYPE_INFO(float))
	STRUCT_VAR_INFO(MtlType, TYPE_INFO(MtlTypes))
	STRUCT_VAR_INFO(col_d, TYPE_INFO(CryIRGB))
	STRUCT_VAR_INFO(col_s, TYPE_INFO(CryIRGB))
	STRUCT_VAR_INFO(col_a, TYPE_INFO(CryIRGB))
	STRUCT_VAR_INFO(specLevel, TYPE_INFO(float))
	STRUCT_VAR_INFO(specShininess, TYPE_INFO(float))
	STRUCT_VAR_INFO(selfIllum, TYPE_INFO(float))
	STRUCT_VAR_INFO(opacity, TYPE_INFO(float))
	STRUCT_VAR_INFO(tex_a, TYPE_INFO(TextureMap3))
	STRUCT_VAR_INFO(tex_d, TYPE_INFO(TextureMap3))
	STRUCT_VAR_INFO(tex_s, TYPE_INFO(TextureMap3))
	STRUCT_VAR_INFO(tex_o, TYPE_INFO(TextureMap3))
	STRUCT_VAR_INFO(tex_b, TYPE_INFO(TextureMap3))
	STRUCT_VAR_INFO(tex_g, TYPE_INFO(TextureMap3))
	STRUCT_VAR_INFO(tex_fl, TYPE_INFO(TextureMap3))
	STRUCT_VAR_INFO(tex_rl, TYPE_INFO(TextureMap3))
	STRUCT_VAR_INFO(tex_subsurf, TYPE_INFO(TextureMap3))
	STRUCT_VAR_INFO(tex_det, TYPE_INFO(TextureMap3))
	STRUCT_VAR_INFO(flags, TYPE_INFO(int))
	STRUCT_VAR_INFO(Dyn_Bounce, TYPE_INFO(float))
	STRUCT_VAR_INFO(Dyn_StaticFriction, TYPE_INFO(float))
	STRUCT_VAR_INFO(Dyn_SlidingFriction, TYPE_INFO(float))
	STRUCT_VAR_INFO(nChildren, TYPE_INFO(int))
STRUCT_INFO_END(MTL_CHUNK_DESC_0746)

STRUCT_INFO_BEGIN(MTL_NAME_CHUNK_DESC_0800)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(nFlags, TYPE_INFO(int))
	STRUCT_VAR_INFO(nFlags2, TYPE_INFO(int))
	STRUCT_VAR_INFO(name, TYPE_ARRAY(128, TYPE_INFO(char)))
	STRUCT_VAR_INFO(nPhysicalizeType, TYPE_INFO(int))
	STRUCT_VAR_INFO(nSubMaterials, TYPE_INFO(int))
	STRUCT_VAR_INFO(nSubMatChunkId, TYPE_ARRAY(32, TYPE_INFO(int)))
	STRUCT_VAR_INFO(nAdvancedDataChunkId, TYPE_INFO(int))
	STRUCT_VAR_INFO(sh_opacity, TYPE_INFO(float))
	STRUCT_VAR_INFO(reserve, TYPE_ARRAY(32, TYPE_INFO(int)))
STRUCT_INFO_END(MTL_NAME_CHUNK_DESC_0800)

STRUCT_INFO_BEGIN(MESH_CHUNK_DESC_0744)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(flags1, TYPE_INFO(unsigned char))
	STRUCT_VAR_INFO(flags2, TYPE_INFO(unsigned char))
	STRUCT_VAR_INFO(nVerts, TYPE_INFO(int))
	STRUCT_VAR_INFO(nTVerts, TYPE_INFO(int))
	STRUCT_VAR_INFO(nFaces, TYPE_INFO(int))
	STRUCT_VAR_INFO(VertAnimID, TYPE_INFO(int))
STRUCT_INFO_END(MESH_CHUNK_DESC_0744)

STRUCT_INFO_BEGIN(MESH_CHUNK_DESC_0800)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(nFlags, TYPE_INFO(int))
	STRUCT_VAR_INFO(nFlags2, TYPE_INFO(int))
	STRUCT_VAR_INFO(nVerts, TYPE_INFO(int))
	STRUCT_VAR_INFO(nIndices, TYPE_INFO(int))
	STRUCT_VAR_INFO(nSubsets, TYPE_INFO(int))
	STRUCT_VAR_INFO(nSubsetsChunkId, TYPE_INFO(int))
	STRUCT_VAR_INFO(nVertAnimID, TYPE_INFO(int))
	STRUCT_VAR_INFO(nStreamChunkID, TYPE_ARRAY(16, TYPE_INFO(int)))
	STRUCT_VAR_INFO(nPhysicsDataChunkId, TYPE_ARRAY(4, TYPE_INFO(int)))
	STRUCT_VAR_INFO(bboxMin, TYPE_INFO(Vec3))
	STRUCT_VAR_INFO(bboxMax, TYPE_INFO(Vec3))
	STRUCT_VAR_INFO(texMappingDensity, TYPE_INFO(float))
	STRUCT_VAR_INFO(reserved, TYPE_ARRAY(31, TYPE_INFO(int)))
STRUCT_INFO_END(MESH_CHUNK_DESC_0800)

STRUCT_INFO_BEGIN(STREAM_DATA_CHUNK_DESC_0800)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(nFlags, TYPE_INFO(int))
	STRUCT_VAR_INFO(nStreamType, TYPE_INFO(int))
	STRUCT_VAR_INFO(nCount, TYPE_INFO(int))
	STRUCT_VAR_INFO(nElementSize, TYPE_INFO(int))
	STRUCT_VAR_INFO(reserved, TYPE_ARRAY(2, TYPE_INFO(int)))
STRUCT_INFO_END(STREAM_DATA_CHUNK_DESC_0800)

STRUCT_INFO_BEGIN(MESH_SUBSETS_CHUNK_DESC_0800::MeshSubset)
	STRUCT_VAR_INFO(nFirstIndexId, TYPE_INFO(int))
	STRUCT_VAR_INFO(nNumIndices, TYPE_INFO(int))
	STRUCT_VAR_INFO(nFirstVertId, TYPE_INFO(int))
	STRUCT_VAR_INFO(nNumVerts, TYPE_INFO(int))
	STRUCT_VAR_INFO(nMatID, TYPE_INFO(int))
	STRUCT_VAR_INFO(fRadius, TYPE_INFO(float))
	STRUCT_VAR_INFO(vCenter, TYPE_INFO(Vec3))
STRUCT_INFO_END(MESH_SUBSETS_CHUNK_DESC_0800::MeshSubset)

STRUCT_INFO_BEGIN(MESH_SUBSETS_CHUNK_DESC_0800::MeshBoneIDs)
	STRUCT_VAR_INFO(numBoneIDs, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(arrBoneIDs, TYPE_ARRAY(128, TYPE_INFO(uint16)))
STRUCT_INFO_END(MESH_SUBSETS_CHUNK_DESC_0800::MeshBoneIDs)

STRUCT_INFO_BEGIN(MESH_SUBSETS_CHUNK_DESC_0800)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(nFlags, TYPE_INFO(int))
	STRUCT_VAR_INFO(nCount, TYPE_INFO(int))
	STRUCT_VAR_INFO(reserved, TYPE_ARRAY(2, TYPE_INFO(int)))
STRUCT_INFO_END(MESH_SUBSETS_CHUNK_DESC_0800)

STRUCT_INFO_BEGIN(MESH_PHYSICS_DATA_CHUNK_DESC_0800)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(nDataSize, TYPE_INFO(int))
	STRUCT_VAR_INFO(nFlags, TYPE_INFO(int))
	STRUCT_VAR_INFO(nTetrahedraDataSize, TYPE_INFO(int))
	STRUCT_VAR_INFO(nTetrahedraChunkId, TYPE_INFO(int))
	STRUCT_VAR_INFO(reserved, TYPE_ARRAY(2, TYPE_INFO(int)))
STRUCT_INFO_END(MESH_PHYSICS_DATA_CHUNK_DESC_0800)

STRUCT_INFO_BEGIN(BONEANIM_CHUNK_DESC_0290)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(nBones, TYPE_INFO(int))
STRUCT_INFO_END(BONEANIM_CHUNK_DESC_0290)

STRUCT_INFO_BEGIN(BONENAMELIST_CHUNK_DESC_0744)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(nEntities, TYPE_INFO(int))
STRUCT_INFO_END(BONENAMELIST_CHUNK_DESC_0744)

STRUCT_INFO_BEGIN(BONENAMELIST_CHUNK_DESC_0745)
	STRUCT_VAR_INFO(numEntities, TYPE_INFO(int))
STRUCT_INFO_END(BONENAMELIST_CHUNK_DESC_0745)

STRUCT_INFO_BEGIN(COMPILED_BONE_CHUNK_DESC_0800)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(reserved, TYPE_ARRAY(32, TYPE_INFO(char)))
STRUCT_INFO_END(COMPILED_BONE_CHUNK_DESC_0800)

STRUCT_INFO_BEGIN(COMPILED_PHYSICALBONE_CHUNK_DESC_0800)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(reserved, TYPE_ARRAY(32, TYPE_INFO(char)))
STRUCT_INFO_END(COMPILED_PHYSICALBONE_CHUNK_DESC_0800)

STRUCT_INFO_BEGIN(COMPILED_PHYSICALPROXY_CHUNK_DESC_0800)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(numPhysicalProxies, TYPE_INFO(uint32))
STRUCT_INFO_END(COMPILED_PHYSICALPROXY_CHUNK_DESC_0800)

STRUCT_INFO_BEGIN(COMPILED_MORPHTARGETS_CHUNK_DESC_0800)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(numMorphTargets, TYPE_INFO(uint32))
STRUCT_INFO_END(COMPILED_MORPHTARGETS_CHUNK_DESC_0800)

STRUCT_INFO_BEGIN(COMPILED_INTFACES_CHUNK_DESC_0800)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
STRUCT_INFO_END(COMPILED_INTFACES_CHUNK_DESC_0800)

STRUCT_INFO_BEGIN(COMPILED_INTSKINVERTICES_CHUNK_DESC_0800)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(reserved, TYPE_ARRAY(32, TYPE_INFO(char)))
STRUCT_INFO_END(COMPILED_INTSKINVERTICES_CHUNK_DESC_0800)

STRUCT_INFO_BEGIN(COMPILED_EXT2INTMAP_CHUNK_DESC_0800)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
STRUCT_INFO_END(COMPILED_EXT2INTMAP_CHUNK_DESC_0800)

STRUCT_INFO_BEGIN(COMPILED_BONEBOXES_CHUNK_DESC_0800)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
STRUCT_INFO_END(COMPILED_BONEBOXES_CHUNK_DESC_0800)

STRUCT_INFO_BEGIN(BaseKey)
	STRUCT_VAR_INFO(time, TYPE_INFO(int))
STRUCT_INFO_END(BaseKey)

STRUCT_INFO_BEGIN(BaseTCB)
	STRUCT_VAR_INFO(t, TYPE_INFO(float))
	STRUCT_VAR_INFO(c, TYPE_INFO(float))
	STRUCT_VAR_INFO(b, TYPE_INFO(float))
	STRUCT_VAR_INFO(ein, TYPE_INFO(float))
	STRUCT_VAR_INFO(eout, TYPE_INFO(float))
STRUCT_INFO_END(BaseTCB)

STRUCT_INFO_BEGIN(BaseKey3)
	STRUCT_BASE_INFO(BaseKey)
	STRUCT_VAR_INFO(val, TYPE_INFO(Vec3))
STRUCT_INFO_END(BaseKey3)

STRUCT_INFO_BEGIN(BaseKeyQ)
	STRUCT_BASE_INFO(BaseKey)
	STRUCT_VAR_INFO(val, TYPE_INFO(CryQuat))
STRUCT_INFO_END(BaseKeyQ)

STRUCT_INFO_BEGIN(CryTCB3Key)
	STRUCT_BASE_INFO(BaseKey3)
	STRUCT_BASE_INFO(BaseTCB)
STRUCT_INFO_END(CryTCB3Key)

STRUCT_INFO_BEGIN(CryTCBQKey)
	STRUCT_BASE_INFO(BaseKeyQ)
	STRUCT_BASE_INFO(BaseTCB)
STRUCT_INFO_END(CryTCBQKey)

STRUCT_INFO_BEGIN(CryKeyPQLog)
	STRUCT_VAR_INFO(nTime, TYPE_INFO(int))
	STRUCT_VAR_INFO(vPos, TYPE_INFO(Vec3))
	STRUCT_VAR_INFO(vRotLog, TYPE_INFO(Vec3))
STRUCT_INFO_END(CryKeyPQLog)

ENUM_INFO_BEGIN(CtrlTypes)
	ENUM_ELEM_INFO(, CTRL_NONE)
	ENUM_ELEM_INFO(, CTRL_CRYBONE)
	ENUM_ELEM_INFO(, CTRL_LINEER1)
	ENUM_ELEM_INFO(, CTRL_LINEER3)
	ENUM_ELEM_INFO(, CTRL_LINEERQ)
	ENUM_ELEM_INFO(, CTRL_BEZIER1)
	ENUM_ELEM_INFO(, CTRL_BEZIER3)
	ENUM_ELEM_INFO(, CTRL_BEZIERQ)
	ENUM_ELEM_INFO(, CTRL_TCB1)
	ENUM_ELEM_INFO(, CTRL_TCB3)
	ENUM_ELEM_INFO(, CTRL_TCBQ)
	ENUM_ELEM_INFO(, CTRL_BSPLINE_2O)
	ENUM_ELEM_INFO(, CTRL_BSPLINE_1O)
	ENUM_ELEM_INFO(, CTRL_BSPLINE_2C)
	ENUM_ELEM_INFO(, CTRL_BSPLINE_1C)
	ENUM_ELEM_INFO(, CTRL_CONST)
ENUM_INFO_END(CtrlTypes)

STRUCT_INFO_BEGIN(CONTROLLER_CHUNK_DESC_0826)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(type, TYPE_INFO(CtrlTypes))
	STRUCT_VAR_INFO(nKeys, TYPE_INFO(int))
	STRUCT_VAR_INFO(nFlags, TYPE_INFO(unsigned int))
	STRUCT_VAR_INFO(nControllerId, TYPE_INFO(unsigned int))
STRUCT_INFO_END(CONTROLLER_CHUNK_DESC_0826)

STRUCT_INFO_BEGIN(CONTROLLER_CHUNK_DESC_0827)
	STRUCT_VAR_INFO(numKeys, TYPE_INFO(unsigned int))
	STRUCT_VAR_INFO(nControllerId, TYPE_INFO(unsigned int))
STRUCT_INFO_END(CONTROLLER_CHUNK_DESC_0827)

STRUCT_INFO_BEGIN(CONTROLLER_CHUNK_DESC_0828)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(numKeys, TYPE_INFO(unsigned int))
	STRUCT_VAR_INFO(nControllerId, TYPE_INFO(unsigned int))
STRUCT_INFO_END(CONTROLLER_CHUNK_DESC_0828)

STRUCT_INFO_TYPE_EMPTY(BASE_CONTROLLER_CHUNK)

STRUCT_INFO_BEGIN(CONTROLLER_CHUNK_DESC_0829)
	STRUCT_BASE_INFO(BASE_CONTROLLER_CHUNK)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(nControllerId, TYPE_INFO(unsigned int))
	STRUCT_VAR_INFO(numRotationKeys, TYPE_INFO(uint16))
	STRUCT_VAR_INFO(numPositionKeys, TYPE_INFO(uint16))
	STRUCT_VAR_INFO(RotationFormat, TYPE_INFO(uint8))
	STRUCT_VAR_INFO(RotationTimeFormat, TYPE_INFO(uint8))
	STRUCT_VAR_INFO(PositionFormat, TYPE_INFO(uint8))
	STRUCT_VAR_INFO(PositionKeysInfo, TYPE_INFO(uint8))
	STRUCT_VAR_INFO(PositionTimeFormat, TYPE_INFO(uint8))
STRUCT_INFO_END(CONTROLLER_CHUNK_DESC_0829)

STRUCT_INFO_BEGIN(CONTROLLER_CHUNK_DESC_0900)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(numKeyPos, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(numKeyRot, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(numKeyTime, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(numAnims, TYPE_INFO(uint32))
STRUCT_INFO_END(CONTROLLER_CHUNK_DESC_0900)

STRUCT_INFO_BEGIN(CONTROLLER_CHUNK_DESC_0901)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(numTimesteps, TYPE_INFO(uint32))
STRUCT_INFO_END(CONTROLLER_CHUNK_DESC_0901)

STRUCT_INFO_BEGIN(CONTROLLER_CHUNK_DESC_0902)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(numStartDirs, TYPE_INFO(uint32))
STRUCT_INFO_END(CONTROLLER_CHUNK_DESC_0902)

STRUCT_INFO_BEGIN(CONTROLLER_CHUNK_DESC_0903)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(numKeyPos, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(numKeyRot, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(numKeyTime, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(numAnims, TYPE_INFO(uint32))
STRUCT_INFO_END(CONTROLLER_CHUNK_DESC_0903)

STRUCT_INFO_BEGIN(CONTROLLER_CHUNK_DESC_0904)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(numKeyPos, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(numKeyRot, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(numKeyTime, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(numAnims, TYPE_INFO(uint32))
STRUCT_INFO_END(CONTROLLER_CHUNK_DESC_0904)

STRUCT_INFO_BEGIN(CONTROLLER_CHUNK_DESC_0905)
STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
STRUCT_VAR_INFO(numKeyPos, TYPE_INFO(uint32))
STRUCT_VAR_INFO(numKeyRot, TYPE_INFO(uint32))
STRUCT_VAR_INFO(numKeyTime, TYPE_INFO(uint32))
STRUCT_VAR_INFO(numAnims, TYPE_INFO(uint32))
STRUCT_INFO_END(CONTROLLER_CHUNK_DESC_0905)

STRUCT_INFO_BEGIN(NODE_CHUNK_DESC_0823)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(name, TYPE_ARRAY(64, TYPE_INFO(char)))
	STRUCT_VAR_INFO(ObjectID, TYPE_INFO(int))
	STRUCT_VAR_INFO(ParentID, TYPE_INFO(int))
	STRUCT_VAR_INFO(nChildren, TYPE_INFO(int))
	STRUCT_VAR_INFO(MatID, TYPE_INFO(int))
	STRUCT_VAR_INFO(IsGroupHead, TYPE_INFO(bool))
	STRUCT_VAR_INFO(IsGroupMember, TYPE_INFO(bool))
	STRUCT_VAR_INFO(tm, TYPE_ARRAY(4, TYPE_ARRAY(4, TYPE_INFO(float))))
	STRUCT_VAR_INFO(pos, TYPE_INFO(Vec3))
	STRUCT_VAR_INFO(rot, TYPE_INFO(CryQuat))
	STRUCT_VAR_INFO(scl, TYPE_INFO(Vec3))
	STRUCT_VAR_INFO(pos_cont_id, TYPE_INFO(int))
	STRUCT_VAR_INFO(rot_cont_id, TYPE_INFO(int))
	STRUCT_VAR_INFO(scl_cont_id, TYPE_INFO(int))
	STRUCT_VAR_INFO(PropStrLen, TYPE_INFO(int))
STRUCT_INFO_END(NODE_CHUNK_DESC_0823)

ENUM_INFO_BEGIN(HelperTypes)
	ENUM_ELEM_INFO(, HP_POINT)
	ENUM_ELEM_INFO(, HP_DUMMY)
	ENUM_ELEM_INFO(, HP_XREF)
	ENUM_ELEM_INFO(, HP_CAMERA)
	ENUM_ELEM_INFO(, HP_GEOMETRY)
ENUM_INFO_END(HelperTypes)

STRUCT_INFO_BEGIN(HELPER_CHUNK_DESC_0744)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(type, TYPE_INFO(HelperTypes))
	STRUCT_VAR_INFO(size, TYPE_INFO(Vec3))
STRUCT_INFO_END(HELPER_CHUNK_DESC_0744)

STRUCT_INFO_BEGIN(MESHMORPHTARGET_CHUNK_DESC_0001)
	STRUCT_VAR_INFO(nChunkIdMesh, TYPE_INFO(unsigned int))
	STRUCT_VAR_INFO(numMorphVertices, TYPE_INFO(unsigned int))
STRUCT_INFO_END(MESHMORPHTARGET_CHUNK_DESC_0001)

STRUCT_INFO_BEGIN(SMeshMorphTargetVertex)
	STRUCT_VAR_INFO(nVertexId, TYPE_INFO(unsigned int))
	STRUCT_VAR_INFO(ptVertex, TYPE_INFO(Vec3))
STRUCT_INFO_END(SMeshMorphTargetVertex)

STRUCT_INFO_BEGIN(SMeshMorphTargetHeader)
	STRUCT_VAR_INFO(MeshID, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(NameLength, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(numIntVertices, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(numExtVertices, TYPE_INFO(uint32))
STRUCT_INFO_END(SMeshMorphTargetHeader)

STRUCT_INFO_BEGIN(SMeshPhysicalProxyHeader)
	STRUCT_VAR_INFO(ChunkID, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(numPoints, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(numIndices, TYPE_INFO(uint32))
	STRUCT_VAR_INFO(numMaterials, TYPE_INFO(uint32))
STRUCT_INFO_END(SMeshPhysicalProxyHeader)

STRUCT_INFO_BEGIN(BONEINITIALPOS_CHUNK_DESC_0001)
	STRUCT_VAR_INFO(nChunkIdMesh, TYPE_INFO(unsigned int))
	STRUCT_VAR_INFO(numBones, TYPE_INFO(unsigned int))
STRUCT_INFO_END(BONEINITIALPOS_CHUNK_DESC_0001)

STRUCT_INFO_BEGIN(SBoneInitPosMatrix)
	STRUCT_VAR_INFO(mx, TYPE_ARRAY(4, TYPE_ARRAY(3, TYPE_INFO(float))))
STRUCT_INFO_END(SBoneInitPosMatrix)

STRUCT_INFO_BEGIN(EXPORT_FLAGS_CHUNK_DESC)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(flags, TYPE_INFO(unsigned int))
	STRUCT_VAR_INFO(rc_version, TYPE_ARRAY(4, TYPE_INFO(unsigned int)))
	STRUCT_VAR_INFO(rc_version_string, TYPE_ARRAY(16, TYPE_INFO(char)))
	STRUCT_VAR_INFO(reserved, TYPE_ARRAY(32, TYPE_INFO(unsigned int)))
STRUCT_INFO_END(EXPORT_FLAGS_CHUNK_DESC)

STRUCT_INFO_BEGIN(BREAKABLE_PHYSICS_CHUNK_DESC)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(granularity, TYPE_INFO(unsigned int))
	STRUCT_VAR_INFO(nMode, TYPE_INFO(int))
	STRUCT_VAR_INFO(nRetVtx, TYPE_INFO(int))
	STRUCT_VAR_INFO(nRetTets, TYPE_INFO(int))
	STRUCT_VAR_INFO(nReserved, TYPE_ARRAY(10, TYPE_INFO(int)))
STRUCT_INFO_END(BREAKABLE_PHYSICS_CHUNK_DESC)

STRUCT_INFO_BEGIN(FACEMAP_CHUNK_DESC)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(nodeName, TYPE_ARRAY(64, TYPE_INFO(char)))
	STRUCT_VAR_INFO(StreamID, TYPE_INFO(int))
STRUCT_INFO_END(FACEMAP_CHUNK_DESC)

STRUCT_INFO_BEGIN(FOLIAGE_INFO_CHUNK_DESC)
	STRUCT_VAR_INFO(chdr, TYPE_INFO(CHUNK_HEADER))
	STRUCT_VAR_INFO(nSpines, TYPE_INFO(int))
	STRUCT_VAR_INFO(nSpineVtx, TYPE_INFO(int))
	STRUCT_VAR_INFO(nSkinnedVtx, TYPE_INFO(int))
	STRUCT_VAR_INFO(nBoneIds, TYPE_INFO(int))
STRUCT_INFO_END(FOLIAGE_INFO_CHUNK_DESC)

