namespace TekkenOffsets
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
}
