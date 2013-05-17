<IK_Definition>

  <AimIK_Definition>

    <RotationList>
      <Rotation JointName="Bip01 Spine"                  Primary="1" Additive="0" />
      <Rotation JointName="Bip01 Spine1"                 Primary="1" Additive="0" />
      <Rotation JointName="Bip01 Spine2"                 Primary="1" Additive="0" />
      <Rotation JointName="Bip01 Spine3"                 Primary="1" Additive="0" />
      <Rotation JointName="Bip01 Neck"                   Primary="1" Additive="0" />
      <Rotation JointName="Bip01 Head"                   Primary="0" Additive="0" />

      <Rotation JointName="Bip01 R Clavicle"             Primary="1" Additive="0" />
      <Rotation JointName="Bip01 R UpperArm"             Primary="1" Additive="0" />
      <Rotation JointName="Bip01 R ForeArm"              Primary="1" Additive="0" />
      <Rotation JointName="Bip01 R Hand"                 Primary="1" Additive="0" />
      <Rotation JointName="weapon_bone"                  Primary="1" Additive="0" />

      <Rotation JointName="Bip01 L Clavicle"             Primary="0" Additive="0" />
      <Rotation JointName="Bip01 L UpperArm"             Primary="0" Additive="0" />
      <Rotation JointName="Bip01 L ForeArm"              Primary="0" Additive="0" />
      <Rotation JointName="Bip01 L Hand"                 Primary="0" Additive="0" />
    </RotationList>


    <PositionList>
      <Position JointName="Bip01 R Clavicle"             />
      <Position JointName="weapon_bone"	                 />
      <Position JointName="Bip01 L Clavicle"             />
    </PositionList>

    <RWeaponJoint JointName="weapon_bone"  />

    <DirectionalBlends>
      <Joint AnimToken="AimPoses"  ParameterJoint="weapon_bone"  StartJoint="Bip01 R Clavicle" />
    </DirectionalBlends>

  </AimIK_Definition>



  <LookIK_Definition>

  </LookIK_Definition>


  <LimbIK_Definition>
    <IK Handle="RgtArm01" Solver="2BIK" Root="Bip01 R UpperArm" EndEffector="Bip01 R Hand" />
    <IK Handle="LftArm01" Solver="2BIK" Root="Bip01 L UpperArm" EndEffector="Bip01 L Hand" />
    <IK Handle="RgtLeg01" Solver="2BIK" Root="Bip01 R Thigh"    EndEffector="Bip01 R Foot" />
    <IK Handle="LftLeg01" Solver="2BIK" Root="Bip01 L Thigh"    EndEffector="Bip01 L Foot" />
    <IK Handle="Spine_SpineToNeck" Solver="CCDX" Root="Bip01 Spine" EndEffector="Bip01 Neck" fStepSize="10" fThreshold="10" nMaxInteration="15"/>
    <IK Handle="RGAlign1" Solver="CCD5" J0="Bip01 R Thigh" J1="Bip01 R Calf" J2="Bip01 R Foot" J3="Bip01 R Heel" J4="Bip01 R Toe0" />
    <IK Handle="LGAlign1" Solver="CCD5" J0="Bip01 L Thigh" J1="Bip01 L Calf" J2="Bip01 L Foot" J3="Bip01 L Heel" J4="Bip01 L Toe0" />
  </LimbIK_Definition>

  <Animation_Driven_IK_Targets>
 		<ADIKTarget Handle="LftArm01" Target="Bip01 Chin_IKTarget" Weight="Bip01 Chin_IKBlend"/>
  </Animation_Driven_IK_Targets>

</IK_Definition>
