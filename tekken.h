namespace Tekken
{
  namespace Offsets
  {
    enum Move
    {
      MoveNameKey = 0x0,
      AnimNameKey = 0x20,
      AnimAddr1 = 0x50,
      AnimAddr2 = 0x54,
      CancelList = 0x98,
      ExtraPropList = 0x138,
      StartPropList = 0x140,
      EndPropList = 0x148
    };

    enum Cancel
    {
      Command = 0x0,
      RequirementsList = 0x8,
      CancelExtradata = 0x10,
      WindowStart = 0x18,
      WindowEnd = 0x1C,
      TransitionFrame = 0x20,
      Move = 0x24,
      Option = 0x26
    };

    enum Moveset
    {
      ReactionsHeader = 0x168,
      ReactionsCount = 0x178,
      RequirementsHeader = 0x180,
      RequirementsCount = 0x188,
      HitConditionsHeader = 0x190,
      HitConditionsCount = 0x198,
      ProjectilesHeader = 0x1A0,
      ProjectilesCount = 0x1A8,
      PushbacksHeader = 0x1B0,
      PushbacksCount = 0x1B8,
      PushbackExtraDataHeader = 0x1C0,
      PushbackExtraDataCount = 0x1C8,
      CancelsHeader = 0x1D0,
      CancelsCount = 0x1D8,
      GroupCancelsHeader = 0x1E0,
      GroupCancelsCount = 0x1E8,
      CancelExtraDatasHeader = 0x1F0,
      CancelExtraDatasCount = 0x1F8,
      ExtraMovePropertiesHeader = 0x200,
      ExtraMovePropertiesCount = 0x208,
      MoveStartPropsHeader = 0x210,
      MoveStartPropsCount = 0x218,
      MoveEndPropsHeader = 0x220,
      MoveEndPropsCount = 0x228,
      MovesHeader = 0x230,
      MovesCount = 0x238,
      VoiceclipsHeader = 0x240,
      VoiceclipsCount = 0x248,
      InputSequencesHeader = 0x250,
      InputSequencesCount = 0x258,
      InputExtraDataHeader = 0x260,
      InputExtraDataCount = 0x268,
      ParryListHeader = 0x270,
      ParryListCount = 0x278,
      ThrowExtrasHeader = 0x280,
      ThrowExtrasCount = 0x288,
      ThrowsHeader = 0x290,
      ThrowsCount = 0x298,
      DialoguesHeader = 0x2A0,
      DialoguesCount = 0x2A8
    };

    enum ExtraProp
    {
      Type = 0x0,
      _0x4 = 0x4,
      RequirementAddr = 0x8,
      Prop = 0x10,
      Value = 0x14,
      Value2 = 0x18,
      Value3 = 0x1C,
      Value4 = 0x20,
      Value5 = 0x24
    };
  };

  namespace Sizes
  {
    enum Moveset
    {
      Pushback = 0x10,
      PushbackExtradata = 0x2,
      Requirement = 0x14,
      CancelExtradata = 0x4,
      Cancel = 0x28,
      ReactionList = 0x70,
      HitCondition = 0x18,
      ExtraMoveProperty = 0x28,
      OtherMoveProperty = 0x20,
      Move = 0x448,
      Voiceclip = 0xC,
      InputExtradata = 0x8,
      InputSequence = 0x10,
      Projectile = 0xD8,
      ThrowExtra = 0xC,
      Throw = 0x10,
      ParryList = 0x4,
      DialogueManager = 0x18
    };
  }

  std::string ENC_SIG_BYTES = "48 89 5C 24 08 57 48 83 EC 20 48 8B 59 08 48 8B 39 48 8B D3 48 8B CF E8 ?? ?? ?? ?? 48 3B C7 0F 85 ?? ?? ?? ?? 48 83 F7 1D 40 0F B6 C7 24 1F 76 20 0F B6 C8 0F 1F 40 00 0F 1F 84 00 00 00 00 00 48 8B C3 48 C1 E8 3F 48 8D 1C 58 48 83 E9 01 75 EF F3 0F 10 0D ?? ?? ?? ?? F3 0F 10 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? F3 0F 10 0D ?? ?? ?? ?? 33 C0 0F 2F C1 72 16 F3 0F 5C C1 0F 2F C1 73 0D 48 B9 00 00 00 00 00 00 00 80 48 8B C1 48 83 E3 E0 33 D2 48 33 DF 48 C1 E3 20 F3 48 0F 2C C8 48 03 C8 48 8B C3 48 F7 F1 48 8B 5C 24 30 48 83 C4 20 5F C3 48 8B 5C 24 30 33 C0 48 83 C4 20 5F C3";

  std::string HUD_NAME_SIG_BYTES = "48 8B C8 4C 8B F0 E8 ?? ?? ?? ?? 84 C0 74 31 40 0F B6 CE";
  std::string HUD_ICON_SIG_BYTES = "48 8B C8 4C 8B F8 E8 ?? ?? ?? ?? 84 C0 74 52 80 BD 8A 00";
  std::string MOVSET_OFFSET_SIG_BYTES = "48 89 91 ?? ?? ?? 00 4C 8B D9 48 89 91 ?? ?? ?? 00 48 8B DA 48 89 91 ?? ?? ?? 00 48 89 91 ?? ?? ?? 00 0F B7 02 89 81 ?? ?? ?? 00 B8 01 80 00 80";
}
