package action_script
{
   import flash.display.MovieClip;
   import flash.events.Event;
   import flash.events.KeyboardEvent;
   import flash.external.ExternalInterface;
   import flash.ui.Keyboard;
   import flash.utils.getTimer;
   
   public class ItemList
   {
      
      internal static const ButtonMax:int = 11;
      
      internal static const ItemNumMax:int = 99;
      
      public static const ReceiveType_ModeType:int = 0;
      
      public static const ReceiveType_RequestEnd:int = 1;
      
      public static const ReceiveType_RequestResume:int = 2;
      
      public static const ReceiveType_Zeny:int = 3;
      
      public static const ReceiveType_ListType:int = 4;
      
      public static const ReceiveType_ListElemMax:int = 5;
      
      public static const ReceiveType_ListSelectList:int = 6;
      
      public static const ReceiveType_ListButtonStart:int = 7;
      
      public static const ReceiveType_ListNameStart:int = 8;
      
      public static const ReceiveType_ListNameEnd:int = ReceiveType_ListNameStart + ButtonMax - 1;
      
      public static const ReceiveType_ListNumStart:int = ReceiveType_ListNameEnd + 1;
      
      public static const ReceiveType_ListNumEnd:int = ReceiveType_ListNumStart + ButtonMax - 1;
      
      public static const ReceiveType_ListPriceStart:int = ReceiveType_ListNumEnd + 1;
      
      public static const ReceiveType_ListPriceEnd:int = ReceiveType_ListPriceStart + ButtonMax - 1;
      
      public static const ReceiveType_ListRareStart:int = ReceiveType_ListPriceEnd + 1;
      
      public static const ReceiveType_ListRareEnd:int = ReceiveType_ListRareStart + ButtonMax - 1;
      
      public static const ReceiveType_ListIconStart:int = ReceiveType_ListRareEnd + 1;
      
      public static const ReceiveType_ListIconEnd:int = ReceiveType_ListIconStart + ButtonMax - 1;
      
      public static const ReceiveType_ListValidStart:int = ReceiveType_ListIconEnd + 1;
      
      public static const ReceiveType_ListValidEnd:int = ReceiveType_ListValidStart + ButtonMax - 1;
      
      public static const ReceiveType_ListNewStart:int = ReceiveType_ListValidEnd + 1;
      
      public static const ReceiveType_ListNewEnd:int = ReceiveType_ListNewStart + ButtonMax - 1;
      
      public static const ReceiveType_ListCheckStart:int = ReceiveType_ListNewEnd + 1;
      
      public static const ReceiveType_ListCheckEnd:int = ReceiveType_ListCheckStart + ButtonMax - 1;
      
      public static const ReceiveType_Material1Name:int = ReceiveType_ListCheckEnd + 1;
      
      public static const ReceiveType_Material2Name:int = ReceiveType_Material1Name + 1;
      
      public static const ReceiveType_Material3Name:int = ReceiveType_Material2Name + 1;
      
      public static const ReceiveType_Material1Flag:int = ReceiveType_Material3Name + 1;
      
      public static const ReceiveType_Material2Flag:int = ReceiveType_Material1Flag + 1;
      
      public static const ReceiveType_Material3Flag:int = ReceiveType_Material2Flag + 1;
      
      public static const ReceiveType_MaterialMixReset:int = ReceiveType_Material3Flag + 1;
      
      public static const ReceiveType_AbilityBallNum:int = ReceiveType_MaterialMixReset + 1;
      
      public static const ReceiveType_RequestEndFood:int = ReceiveType_AbilityBallNum + 1;
      
      public static const ReceiveType_PayType:int = ReceiveType_RequestEndFood + 1;
      
      public static const ReceiveType_PreModeIndex:int = ReceiveType_PayType + 1;
      
      public static const ReceiveType_InputEnableFood:int = ReceiveType_PreModeIndex + 1;
      
      public static const ReceiveType_EnableListPause:int = ReceiveType_InputEnableFood + 1;
      
      public static const ReceiveType_SortTypeName:int = ReceiveType_EnableListPause + 1;
      
      public static const ReceiveType_SortTypeArrow:int = ReceiveType_SortTypeName + 1;
      
      public static const ReceiveType_Num:int = ReceiveType_SortTypeArrow + 1;
      
      public static const SendType_RequestList:int = 0;
      
      public static const SendType_RequestSort:int = 1;
      
      public static const SendType_RequestMaterial:int = 2;
      
      public static const SendType_MaterialIndex:int = 3;
      
      public static const SendType_SelectIndex:int = 4;
      
      public static const SendType_ListOutComp:int = 5;
      
      public static const SendType_DecideIndex:int = 6;
      
      public static const SendType_DecideButtonPosition:int = 7;
      
      public static const SendType_ListOutStart:int = 8;
      
      public static const SendType_ListFoldNewFlag:int = 9;
      
      public static const SendType_MixMaterialResultName:int = 10;
      
      public static const SendType_ListPosition:int = 11;
      
      public static const SendType_PauseList:int = 12;
      
      public static const ModeType_LobbyItem:int = 0;
      
      public static const ModeType_LobbyCostume:int = 1;
      
      public static const ModeType_LobbySkill:int = 2;
      
      public static const ModeType_LobbyDictionary:int = 3;
      
      public static const ModeType_Battle:int = 4;
      
      public static const ModeType_ShopItem:int = 5;
      
      public static const ModeType_ShopCostume:int = 6;
      
      public static const ModeType_ShopAccessory:int = 7;
      
      public static const ModeType_ShopSkill:int = 8;
      
      public static const ModeType_MixMaterial:int = 9;
      
      public static const ModeType_MixRecipe:int = 10;
      
      public static const ModeType_MixAbility:int = 11;
      
      public static const ModeType_MixAbilityAddition:int = 12;
      
      public static const ModeType_Present:int = 13;
      
      public static const ModeType_CustomizeItem:int = 14;
      
      public static const ModeType_CustomizeSkill:int = 15;
      
      public static const ModeType_CustomizeCostume:int = 16;
      
      public static const ModeType_FoodSelect:int = 17;
      
      public static const ModeType_EventShop:int = 18;
      
      public static const ModeType_FloatPetCustomize:int = 19;
      
      public static const ModeType_DeliveryQuest:int = 20;
      
      public static const ModeType_Num:int = 21;
      
      public static const ListType_Select:int = 0;
      
      public static const ListType_Buy:int = 1;
      
      public static const ListType_Sell:int = 2;
      
      public static const ModeIndex_1:int = 0;
      
      public static const ModeIndex_2:int = 1;
      
      public static const ModeIndex_3:int = 2;
      
      public static const ModeIndex_4:int = 3;
      
      public static const ModeIndex_5:int = 4;
      
      public static const ModeIndex_6:int = 5;
      
      public static const SkillType_SPA:int = 0;
      
      public static const SkillType_ULT:int = 1;
      
      public static const SkillType_ESC:int = 2;
      
      public static const SkillType_MET:int = 5;
      
      public static const FilterType_Costume:int = 0;
      
      public static const FilterType_Battle:int = 1;
      
      public static const FilterType_Material:int = 2;
      
      public static const FilterType_Food:int = 3;
      
      public static const FilterType_Gift:int = 4;
      
      public static const FilterType_Treasures:int = 5;
      
      public static const FilterType_Merchandise:int = 6;
      
      public static const FilterType_Skill:int = 7;
      
      public static const FilterType_CostumeBust:int = 8;
      
      public static const FilterType_CostumePants:int = 9;
      
      public static const FilterType_CostumeGloves:int = 10;
      
      public static const FilterType_CostumeShoes:int = 11;
      
      public static const FilterType_CostumeAccessory:int = 12;
      
      public static const FilterType_CostumeTalisman:int = 13;
      
      public static const FilterType_CostumeAbilityBall:int = 14;
      
      public static const FilterType_BattleRecoveryLife:int = 15;
      
      public static const FilterType_BattleRecoveryEnergy:int = 16;
      
      public static const FilterType_BattleRecoveryGuard:int = 17;
      
      public static const FilterType_BattleRecoveryPoison:int = 18;
      
      public static const FilterType_BattleStatus:int = 19;
      
      public static const FilterType_BattleRevival:int = 20;
      
      public static const FilterType_BattleSummonAvatar:int = 21;
      
      public static const FilterType_SkillSpa:int = 22;
      
      public static const FilterType_SkillUlt:int = 23;
      
      public static const FilterType_SkillMet:int = 24;
      
      public static const FilterType_SkillEsc:int = 25;
      
      public static const FilterType_SkillGet:int = 26;
      
      public static const FilterType_SkillBlow:int = 27;
      
      public static const FilterType_SkillQigong:int = 28;
      
      public static const FilterType_SkillTransform:int = 29;
      
      public static const FilterType_SkillStreng:int = 30;
      
      public static const FilterType_SkillOther:int = 31;
      
      public static const FilterType_SkillXBlow:int = 32;
      
      public static const FilterType_SkillXQigong:int = 33;
      
      public static const FilterType_SkillXStreng:int = 34;
      
      public static const FilterType_SkillXOther:int = 35;
      
      public static const FilterType_SkillPRBBlow:int = 36;
      
      public static const FilterType_SkillPRBQigong:int = 37;
      
      public static const FilterType_SkillPRBTransform:int = 38;
      
      public static const FilterType_SkillPRBStreng:int = 39;
      
      public static const FilterType_SkillPRBOther:int = 40;
      
      public static const FilterType_1LessRarity:int = 41;
      
      public static const FilterType_2LessRarity:int = 42;
      
      public static const FilterType_3LessRarity:int = 43;
      
      public static const FilterType_4LessRarity:int = 44;
      
      public static const FilterType_5LessRarity:int = 45;
      
      public static const FilterType_Pet:int = 46;
      
      public static const FilterType_All:int = 47;
      
      public static const SortType_Get:int = 0;
      
      public static const SortType_Num:int = 1;
      
      public static const SortType_Rare:int = 2;
      
      public static const SortType_Category_Type_ID:int = 3;
      
      public static const SortType_SkillBlow:int = 4;
      
      public static const SortType_SkillQigong:int = 5;
      
      public static const SortType_SkillTransform:int = 6;
      
      public static const SortType_SkillStreng:int = 7;
      
      public static const SortType_SkillOther:int = 8;
      
      public static const SortType_NoSort:int = 9;
      
      public static const PauseType_StartPopup:int = 0;
      
      public static const PauseType_ChangeArrow:int = 1;
      
      public static const PayType_Zeny:int = 0;
      
      public static const PayType_Medal:int = 1;
      
      public static const PayType_STPMedal:int = 2;
      
      public static const PayType_BMedal:int = 3;
      
      private static const StringList:Object = {
         "UnitZeny":"ITM_SRT_00_001",
         "HeaderZeny":"SHP_CMN_00_000",
         "HeaderNumber":"ITM_STR_00_000",
         "HeaderPrice1":"SHP_STR_00_002",
         "HeaderPrice2":"SHP_STR_00_003",
         "HeaderPrice3":"SHP_STR_00_005",
         "HeaderPrice4":"SHP_STR_00_006",
         "HeaderPriceMix":"MIX_DLG_01_000",
         "FilterAll":"ITM_SRT_00_000",
         "FilterCostume":"ITM_SRT_00_005",
         "FilterBattle":"ITM_SRT_00_006",
         "FilterMaterial":"ITM_SRT_00_007",
         "FilterFood":"ITM_SRT_00_018",
         "FilterGift":"ITM_SRT_00_020",
         "FilterTreasures":"ITM_SRT_00_008",
         "FilterBattleRecoveryLife":"ITM_SRT_00_009",
         "FilterBattleRecoveryEnergy":"ITM_SRT_00_010",
         "FilterBattleRecoveryGuard":"ITM_SRT_00_013",
         "FilterBattleRecoveryPoison":"ITM_SRT_00_014",
         "FilterBattleStatus":"ITM_SRT_00_011",
         "FilterBattleRevival":"ITM_SRT_00_012",
         "FilterBattleSummonAvatar":"ITM_SRT_00_015",
         "FilterCostumeBust":"ITM_SRT_02_002",
         "FilterCostumePants":"ITM_SRT_02_003",
         "FilterCostumeGloves":"ITM_SRT_02_004",
         "FilterCostumeShoes":"ITM_SRT_02_005",
         "FilterCostumeAccessory":"ITM_SRT_02_007",
         "FilterCostumeTalisman":"ITM_SRT_02_006",
         "FilterCustumeAbilityBall":"ITM_SRT_00_017",
         "FilterSkillSpa":"ITM_SRT_01_003",
         "FilterSkillUlt":"ITM_SRT_01_002",
         "FilterSkillMet":"ITM_SRT_01_005",
         "FilterSkillEsc":"ITM_SRT_01_004",
         "FilterSkillBlow":"ITM_STR_02_004",
         "FilterSkillQigong":"ITM_STR_02_005",
         "FilterSkillTransform":"ITM_STR_02_006",
         "FilterSkillStreng":"ITM_STR_02_007",
         "FilterSkillOther":"ITM_SRT_00_016",
         "FilterSkillXBlow":"ITM_STR_02_020",
         "FilterSkillXQigong":"ITM_STR_02_021",
         "FilterSkillXStreng":"ITM_STR_02_022",
         "FilterSkillXOther":"ITM_STR_02_023",
         "FilterSkillPRBBlow":"ITM_STR_02_024",
         "FilterSkillPRBQigong":"ITM_STR_02_025",
         "FilterSkillPRBTransform":"ITM_STR_02_026",
         "FilterSkillPRBStreng":"ITM_STR_02_027",
         "FilterSkillPRBOther":"ITM_STR_02_028",
         "FilterPet":"ITM_SRT_02_008",
         "SortGet":"ITM_SRT_00_002",
         "SortNum":"ITM_SRT_00_003",
         "SortRare":"ITM_SRT_00_004",
         "Invalid":null
      };
      
      private static const FilterTable:Object = {
         "LobbyItem":[[FilterType_Battle,StringList.FilterBattle],[FilterType_Material,StringList.FilterMaterial],[FilterType_Food,StringList.FilterFood],[FilterType_Gift,StringList.FilterGift],[FilterType_Treasures,StringList.FilterTreasures]],
         "LobbyCostume":[[FilterType_Costume,StringList.FilterAll],[FilterType_CostumeBust,StringList.FilterCostumeBust],[FilterType_CostumePants,StringList.FilterCostumePants],[FilterType_CostumeGloves,StringList.FilterCostumeGloves],[FilterType_CostumeShoes,StringList.FilterCostumeShoes],[FilterType_CostumeAccessory,StringList.FilterCostumeAccessory],[FilterType_CostumeAbilityBall,StringList.FilterCustumeAbilityBall],[FilterType_CostumeTalisman,StringList.FilterCostumeTalisman]],
         "LobbySkill":[[FilterType_Skill,StringList.FilterAll],[FilterType_SkillSpa,StringList.FilterSkillSpa],[FilterType_SkillUlt,StringList.FilterSkillUlt],[FilterType_SkillMet,StringList.FilterSkillMet],[FilterType_SkillEsc,StringList.FilterSkillEsc]],
         "LobbyDictionary":[[FilterType_SkillSpa,StringList.FilterSkillSpa],[FilterType_SkillUlt,StringList.FilterSkillUlt],[FilterType_SkillMet,StringList.FilterSkillMet],[FilterType_SkillEsc,StringList.FilterSkillEsc],[FilterType_CostumeTalisman,StringList.FilterCostumeTalisman],[FilterType_CostumeBust,StringList.FilterCostumeBust],[FilterType_CostumePants,StringList.FilterCostumePants],[FilterType_CostumeGloves,StringList.FilterCostumeGloves],[FilterType_CostumeShoes,StringList.FilterCostumeShoes],[FilterType_CostumeAccessory,StringList.FilterCostumeAccessory]],
         "Battle":[[FilterType_BattleRecoveryLife,StringList.FilterBattleRecoveryLife],[FilterType_BattleRecoveryEnergy,StringList.FilterBattleRecoveryEnergy],[FilterType_BattleRecoveryGuard,StringList.FilterBattleRecoveryGuard],[FilterType_BattleRecoveryPoison,StringList.FilterBattleRecoveryPoison],[FilterType_BattleStatus,StringList.FilterBattleStatus],[FilterType_BattleRevival,StringList.FilterBattleRevival]],
         "ShopItem":[[FilterType_Merchandise,StringList.FilterAll],[FilterType_BattleRecoveryLife,StringList.FilterBattleRecoveryLife],[FilterType_BattleRecoveryEnergy,StringList.FilterBattleRecoveryEnergy],[FilterType_BattleRecoveryGuard,StringList.FilterBattleRecoveryGuard],[FilterType_BattleRecoveryPoison,StringList.FilterBattleRecoveryPoison],[FilterType_BattleStatus,StringList.FilterBattleStatus],[FilterType_BattleRevival,StringList.FilterBattleRevival],[FilterType_Material,StringList.FilterMaterial],[FilterType_Food,StringList.FilterFood],[FilterType_CostumeTalisman,StringList.FilterCostumeTalisman]],
         "ShopTalisman":[[FilterType_CostumeTalisman,StringList.FilterCostumeTalisman]],
         "ShopAccessory":[[FilterType_CostumeAccessory,StringList.FilterCostumeAccessory]],
         "ShopCostume":[[FilterType_Costume,StringList.FilterAll],[FilterType_CostumeBust,StringList.FilterCostumeBust],[FilterType_CostumePants,StringList.FilterCostumePants],[FilterType_CostumeGloves,StringList.FilterCostumeGloves],[FilterType_CostumeShoes,StringList.FilterCostumeShoes]],
         "ShopGift":[[FilterType_Gift,StringList.FilterGift]],
         "ShopPet":[[FilterType_Pet,StringList.FilterPet]],
         "ShopSkill":[[FilterType_Skill,StringList.FilterAll],[FilterType_SkillSpa,StringList.FilterSkillSpa],[FilterType_SkillUlt,StringList.FilterSkillUlt],[FilterType_SkillEsc,StringList.FilterSkillEsc]],
         "ShopSell":[[FilterType_Costume,StringList.FilterCostume],[FilterType_Battle,StringList.FilterBattle],[FilterType_Material,StringList.FilterMaterial],[FilterType_Food,StringList.FilterFood],[FilterType_Gift,StringList.FilterGift]],
         "MixMaterial":[[FilterType_All,StringList.FilterAll],[FilterType_Costume,StringList.FilterCostumeTalisman],[FilterType_Material,StringList.FilterMaterial]],
         "MixRecipe":[[FilterType_All,StringList.FilterAll],[FilterType_Costume,StringList.FilterCostumeTalisman],[FilterType_Battle,StringList.FilterBattle],[FilterType_Material,StringList.FilterMaterial]],
         "MixAbility":[[FilterType_Costume,StringList.FilterAll],[FilterType_CostumeBust,StringList.FilterCostumeBust],[FilterType_CostumePants,StringList.FilterCostumePants],[FilterType_CostumeGloves,StringList.FilterCostumeGloves],[FilterType_CostumeShoes,StringList.FilterCostumeShoes]],
         "Present":[[FilterType_CostumeBust,StringList.FilterCostumeBust],[FilterType_CostumePants,StringList.FilterCostumePants],[FilterType_CostumeGloves,StringList.FilterCostumeGloves],[FilterType_CostumeShoes,StringList.FilterCostumeShoes],[FilterType_CostumeAccessory,StringList.FilterCostumeAccessory],[FilterType_CostumeTalisman,StringList.FilterCostumeTalisman],[FilterType_Battle,StringList.FilterBattle],[FilterType_Material,StringList.FilterMaterial],[FilterType_Food,StringList.FilterFood]],
         "CustomizeItem":[[FilterType_Battle,StringList.FilterAll],[FilterType_BattleRecoveryLife,StringList.FilterBattleRecoveryLife],[FilterType_BattleRecoveryEnergy,StringList.FilterBattleRecoveryEnergy],[FilterType_BattleRecoveryGuard,StringList.FilterBattleRecoveryGuard],[FilterType_BattleRecoveryPoison,StringList.FilterBattleRecoveryPoison],[FilterType_BattleStatus,StringList.FilterBattleStatus],[FilterType_BattleRevival,StringList.FilterBattleRevival]],
         "CustomizeSkill":[[FilterType_Skill,StringList.FilterAll],[FilterType_SkillBlow,StringList.FilterSkillBlow],[FilterType_SkillQigong,StringList.FilterSkillQigong],[FilterType_SkillTransform,StringList.FilterSkillTransform],[FilterType_SkillStreng,StringList.FilterSkillStreng],[FilterType_SkillOther,StringList.FilterSkillOther],[FilterType_SkillXBlow,StringList.FilterSkillXBlow],[FilterType_SkillXQigong,StringList.FilterSkillXQigong],[FilterType_SkillXStreng,StringList.FilterSkillXStreng],[FilterType_SkillXOther,StringList.FilterSkillXOther],[FilterType_SkillPRBBlow,StringList.FilterSkillPRBBlow],[FilterType_SkillPRBQigong,StringList.FilterSkillPRBQigong],[FilterType_SkillPRBTransform,StringList.FilterSkillPRBTransform],[FilterType_SkillPRBStreng,StringList.FilterSkillPRBStreng],[FilterType_SkillPRBOther,StringList.FilterSkillPRBOther]],
         "CustomizeCostume":[[FilterType_Costume,StringList.FilterAll],[FilterType_CostumeBust,StringList.FilterCostumeBust],[FilterType_CostumePants,StringList.FilterCostumePants],[FilterType_CostumeGloves,StringList.FilterCostumeGloves],[FilterType_CostumeShoes,StringList.FilterCostumeShoes],[FilterType_CostumeAccessory,StringList.FilterCostumeAccessory],[FilterType_CostumeTalisman,StringList.FilterCostumeTalisman]],
         "FoodSelect":[[FilterType_Food,StringList.FilterFood]],
         "CostomizeSkillSPA":[[FilterType_Skill,StringList.FilterAll],[FilterType_SkillBlow,StringList.FilterSkillBlow],[FilterType_SkillQigong,StringList.FilterSkillQigong],[FilterType_SkillStreng,StringList.FilterSkillStreng],[FilterType_SkillOther,StringList.FilterSkillOther],[FilterType_SkillPRBBlow,StringList.FilterSkillPRBBlow],[FilterType_SkillPRBQigong,StringList.FilterSkillPRBQigong]],
         "CostomizeSkillULT":[[FilterType_Skill,StringList.FilterAll],[FilterType_SkillBlow,StringList.FilterSkillBlow],[FilterType_SkillQigong,StringList.FilterSkillQigong],[FilterType_SkillOther,StringList.FilterSkillOther],[FilterType_SkillXBlow,StringList.FilterSkillXBlow],[FilterType_SkillXQigong,StringList.FilterSkillXQigong],[FilterType_SkillXOther,StringList.FilterSkillXOther],[FilterType_SkillPRBQigong,StringList.FilterSkillPRBQigong]],
         "CostomizeSkillMET":[[FilterType_Skill,StringList.FilterAll],[FilterType_SkillTransform,StringList.FilterSkillTransform]],
         "CostomizeSkillESC":[[FilterType_Skill,StringList.FilterAll],[FilterType_SkillBlow,StringList.FilterSkillBlow],[FilterType_SkillQigong,StringList.FilterSkillQigong],[FilterType_SkillStreng,StringList.FilterSkillStreng],[FilterType_SkillOther,StringList.FilterSkillOther],[FilterType_SkillPRBQigong,StringList.FilterSkillPRBQigong]],
         "DeliveryQuest":[[FilterType_Battle,StringList.FilterBattle],[FilterType_Material,StringList.FilterMaterial],[FilterType_Food,StringList.FilterFood]]
      };
      
      private static const SortTable:Object = {
         "LobbyItem":[SortType_Category_Type_ID],
         "LobbyCostume":[SortType_Get],
         "LobbySkill":[SortType_Category_Type_ID],
         "LobbyDictionary":[SortType_Category_Type_ID],
         "Battle":[SortType_Category_Type_ID],
         "ShopItem":[SortType_NoSort],
         "ShopCostume":[SortType_NoSort],
         "ShopSkill":[SortType_Category_Type_ID],
         "ShopSell":[SortType_Get],
         "MixMaterial":[SortType_Category_Type_ID],
         "MixRecipe":[SortType_Category_Type_ID],
         "MixAbility":[SortType_Category_Type_ID],
         "Present":[SortType_Category_Type_ID],
         "CustomizeItem":[SortType_Category_Type_ID],
         "CustomizeSkill":[SortType_Category_Type_ID],
         "CustomizeCostume":[SortType_Category_Type_ID],
         "FoodSelect":[SortType_Category_Type_ID]
      };
      
      private static const ModeInfoTable:Array = [{
         "ShowZeny":false,
         "LayoutType":-1,
         "ListNum":true,
         "ListPrice":false,
         "ListRare":true,
         "ItemElemMax":ButtonMax,
         "FilterTable":FilterTable.LobbyItem
      },{
         "ShowZeny":false,
         "LayoutType":-1,
         "ListNum":true,
         "ListPrice":false,
         "ListRare":true,
         "ItemElemMax":ButtonMax,
         "FilterTable":FilterTable.LobbyCostume
      },{
         "ShowZeny":false,
         "LayoutType":4,
         "ListNum":false,
         "ListPrice":false,
         "ListRare":true,
         "ItemElemMax":ButtonMax,
         "FilterTable":FilterTable.LobbySkill
      },{
         "ShowZeny":false,
         "LayoutType":4,
         "ListNum":false,
         "ListPrice":false,
         "ListRare":true,
         "ItemElemMax":ButtonMax,
         "FilterTable":FilterTable.LobbyDictionary
      },{
         "ShowZeny":false,
         "LayoutType":-1,
         "ListNum":true,
         "ListPrice":false,
         "ListRare":true,
         "ItemElemMax":ButtonMax,
         "FilterTable":FilterTable.Battle,
         "FilterTable2":null
      },{
         "ShowZeny":true,
         "LayoutType":-1,
         "ListNum":true,
         "ListPrice":true,
         "ListRare":true,
         "ItemElemMax":ButtonMax,
         "FilterTable":FilterTable.ShopItem,
         "FilterTable2":FilterTable.ShopTalisman
      },{
         "ShowZeny":true,
         "LayoutType":-1,
         "ListNum":true,
         "ListPrice":true,
         "ListRare":true,
         "ItemElemMax":ButtonMax,
         "FilterTable":FilterTable.ShopCostume,
         "FilterTable2":null
      },{
         "ShowZeny":true,
         "LayoutType":-1,
         "ListNum":true,
         "ListPrice":true,
         "ListRare":true,
         "ItemElemMax":ButtonMax,
         "FilterTable":FilterTable.ShopAccessory,
         "FilterTable2":FilterTable.ShopAccessory
      },{
         "ShowZeny":true,
         "LayoutType":0,
         "ListNum":false,
         "ListPrice":true,
         "ListRare":false,
         "ItemElemMax":ButtonMax,
         "FilterTable":FilterTable.ShopSkill,
         "FilterTable2":null
      },{
         "ShowZeny":true,
         "LayoutType":-1,
         "ListNum":true,
         "ListPrice":true,
         "ListRare":true,
         "ItemElemMax":ButtonMax - 3,
         "FilterTable":FilterTable.MixMaterial
      },{
         "ShowZeny":true,
         "LayoutType":1,
         "ListNum":true,
         "ListPrice":true,
         "ListRare":true,
         "ItemElemMax":ButtonMax - 3,
         "FilterTable":FilterTable.MixRecipe
      },{
         "ShowZeny":true,
         "LayoutType":-1,
         "ListNum":true,
         "ListPrice":true,
         "ListRare":true,
         "ItemElemMax":ButtonMax - 3,
         "FilterTable":FilterTable.MixAbility
      },{
         "ShowZeny":false,
         "LayoutType":-1,
         "ListNum":true,
         "ListPrice":false,
         "ListRare":true,
         "ItemElemMax":ButtonMax
      },{
         "ShowZeny":false,
         "LayoutType":-1,
         "ListNum":true,
         "ListPrice":false,
         "ListRare":true,
         "ItemElemMax":ButtonMax,
         "FilterTable":FilterTable.Present
      },{
         "ShowZeny":false,
         "LayoutType":-1,
         "ListNum":true,
         "ListPrice":false,
         "ListRare":true,
         "ItemElemMax":ButtonMax - 1,
         "FilterTable":FilterTable.CustomizeItem
      },{
         "ShowZeny":false,
         "LayoutType":2,
         "ListNum":false,
         "ListPrice":false,
         "ListRare":false,
         "ItemElemMax":ButtonMax,
         "FilterTable":FilterTable.CustomizeSkill
      },{
         "ShowZeny":false,
         "LayoutType":3,
         "ListNum":false,
         "ListPrice":false,
         "ListRare":true,
         "ItemElemMax":ButtonMax
      },{
         "ShowZeny":false,
         "LayoutType":3,
         "ListNum":true,
         "ListPrice":false,
         "ListRare":true,
         "ItemElemMax":ButtonMax,
         "FilterTable":FilterTable.FoodSelect
      },{
         "ShowZeny":true,
         "LayoutType":-1,
         "ListNum":true,
         "ListPrice":true,
         "ListRare":false,
         "ItemElemMax":ButtonMax
      },{
         "ShowZeny":false,
         "LayoutType":5,
         "ListNum":false,
         "ListPrice":false,
         "ListRare":true,
         "ItemElemMax":ButtonMax
      },{
         "ShowZeny":false,
         "LayoutType":3,
         "ListNum":true,
         "ListPrice":false,
         "ListRare":true,
         "ItemElemMax":ButtonMax,
         "FilterTable":FilterTable.DeliveryQuest
      }];
      
      private static const Hide_RareIcon:String = "-999";
      
      private static const Hide_KindIcon:String = "-999";
      
      private static const Hide_NumberPlate:int = -999;
      
      private static const CursolMoveLimit:int = 3;
      
      private var m_callback:Callback = null;
      
      private var m_list_box:ListBox = null;
      
      private var m_button_list:ButtonList = null;
      
      private var m_filter_button:LoopButton = null;
      
      private var m_material_box:MaterialBox = null;
      
      private var m_timeline:MovieClip = null;
      
      private var m_elapsed_time:Number = 0;
      
      private var m_oneshot_newflag:Boolean = false;
      
      private var m_material_mix_counter:int = 0;
      
      private var m_menuType_func:Function = null;
      
      private var m_sort_type:int = 0;
      
      private var m_input_enable:Boolean = true;
      
      private var m_enable_list_pause:Boolean = false;
      
      private var m_status_is_list_pause:Boolean = false;
      
      private var m_restore_pos_change_page_init_list:Boolean = false;
      
      private var m_iconsUpdater:IconsUpdater = null;
      
      // ItemSearch: the game writes this tag into ReceiveType_RequestResume
      // after it changed the shown rows (the game itself only writes 0 or
      // small numbers there): bits 24-30 the tag, bits 14-23 how many
      // favorites there are, bit 13 set when they follow one leading row
      // ("None"), bits 0-12 the row to select. The open list then asks for
      // its rows again with that row selected.
      public static const SearchRefreshTag:int = 0x5E000000;

      public static const SearchRefreshTagMask:int = 0x7F000000;

      public static const SearchFavoriteShift:int = 14;

      public static const SearchFavoriteMask:int = 0x3FF;

      public static const SearchFavoriteAfterFirst:int = 0x2000;

      public static const SearchRowMask:int = 0x1FFF;

      // ItemSearch: the m_favorite_count entries from m_favorite_first on
      // are favorites and get a gold star after their name.
      private var m_favorite_first:int = 0;

      private var m_favorite_count:int = 0;

      // ItemSearch: true while the list takes keys (checkKey listening).
      // updateNewFlagNotify keeps running after endList, so without this a
      // refresh meant for the next opening was taken by the closed list.
      private var m_search_ready:Boolean = false;
      
      public function ItemList(param1:Function)
      {
         super();
         this.m_callback = new Callback(ReceiveType_Num);
         this.m_list_box = new ListBox(ButtonMax);
         this.m_button_list = new ButtonList(ButtonMax);
         this.m_filter_button = null;
         this.m_material_box = null;
         this.m_timeline = null;
         this.m_elapsed_time = 0;
         this.m_oneshot_newflag = false;
         this.m_material_mix_counter = 0;
         this.m_menuType_func = param1;
         this.m_sort_type = 0;
         this.m_input_enable = true;
         this.m_enable_list_pause = false;
         this.m_status_is_list_pause = false;
         this.m_iconsUpdater = new IconsUpdater();
         try
         {
            ExternalInterface.addCallback("UpdateIcons",this.UpdateIcons);
         }
         catch(e:Error)
         {
         }
      }
      
      public function Initialize(param1:MovieClip) : void
      {
         this.m_timeline = param1;
         this.m_timeline.visible = false;
         this.m_timeline.item_list.visible = false;
         this.m_timeline.custom1.visible = false;
         this.m_timeline.custom1b.visible = false;
         this.m_timeline.custom2.visible = false;
         this.m_timeline.custom3.visible = false;
         this.m_timeline.kyoka.visible = false;
         this.m_timeline.item_list.nest.sort.visible = false;
         this.m_timeline.custom1b.nest.sort.visible = false;
         this.m_timeline.custom2.nest.sort.visible = false;
         this.m_list_box.ClearItem();
         this.m_button_list.ClearButton();
         if(this.m_material_box)
         {
            this.m_material_box.Destroy();
            this.m_material_box = null;
         }
         if(this.m_filter_button)
         {
            this.m_filter_button.Destroy();
            this.m_filter_button = null;
         }
         var _loc2_:int = 0;
         while(_loc2_ < ReceiveType_Num)
         {
            this.m_callback.SetUserDataValidFlag(_loc2_,false);
            _loc2_++;
         }
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitStart);
         ExternalInterface.addCallback("requestListEnd",this.requestListEnd);
         var _loc3_:MovieClip = this.m_timeline.item_list.nest.spin._CMN_M_B_mc_l1;
         var _loc4_:MovieClip = this.m_timeline.item_list.nest.spin._CMN_M_B_mc_r1;
         this.m_iconsUpdater.AddButton(_loc3_.getChildAt(3) as MovieClip,_loc3_.getChildAt(2) as MovieClip,_loc3_.getChildAt(1) as MovieClip,_loc3_.getChildAt(0) as MovieClip);
         this.m_iconsUpdater.AddButton(_loc4_.getChildAt(3) as MovieClip,_loc4_.getChildAt(2) as MovieClip,_loc4_.getChildAt(1) as MovieClip,_loc4_.getChildAt(0) as MovieClip);
         var _loc5_:MovieClip = this.m_timeline.custom1.nest.spin._CMN_M_B_mc_l1;
         var _loc6_:MovieClip = this.m_timeline.custom1.nest.spin._CMN_M_B_mc_r1;
         this.m_iconsUpdater.AddButton(_loc5_.getChildAt(3) as MovieClip,_loc5_.getChildAt(2) as MovieClip,_loc5_.getChildAt(1) as MovieClip,_loc5_.getChildAt(0) as MovieClip);
         this.m_iconsUpdater.AddButton(_loc6_.getChildAt(3) as MovieClip,_loc6_.getChildAt(2) as MovieClip,_loc6_.getChildAt(1) as MovieClip,_loc6_.getChildAt(0) as MovieClip);
         var _loc7_:MovieClip = this.m_timeline.custom1b.nest.spin._CMN_M_B_mc_l1;
         var _loc8_:MovieClip = this.m_timeline.custom1b.nest.spin._CMN_M_B_mc_r1;
         this.m_iconsUpdater.AddButton(_loc7_.getChildAt(3) as MovieClip,_loc7_.getChildAt(2) as MovieClip,_loc7_.getChildAt(1) as MovieClip,_loc7_.getChildAt(0) as MovieClip);
         this.m_iconsUpdater.AddButton(_loc8_.getChildAt(3) as MovieClip,_loc8_.getChildAt(2) as MovieClip,_loc8_.getChildAt(1) as MovieClip,_loc8_.getChildAt(0) as MovieClip);
      }
      
      public function UpdateIcons(param1:int, param2:int, param3:int, param4:int, param5:int, param6:int) : *
      {
         this.m_iconsUpdater.updateButtons(param1,param2,param3,param4,param5,param6);
      }
      
      private function waitStart(param1:Event) : void
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ModeType))
         {
            return;
         }
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         if(_loc2_ == ModeType_MixAbility && !this.m_callback.GetUserDataValidFlag(ReceiveType_AbilityBallNum))
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStart);
         this.Start();
      }
      
      public function Start() : void
      {
         this.m_timeline.visible = true;
         this.startList();
      }
      
      private function waitRequestEnd(param1:Event) : void
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_RequestEnd))
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitRequestEnd);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStartList);
         this.endList();
      }
      
      private function waitRequestEndFood(param1:Event) : void
      {
         if(this.m_callback.GetUserDataValidFlag(ReceiveType_InputEnableFood))
         {
            this.m_input_enable = this.m_callback.GetUserDataInt(ReceiveType_InputEnableFood) == 1;
            this.m_callback.SetUserDataValidFlag(ReceiveType_InputEnableFood,false);
         }
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_RequestEndFood))
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitRequestEndFood);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStartList);
         this.endList();
      }
      
      private function endList(param1:Boolean = false) : void
      {
         var _loc2_:MovieClip = this.GetMovieClip();
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_search_ready = false;
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitResumeList);
         if(_loc2_.visible && _loc2_.currentLabel == "wait")
         {
            _loc2_.gotoAndPlay("end");
         }
         if(Boolean(this.m_timeline.kyoka.visible) && this.m_timeline.kyoka.currentLabel == "wait")
         {
            this.m_timeline.kyoka.gotoAndPlay("end");
         }
         this.m_callback.CallbackUserData("user",SendType_ListOutStart,param1 ? -1 : 1);
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitEnd);
      }
      
      private function waitEnd(param1:Event) : void
      {
         var _loc2_:MovieClip = this.GetMovieClip();
         if(_loc2_.visible && _loc2_.currentLabel != "end_comp")
         {
            return;
         }
         var _loc3_:Boolean = this.m_callback.GetUserDataValidFlag(ReceiveType_RequestEndFood);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEnd);
         var _loc4_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         if(this.isCancelExit() || _loc3_)
         {
            this.m_callback.CallbackExit();
            this.Initialize(this.m_timeline);
         }
         else
         {
            this.m_callback.SetUserDataValidFlag(ReceiveType_RequestEnd,false);
            this.m_callback.SetUserDataValidFlag(ReceiveType_ListType,false);
            this.m_callback.CallbackUserData("user",SendType_ListOutComp,1);
            this.startList();
         }
      }
      
      private function startList() : void
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ListType))
         {
            this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitStartList);
            this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitRequestEnd);
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitRequestEnd);
         this.m_favorite_count = 0;
         this.Setup_ListButtonLayout();
         this.m_timeline.item_list.visible = false;
         this.m_timeline.custom1.visible = false;
         this.m_timeline.custom1b.visible = false;
         this.m_timeline.custom2.visible = false;
         this.m_timeline.custom3.visible = false;
         var _loc1_:MovieClip = this.GetMovieClip();
         _loc1_.visible = true;
         _loc1_.gotoAndPlay("start");
         this.m_list_box.ClearItem();
         this.initListBase();
         this.requestList(true);
         this.updateSortInfo();
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         if(_loc2_ == ModeType_FoodSelect || _loc2_ == ModeType_DeliveryQuest)
         {
            this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitRequestEndFood);
         }
      }
      
      private function waitStartList(param1:Event) : void
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ListType))
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStartList);
         this.startList();
      }
      
      private function Setup_ListButtonLayout() : void
      {
         var _loc2_:int = 0;
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         if(_loc1_ == ModeType_EventShop)
         {
            _loc2_ = this.m_callback.GetUserDataInt(ReceiveType_PreModeIndex);
            if(_loc2_ == ModeIndex_1)
            {
               ModeInfoTable[_loc1_].LayoutType = 0;
               ModeInfoTable[_loc1_].ListRare = false;
               ModeInfoTable[_loc1_].ListNum = false;
            }
            else if(_loc2_ == ModeIndex_6)
            {
               ModeInfoTable[_loc1_].LayoutType = 6;
               ModeInfoTable[_loc1_].ListRare = true;
               ModeInfoTable[_loc1_].ListNum = false;
            }
            else
            {
               ModeInfoTable[_loc1_].LayoutType = -1;
               ModeInfoTable[_loc1_].ListRare = true;
               ModeInfoTable[_loc1_].ListNum = true;
            }
         }
         this.setListButtonLayout(_loc1_);
      }
      
      private function TryChange_ListButtonLayout() : void
      {
         var _loc2_:int = 0;
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         if(_loc1_ != ModeType_LobbyDictionary)
         {
            return;
         }
         if(_loc1_ == ModeType_LobbyDictionary)
         {
            _loc2_ = -1;
            if(this.m_filter_button)
            {
               _loc2_ = this.m_filter_button.GetSelectUserData();
            }
            if(_loc2_ == FilterType_SkillSpa || _loc2_ == FilterType_SkillUlt || _loc2_ == FilterType_SkillMet || _loc2_ == FilterType_SkillEsc)
            {
               ModeInfoTable[_loc1_].LayoutType = 4;
               ModeInfoTable[_loc1_].ListRare = false;
            }
            else
            {
               ModeInfoTable[_loc1_].LayoutType = 7;
               ModeInfoTable[_loc1_].ListRare = true;
            }
         }
         this.setListButtonLayout(_loc1_);
      }
      
      private function setListButtonLayout(param1:int) : void
      {
         var _loc2_:int = int(ModeInfoTable[param1].LayoutType);
         this.m_menuType_func(_loc2_);
      }
      
      private function initListBase() : void
      {
         var _loc10_:String = null;
         var _loc11_:String = null;
         var _loc12_:int = 0;
         var _loc13_:Array = null;
         var _loc14_:* = undefined;
         var _loc15_:int = 0;
         var _loc16_:MovieClip = null;
         trace("initListBase()");
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveType_ListType);
         var _loc3_:int = this.m_callback.GetUserDataInt(ReceiveType_PreModeIndex);
         var _loc4_:Object = ModeInfoTable[_loc1_];
         var _loc5_:Boolean = _loc4_.ItemElemMax < ButtonMax - 1;
         var _loc6_:MovieClip = this.GetMovieClip();
         var _loc7_:MovieClip = _loc6_.nest;
         if(_loc7_.sys_header_nmb != null)
         {
            _loc10_ = "";
            if(_loc4_.ListNum)
            {
               _loc10_ = StringList.HeaderNumber;
            }
            Utility.SetAutoSizedhtmlText(_loc7_.sys_header_nmb.sys,_loc10_);
         }
         if(_loc7_.sys_header_price != null)
         {
            _loc11_ = "";
            if(_loc4_.ListPrice)
            {
               if(this.isNeedMixMaterial() || _loc1_ == ModeType_MixRecipe)
               {
                  _loc11_ = StringList.HeaderPriceMix;
               }
               else
               {
                  _loc12_ = this.m_callback.GetUserDataInt(ReceiveType_PayType);
                  if(_loc12_ == PayType_Zeny)
                  {
                     _loc11_ = StringList.HeaderPrice1;
                  }
                  else if(_loc12_ == PayType_Medal)
                  {
                     _loc11_ = StringList.HeaderPrice2;
                  }
                  else if(_loc12_ == PayType_STPMedal)
                  {
                     _loc11_ = StringList.HeaderPrice3;
                  }
                  else if(_loc12_ == PayType_BMedal)
                  {
                     _loc11_ = StringList.HeaderPrice4;
                  }
               }
            }
            Utility.SetAutoSizedhtmlText(_loc7_.sys_header_price.sys,_loc11_);
         }
         if(_loc7_.material != null)
         {
            if(_loc5_)
            {
               this.m_material_box = new MaterialBox(_loc7_.material,_loc1_ == ModeType_MixRecipe ? true : false);
               this.m_material_box.SetVisible(true);
            }
            else
            {
               _loc7_.material.visible = false;
               this.m_material_box = null;
            }
         }
         else if(this.m_material_box)
         {
            this.m_material_box.Destroy();
            this.m_material_box = null;
         }
         if(_loc7_.spin != null)
         {
            _loc13_ = null;
            if(_loc1_ == ModeType_EventShop)
            {
               if(_loc3_ == ModeIndex_1)
               {
                  _loc13_ = FilterTable.ShopSkill;
               }
               else if(_loc3_ == ModeIndex_2)
               {
                  _loc13_ = FilterTable.ShopCostume;
               }
               else if(_loc3_ == ModeIndex_3)
               {
                  _loc13_ = FilterTable.ShopAccessory;
               }
               else if(_loc3_ == ModeIndex_4)
               {
                  _loc13_ = FilterTable.ShopTalisman;
               }
               else if(_loc3_ == ModeIndex_5)
               {
                  _loc13_ = FilterTable.ShopGift;
               }
               else if(_loc3_ == ModeIndex_6)
               {
                  _loc13_ = FilterTable.ShopPet;
               }
            }
            else if(_loc1_ == ModeType_CustomizeSkill)
            {
               if(_loc3_ == SkillType_SPA)
               {
                  _loc13_ = FilterTable.CostomizeSkillSPA;
               }
               else if(_loc3_ == SkillType_ULT)
               {
                  _loc13_ = FilterTable.CostomizeSkillULT;
               }
               else if(_loc3_ == SkillType_ESC)
               {
                  _loc13_ = FilterTable.CostomizeSkillESC;
               }
               else if(_loc3_ == SkillType_MET)
               {
                  _loc13_ = FilterTable.CostomizeSkillMET;
               }
            }
            else
            {
               _loc13_ = _loc4_.FilterTable;
               if(_loc3_ == ModeIndex_2 && _loc4_.FilterTable2 != null)
               {
                  _loc13_ = _loc4_.FilterTable2;
               }
               else if(_loc3_ == ModeIndex_3 && _loc4_.FilterTable2 != null && this.m_callback.GetUserDataInt(ReceiveType_PayType) == PayType_STPMedal)
               {
                  _loc13_ = _loc4_.FilterTable2;
               }
               else if(_loc2_ == ListType_Sell)
               {
                  _loc13_ = FilterTable.ShopSell;
               }
            }
            if(_loc13_ != null)
            {
               this.m_filter_button = new LoopButton(_loc7_.spin.sys_spin,_loc7_.spin._CMN_M_B_mc_l1,_loc7_.spin._CMN_M_B_mc_r1);
               _loc14_ = 0;
               while(_loc14_ < _loc13_.length)
               {
                  this.m_filter_button.AddText(_loc13_[_loc14_][1],_loc13_[_loc14_][0]);
                  _loc14_++;
               }
               this.m_filter_button.SelectText(0);
               _loc7_.spin.visible = true;
               this.m_filter_button.SetButtonVisible(_loc13_.length > 1);
            }
            else
            {
               _loc7_.spin.visible = false;
               this.m_filter_button = null;
            }
         }
         else if(this.m_filter_button)
         {
            this.m_filter_button.Destroy();
            this.m_filter_button = null;
         }
         if(_loc1_ == ModeType_MixAbility)
         {
            _loc15_ = this.m_callback.GetUserDataInt(ReceiveType_AbilityBallNum);
            this.m_timeline.kyoka.visible = true;
            this.m_timeline.kyoka.gotoAndPlay("start");
            Utility.SetAutoSizedText(this.m_timeline.kyoka.nest.sys_head,"SHP_STR_00_001");
            Utility.SetAutoSizedText(this.m_timeline.kyoka.nest.sys_nmb,"" + _loc15_);
            this.m_callback.SetUserDataValidFlag(ReceiveType_AbilityBallNum,false);
         }
         _loc7_.scroll.dummy_scroll_1.visible = false;
         _loc7_.scroll.dummy_scroll_2.visible = false;
         var _loc8_:MovieClip = _loc7_.scroll.dummy_scroll_1;
         if(_loc5_)
         {
            _loc8_ = _loc7_.scroll.dummy_scroll_2;
         }
         this.m_list_box.SetScrlBar(_loc7_.scroll._CMN_M_B_scroll,_loc8_);
         var _loc9_:int = 0;
         while(_loc9_ < ButtonMax)
         {
            _loc16_ = _loc7_["btn" + (_loc9_ + 1)];
            if(_loc16_)
            {
               _loc7_["new_" + _loc9_].visible = false;
               this.SetButton(_loc16_,"","","","","",false);
            }
            _loc9_++;
         }
      }
      
      private function calcTextW(param1:MovieClip, param2:int) : int
      {
         var _loc3_:int = param1.width;
         var _loc4_:int = param1.width;
         if(param2 < _loc3_)
         {
            param2 = _loc3_;
         }
         else if(_loc4_ < param2)
         {
            param2 = _loc4_;
         }
         return param2;
      }
      
      private function calcTextH(param1:MovieClip, param2:int) : int
      {
         var _loc3_:int = param1.height;
         var _loc4_:int = param1.height;
         if(param2 < _loc3_)
         {
            param2 = _loc3_;
         }
         else if(_loc4_ < param2)
         {
            param2 = _loc4_;
         }
         return param2;
      }
      
      private function SetButton(param1:MovieClip, param2:String, param3:String, param4:String, param5:String, param6:String, param7:Boolean) : *
      {
         var _loc8_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         var _loc9_:Object = ModeInfoTable[_loc8_];
         if(Hide_RareIcon == param2)
         {
            param1.sys_rareicon.visible = false;
         }
         else
         {
            param1.sys_rareicon.visible = true;
            param1.sys_rareicon.htmlText = param2;
         }
         if(Hide_KindIcon == param3)
         {
            param1.sys_itemicon.visible = false;
         }
         else
         {
            param1.sys_itemicon.visible = true;
            param1.sys_itemicon.htmlText = param3;
         }
         param1.sys_itemicon.htmlText = param3;
         var _loc10_:Number = 0;
         if(_loc8_ == ModeType_LobbySkill && Boolean(param1.plt1))
         {
            _loc10_ = param1.plt1.width - param1.plt1.plt_header.width + param1.itemname.sys_nmb.width;
         }
         Utility.SetAutoSizedhtmlText(param1.itemname.sys_itemname,param4,_loc10_);
         switch(_loc8_)
         {
            case ModeType_CustomizeItem:
            case ModeType_CustomizeSkill:
               param1.icn_check.visible = param7;
               if(_loc8_ == ModeType_CustomizeItem)
               {
                  param1.plt1.visible = false;
                  param1.plt2.visible = true;
               }
               else
               {
                  param1.plt1.visible = true;
                  param1.plt2.visible = false;
               }
               if(param1.plt2.visible)
               {
                  if("" + Hide_NumberPlate == param5)
                  {
                     param1.plt1.visible = true;
                     param1.plt2.visible = false;
                     param1.itemname.sys_nmb.visible = false;
                  }
                  else
                  {
                     param1.itemname.sys_nmb.visible = true;
                     param1.itemname.sys_nmb.text = param5;
                  }
               }
               else
               {
                  param1.itemname.sys_nmb.visible = false;
               }
               break;
            case ModeType_CustomizeCostume:
               param1.icn_check.visible = param7;
               param1.plt1.visible = true;
               param1.plt2.visible = false;
               param1.itemname.sys_nmb.visible = false;
               break;
            case ModeType_MixAbilityAddition:
               param1.icn_check.visible = param7;
               param1.plt1.visible = false;
               param1.plt2.visible = true;
               param1.itemname.sys_nmb.visible = true;
               param1.itemname.sys_nmb.text = param5;
               break;
            case ModeType_FloatPetCustomize:
               param1.icn_check.visible = param7;
               param1.plt1.visible = true;
               param1.plt2.visible = false;
               param1.itemname.sys_nmb.visible = true;
               param1.itemname.sys_nmb.text = param5;
               break;
            default:
               param1.itemname.sys_price.text = param6;
               param1.plt1.visible = !_loc9_.ListNum;
               param1.plt2.visible = _loc9_.ListNum;
               param1.itemname.sys_nmb.text = param5;
         }
      }
      
      private function initList(param1:int = 0, param2:int = 0) : void
      {
         var _loc9_:MovieClip = null;
         var _loc10_:MovieClip = null;
         var _loc3_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         var _loc4_:Object = ModeInfoTable[_loc3_];
         var _loc5_:int = this.m_callback.GetUserDataInt(ReceiveType_ListElemMax);
         var _loc6_:int = Math.min(_loc5_,_loc4_.ItemElemMax);
         var _loc7_:MovieClip = this.GetMovieClip();
         this.m_list_box.ClearItem();
         this.m_button_list.ClearButton();
         var _loc8_:int = 0;
         while(_loc8_ < ButtonMax)
         {
            _loc9_ = _loc7_.nest["btn" + (_loc8_ + 1)];
            if(_loc9_)
            {
               _loc9_.visible = _loc8_ < _loc6_;
               _loc10_ = _loc7_.nest["new_" + _loc8_];
               _loc10_.visible = false;
               if(_loc9_.visible)
               {
                  this.m_list_box.AddItem(_loc9_,null);
                  this.m_button_list.AddButton(_loc9_,_loc8_);
               }
            }
            _loc8_++;
         }
         this.m_list_box.SetItemNum(_loc5_);
         this.m_list_box.SelectList(param1,param2);
      }
      
      private function requestList(param1:Boolean = false, param2:Boolean = true) : void
      {
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_search_ready = false;
         var _loc3_:MovieClip = this.GetMovieClip();
         var _loc4_:int = 0;
         while(_loc4_ < ButtonMax)
         {
            this.m_callback.SetUserDataValidFlag(ReceiveType_ListNameStart + _loc4_,false);
            this.m_callback.SetUserDataValidFlag(ReceiveType_ListNumStart + _loc4_,false);
            this.m_callback.SetUserDataValidFlag(ReceiveType_ListPriceStart + _loc4_,false);
            this.m_callback.SetUserDataValidFlag(ReceiveType_ListRareStart + _loc4_,false);
            this.m_callback.SetUserDataValidFlag(ReceiveType_ListIconStart + _loc4_,false);
            this.m_callback.SetUserDataValidFlag(ReceiveType_ListValidStart + _loc4_,false);
            this.m_callback.SetUserDataValidFlag(ReceiveType_ListNewStart + _loc4_,false);
            this.m_callback.SetUserDataValidFlag(ReceiveType_ListCheckStart + _loc4_,false);
            _loc4_++;
         }
         this.m_callback.SetUserDataValidFlag(ReceiveType_ListElemMax,false);
         if(param1 && Boolean(this.m_filter_button))
         {
            this.m_list_box.ClearItem();
            _loc3_.nest.spin.user_add_property_req_filter_type = this.m_filter_button.GetSelectUserData();
            _loc3_.nest.gotoAndPlay("push_out");
            this.outRecipeMaterial();
         }
         if(param2)
         {
            this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitOutList);
         }
      }
      
      private function waitOutList(param1:Event) : void
      {
         var _loc7_:int = 0;
         var _loc8_:int = 0;
         var _loc9_:int = 0;
         var _loc10_:int = 0;
         var _loc2_:MovieClip = this.GetMovieClip();
         var _loc3_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         var _loc4_:Object = ModeInfoTable[_loc3_];
         var _loc5_:MovieClip = _loc2_.nest;
         if(_loc5_.currentLabel == "push_out" && _loc5_.currentFrame < Utility.GetLabelEndFrame(_loc5_,"push_out"))
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitOutList);
         var _loc6_:* = _loc5_.spin ? (_loc5_.spin.user_add_property_req_filter_type != undefined ? true : false) : false;
         if(_loc6_)
         {
            _loc7_ = int(_loc5_.spin.user_add_property_req_filter_type);
            _loc8_ = SortType_NoSort;
            if(_loc4_.SortTable)
            {
               _loc8_ = int(_loc4_.SortTable[this.m_sort_type]);
            }
            _loc9_ = this.m_callback.GetUserDataInt(ReceiveType_ListType);
            if(_loc9_ == ListType_Sell)
            {
               _loc8_ = SortType_Get;
            }
            _loc5_.spin.user_add_property_req_filter_type = undefined;
            _loc10_ = ((_loc7_ & 0xFFFF) << 16) + (_loc8_ & 0xFFFF);
            this.m_favorite_count = 0;
            this.m_callback.CallbackUserData("user",SendType_RequestSort,_loc10_);
         }
         else
         {
            this.m_callback.CallbackUserData("user",SendType_RequestList,this.m_list_box.GetStartIndex());
            if(this.m_list_box.GetSelectIndex() >= 0)
            {
               this.m_callback.CallbackUserData("user",SendType_SelectIndex,this.m_list_box.GetSelectIndex());
            }
            else
            {
               this.m_callback.CallbackUserData("user",SendType_SelectIndex,0);
            }
            this.m_callback.CallbackUserData("user",SendType_ListPosition,0);
         }
         if(this.isNeedMixMaterial() && this.m_material_mix_counter > 0)
         {
            this.m_callback.SetUserDataValidFlag(ReceiveType_Material3Name,false);
            this.m_callback.SetUserDataValidFlag(ReceiveType_Material1Flag,false);
            this.m_callback.SetUserDataValidFlag(ReceiveType_Material2Flag,false);
            this.m_callback.SetUserDataValidFlag(ReceiveType_Material3Flag,false);
            this.m_callback.SetUserDataValidFlag(ReceiveType_MaterialMixReset,false);
            this.m_callback.CallbackUserData("user",SendType_MixMaterialResultName,0);
         }
         this.requestRecipeMaterial();
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitSetupList);
      }
      
      private function waitSetupList(param1:Event) : void
      {
         var _loc2_:int = 0;
         var _loc3_:int = 0;
         if(!this.checkSetupList())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitSetupList);
         this.changeListPage();
         if(this.m_callback.GetUserDataValidFlag(ReceiveType_ListSelectList) && this.m_callback.GetUserDataValidFlag(ReceiveType_ListButtonStart))
         {
            _loc2_ = this.m_callback.GetUserDataInt(ReceiveType_ListSelectList);
            _loc3_ = this.m_callback.GetUserDataInt(ReceiveType_ListButtonStart);
            this.m_list_box.SelectList(_loc2_,_loc3_);
            this.m_callback.SetUserDataValidFlag(ReceiveType_ListSelectList,false);
            this.m_callback.SetUserDataValidFlag(ReceiveType_ListButtonStart,false);
            this.changeListPage();
         }
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitShowList);
      }
      
      private function checkSetupList() : Boolean
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ListElemMax))
         {
            return false;
         }
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         var _loc2_:Object = ModeInfoTable[_loc1_];
         var _loc3_:Boolean = Boolean(_loc2_.ListNum);
         var _loc4_:Boolean = Boolean(_loc2_.ListPrice);
         var _loc5_:Boolean = Boolean(_loc2_.ListRare);
         if(this.isNeedMixMaterial() && this.m_material_mix_counter == 0)
         {
            _loc4_ = false;
         }
         var _loc6_:int = this.m_callback.GetUserDataInt(ReceiveType_ListElemMax);
         var _loc7_:int = Math.min(_loc6_,_loc2_.ItemElemMax);
         var _loc8_:int = 0;
         while(_loc8_ < _loc7_)
         {
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ListNameStart + _loc8_))
            {
               return false;
            }
            if(_loc3_ && !this.m_callback.GetUserDataValidFlag(ReceiveType_ListNumStart + _loc8_))
            {
               return false;
            }
            if(_loc4_ && !this.m_callback.GetUserDataValidFlag(ReceiveType_ListPriceStart + _loc8_))
            {
               return false;
            }
            if(_loc5_ && !this.m_callback.GetUserDataValidFlag(ReceiveType_ListRareStart + _loc8_))
            {
               return false;
            }
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ListIconStart + _loc8_))
            {
               return false;
            }
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ListValidStart + _loc8_))
            {
               return false;
            }
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ListNewStart + _loc8_))
            {
               return false;
            }
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ListCheckStart + _loc8_))
            {
               return false;
            }
            _loc8_++;
         }
         if(this.isNeedMixMaterial() && this.m_material_mix_counter > 0)
         {
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_Material3Name))
            {
               return false;
            }
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_Material1Flag))
            {
               return false;
            }
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_Material2Flag))
            {
               return false;
            }
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_Material3Flag))
            {
               return false;
            }
         }
         if(!this.checkSetupRecipeMaterial())
         {
            return false;
         }
         return true;
      }
      
      private function changeListPage() : void
      {
         var _loc10_:int = 0;
         var _loc11_:int = 0;
         var _loc12_:MovieClip = null;
         if(this.m_list_box.GetSelectIndex() < 0)
         {
            this.initList();
         }
         this.TryChange_ListButtonLayout();
         var _loc1_:MovieClip = this.GetMovieClip();
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         var _loc3_:Object = ModeInfoTable[_loc2_];
         var _loc4_:int = this.m_callback.GetUserDataInt(ReceiveType_ListElemMax);
         var _loc5_:int = Math.min(_loc4_,_loc3_.ItemElemMax);
         if(this.m_button_list.GetButtonNum() != _loc5_ || this.m_list_box.GetItemNum() != _loc4_)
         {
            _loc10_ = 0;
            _loc11_ = 0;
            if(this.m_restore_pos_change_page_init_list == true)
            {
               _loc10_ = this.m_list_box.GetSelectIndex();
               _loc11_ = this.m_list_box.GetSelectButtonIndex();
            }
            this.initList(_loc10_,_loc11_);
         }
         this.m_restore_pos_change_page_init_list = false;
         var _loc6_:Boolean = Boolean(_loc3_.ListNum);
         var _loc7_:Boolean = Boolean(_loc3_.ListPrice);
         var _loc8_:Boolean = Boolean(_loc3_.ListRare);
         if(this.isNeedMixMaterial() && this.m_material_mix_counter == 0)
         {
            _loc7_ = false;
         }
         var _loc9_:int = 0;
         while(_loc9_ < this.m_button_list.GetButtonNum())
         {
            _loc12_ = this.m_button_list.GetButton(_loc9_);
            if(!_loc12_)
            {
               break;
            }
            this.SetButton(_loc12_,_loc8_ ? this.m_callback.GetUserDataString(ReceiveType_ListRareStart + _loc9_) : "",this.m_callback.GetUserDataString(ReceiveType_ListIconStart + _loc9_),this.favoriteName(this.m_callback.GetUserDataString(ReceiveType_ListNameStart + _loc9_),this.m_list_box.GetStartIndex() + _loc9_),_loc6_ ? "" + this.m_callback.GetUserDataInt(ReceiveType_ListNumStart + _loc9_) : "",_loc7_ ? "" + this.m_callback.GetUserDataInt(ReceiveType_ListPriceStart + _loc9_) : "",this.m_callback.GetUserDataInt(ReceiveType_ListCheckStart + _loc9_) ? true : false);
            _loc1_.nest["new_" + _loc9_].visible = this.m_callback.GetUserDataInt(ReceiveType_ListNewStart + _loc9_) ? true : false;
            if(_loc9_ == this.m_list_box.GetSelectButtonIndex())
            {
               if(this.isValidListItem(_loc9_))
               {
                  _loc12_.gotoAndPlay("on");
               }
               else
               {
                  _loc12_.gotoAndPlay("gray_on");
               }
            }
            else if(this.isValidListItem(_loc9_))
            {
               _loc12_.gotoAndPlay("off");
            }
            else
            {
               _loc12_.gotoAndPlay("gray_off");
            }
            _loc9_++;
         }
         if(_loc1_.nest.currentLabel == "push_out")
         {
            _loc1_.nest.gotoAndPlay("push_in");
         }
         this.updateSortInfo();
         this.m_oneshot_newflag = false;
      }
      
      private function waitShowList(param1:Event) : void
      {
         var _loc2_:MovieClip = this.GetMovieClip();
         var _loc3_:MovieClip = _loc2_.nest;
         if(_loc3_.currentLabel == "push_in" && _loc3_.currentFrame < Utility.GetLabelEndFrame(_loc3_,"push_in") - 1)
         {
            return;
         }
         if(_loc2_.currentLabel != "wait")
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitShowList);
         this.changeRecipeMaterial();
         this.selectMixMaterial();
         this.m_timeline.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_search_ready = true;
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.updateNewFlagNotify);
      }
      
      private function changeListButton(param1:int) : Boolean
      {
         var _loc11_:MovieClip = null;
         var _loc12_:int = 0;
         var _loc13_:int = 0;
         var _loc14_:int = 0;
         var _loc15_:int = 0;
         var _loc2_:int = this.m_list_box.GetSelectIndex();
         var _loc3_:int = this.m_list_box.GetSelectButtonIndex();
         var _loc4_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         var _loc5_:int = Math.abs(param1);
         var _loc6_:int = this.m_callback.GetUserDataInt(ReceiveType_ListElemMax);
         var _loc7_:int = _loc2_ + param1;
         var _loc8_:int = this.m_list_box.GetSelectButtonIndex();
         if(_loc5_ == 1)
         {
            _loc12_ = this.m_list_box.GetStartIndex();
            if(_loc7_ < 0)
            {
               _loc7_ = _loc6_ - 1;
               _loc8_ = -1;
            }
            else if(_loc7_ >= _loc6_)
            {
               _loc7_ = 0;
               _loc8_ = -1;
            }
            else
            {
               _loc8_ = -1;
               if(param1 < 0)
               {
                  _loc13_ = _loc7_ - _loc12_ + 1;
                  if(_loc13_ < CursolMoveLimit)
                  {
                     if(_loc7_ < _loc13_)
                     {
                        _loc8_ = _loc7_;
                     }
                     else
                     {
                        _loc8_ = _loc13_;
                     }
                  }
               }
               else if(0 < param1)
               {
                  _loc14_ = this.m_button_list.GetButtonNum();
                  if(_loc14_ > _loc6_)
                  {
                     _loc14_ = _loc6_;
                  }
                  if(_loc7_ - _loc12_ >= _loc14_ - CursolMoveLimit)
                  {
                     _loc8_ = _loc14_ - CursolMoveLimit;
                     _loc15_ = _loc14_ - (_loc7_ - _loc12_);
                     if(_loc15_ < CursolMoveLimit)
                     {
                        _loc8_ += CursolMoveLimit - _loc15_ - 1;
                     }
                  }
               }
            }
            this.m_list_box.SelectList(_loc7_,_loc8_);
         }
         else if(_loc5_ == ModeInfoTable[_loc4_].ItemElemMax)
         {
            if(_loc7_ - _loc8_ < 0)
            {
               _loc7_ = 0;
               _loc8_ = 0;
            }
            else if(_loc6_ <= _loc7_)
            {
               _loc7_ = _loc6_ - 1;
               _loc8_ = -1;
            }
            this.m_list_box.SelectList(_loc7_,_loc8_);
         }
         var _loc9_:int = this.m_list_box.GetSelectIndex();
         var _loc10_:int = this.m_list_box.GetSelectButtonIndex();
         if(!this.isValidListItem(_loc3_))
         {
            _loc11_ = this.m_button_list.GetButton(_loc3_);
            if(_loc11_)
            {
               _loc11_.gotoAndPlay("gray_off");
            }
         }
         if(!this.isValidListItem(_loc10_))
         {
            _loc11_ = this.m_button_list.GetButton(_loc10_);
            if(_loc11_)
            {
               _loc11_.gotoAndPlay("gray_on");
            }
         }
         if(_loc2_ != _loc9_)
         {
            this.m_oneshot_newflag = false;
            this.m_elapsed_time = getTimer();
            return true;
         }
         return false;
      }
      
      private function decideList() : Boolean
      {
         var _loc2_:int = 0;
         if(!this.isValidListItem(this.m_list_box.GetSelectButtonIndex()))
         {
            return false;
         }
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         if(_loc1_ == ModeType_MixAbilityAddition || _loc1_ == ModeType_CustomizeItem || _loc1_ == ModeType_CustomizeSkill || _loc1_ == ModeType_CustomizeCostume)
         {
            this.endList();
            return true;
         }
         if(this.pushMixMaterial())
         {
            return true;
         }
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_search_ready = false;
         this.m_callback.SetUserDataValidFlag(ReceiveType_RequestResume,false);
         this.m_callback.CallbackUserData("user",SendType_DecideButtonPosition,this.m_list_box.GetSelectButtonIndex());
         this.m_callback.CallbackUserData("user",SendType_DecideIndex,this.m_list_box.GetSelectIndex());
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitResumeList);
         if(this.isNeedMixMaterial())
         {
            this.requestList(false,false);
         }
         else if(this.isShopSkill() || this.isShopPet())
         {
            this.requestList(false,false);
         }
         else if(this.isShopMode())
         {
            _loc2_ = this.m_callback.GetUserDataInt(ReceiveType_ListType);
            if(_loc2_ == ListType_Sell)
            {
               this.requestList(false,false);
            }
         }
         else if(_loc1_ == ModeType_FloatPetCustomize)
         {
            this.requestList(false,false);
         }
         return true;
      }
      
      private function cancelList() : Boolean
      {
         if(this.popMixMaterial())
         {
            return true;
         }
         this.endList(true);
         return true;
      }
      
      private function pauseList(param1:int) : Boolean
      {
         if(this.m_callback.GetUserDataValidFlag(ReceiveType_EnableListPause))
         {
            this.m_enable_list_pause = this.m_callback.GetUserDataInt(ReceiveType_EnableListPause) == 1;
            this.m_callback.SetUserDataValidFlag(ReceiveType_EnableListPause,false);
         }
         if(!this.m_enable_list_pause)
         {
            return false;
         }
         if(this.m_status_is_list_pause)
         {
            return false;
         }
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_search_ready = false;
         this.m_callback.SetUserDataValidFlag(ReceiveType_RequestResume,false);
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitResumeList);
         this.m_callback.CallbackUserData("user",SendType_PauseList,param1);
         this.m_status_is_list_pause = true;
         return true;
      }
      
      private function waitResumeList(param1:Event) : void
      {
         var _loc7_:int = 0;
         var _loc8_:int = 0;
         var _loc9_:int = 0;
         var _loc10_:int = 0;
         var _loc11_:MovieClip = null;
         var _loc12_:MovieClip = null;
         var _loc13_:int = 0;
         var _loc14_:Boolean = false;
         var _loc15_:Boolean = false;
         var _loc16_:Boolean = false;
         var _loc17_:int = 0;
         var _loc18_:int = 0;
         var _loc19_:int = 0;
         var _loc20_:Boolean = false;
         var _loc21_:Boolean = false;
         var _loc22_:int = 0;
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_RequestResume))
         {
            return;
         }
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         var _loc3_:int = this.m_callback.GetUserDataInt(ReceiveType_ListType);
         var _loc4_:int = this.m_callback.GetUserDataInt(ReceiveType_AbilityBallNum);
         if(this.isNeedMixMaterial())
         {
            if(this.m_callback.GetUserDataValidFlag(ReceiveType_MaterialMixReset))
            {
               this.m_callback.SetUserDataValidFlag(ReceiveType_MaterialMixReset,false);
               this.popMixMaterial();
               this.popMixMaterial();
               this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitResumeList);
               this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.updateNewFlagNotify);
               if(_loc2_ == ModeType_MixAbility)
               {
                  Utility.SetAutoSizedText(this.m_timeline.kyoka.nest.sys_nmb,"" + _loc4_);
               }
               return;
            }
            if(!this.checkSetupList())
            {
               return;
            }
         }
         else if(this.isShopSkill() || this.isShopPet())
         {
            if(!this.checkSetupList())
            {
               return;
            }
         }
         else if(this.isShopMode())
         {
            if(_loc3_ == ListType_Sell)
            {
               if(!this.checkSetupList())
               {
                  return;
               }
            }
         }
         else if(_loc2_ == ModeType_FloatPetCustomize)
         {
            if(!this.checkSetupList())
            {
               return;
            }
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitResumeList);
         if(ModeType_Present == _loc2_)
         {
            if(this.m_callback.GetUserDataValidFlag(ReceiveType_RequestEnd))
            {
               this.endList();
               return;
            }
         }
         this.m_timeline.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_search_ready = true;
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.updateNewFlagNotify);
         var _loc5_:int = this.m_list_box.GetSelectButtonIndex();
         if(this.m_status_is_list_pause)
         {
            this.m_status_is_list_pause = false;
            if(this.m_callback.GetUserDataValidFlag(ReceiveType_ListSelectList) && this.m_callback.GetUserDataValidFlag(ReceiveType_ListButtonStart))
            {
               _loc7_ = this.m_callback.GetUserDataInt(ReceiveType_ListSelectList);
               _loc8_ = this.m_callback.GetUserDataInt(ReceiveType_ListButtonStart);
               this.m_callback.SetUserDataValidFlag(ReceiveType_ListSelectList,false);
               this.m_callback.SetUserDataValidFlag(ReceiveType_ListButtonStart,false);
               this.m_list_box.SelectList(_loc7_,_loc8_);
            }
            this.selectMixMaterial();
            this.changeListPage();
            return;
         }
         if((_loc2_ == ModeType_MixAbility || _loc2_ == ModeType_MixMaterial) && this.m_callback.GetUserDataValidFlag(ReceiveType_ListButtonStart))
         {
            _loc9_ = this.m_callback.GetUserDataInt(ReceiveType_ListSelectList);
            _loc10_ = this.m_callback.GetUserDataInt(ReceiveType_ListButtonStart);
            this.m_list_box.SelectList(_loc9_,_loc10_);
            this.m_callback.SetUserDataValidFlag(ReceiveType_ListSelectList,false);
            this.m_callback.SetUserDataValidFlag(ReceiveType_ListButtonStart,false);
         }
         if(this.popMixMaterial())
         {
            return;
         }
         var _loc6_:int = this.m_callback.GetUserDataInt(ReceiveType_RequestResume);
         if(_loc6_ == 0)
         {
            _loc11_ = this.GetMovieClip();
            if(_loc2_ == ModeType_CustomizeItem || _loc2_ == ModeType_CustomizeSkill || _loc2_ == ModeType_CustomizeCostume)
            {
               _loc12_ = _loc11_.nest["btn" + (_loc5_ + 1)];
               _loc12_.icn_check.visible = _loc12_.icn_check.visible ? false : true;
               _loc11_.nest["new_" + _loc5_].visible = false;
            }
            else if(_loc2_ == ModeType_FloatPetCustomize)
            {
               _loc13_ = 0;
               while(_loc13_ < ButtonMax)
               {
                  _loc11_.nest["btn" + (_loc13_ + 1)].icn_check.visible = false;
                  _loc13_++;
               }
               _loc11_.nest["btn" + (_loc5_ + 1)].icn_check.visible = true;
               _loc11_.nest["new_" + _loc5_].visible = false;
            }
            else if(this.m_callback.GetUserDataInt(ReceiveType_PayType) == PayType_STPMedal)
            {
               this.changeListPage();
            }
            return;
         }
         if(this.isNeedMixMaterial())
         {
            this.changeListPage();
            this.m_material_box.PushResultText(this.m_callback.GetUserDataString(ReceiveType_Material3Name));
            _loc14_ = this.m_callback.GetUserDataInt(ReceiveType_Material1Flag) == 0 ? false : true;
            _loc15_ = this.m_callback.GetUserDataInt(ReceiveType_Material2Flag) == 0 ? false : true;
            _loc16_ = this.m_callback.GetUserDataInt(ReceiveType_Material3Flag) == 0 ? false : true;
            this.m_material_box.SetActive(_loc14_,_loc15_,_loc16_);
         }
         else if(this.isShopSkill() || this.isShopPet() || this.isShopMode() && _loc3_ == ListType_Sell)
         {
            this.changeListPage();
            if(this.m_callback.GetUserDataValidFlag(ReceiveType_ListSelectList) && this.m_callback.GetUserDataValidFlag(ReceiveType_ListButtonStart))
            {
               _loc17_ = this.m_callback.GetUserDataInt(ReceiveType_ListSelectList);
               _loc18_ = this.m_callback.GetUserDataInt(ReceiveType_ListButtonStart);
               this.m_list_box.SelectList(_loc17_,_loc18_);
               this.m_callback.SetUserDataValidFlag(ReceiveType_ListSelectList,false);
               this.m_callback.SetUserDataValidFlag(ReceiveType_ListButtonStart,false);
               this.changeListPage();
            }
         }
         else
         {
            _loc19_ = this.m_callback.GetUserDataInt(ReceiveType_ListNumStart + _loc5_);
            _loc19_ = _loc19_ + _loc6_;
            _loc20_ = _loc19_ <= 0;
            _loc21_ = true;
            if(_loc2_ == ModeType_LobbyCostume && this.m_filter_button.GetSelectUserData() == FilterType_CostumeAbilityBall && _loc20_)
            {
               _loc21_ = false;
               _loc22_ = this.m_callback.GetUserDataInt(ReceiveType_ListElemMax);
               _loc22_ = _loc22_ - 1;
               this.m_list_box.SetItemNum(_loc22_);
               if(_loc22_ < ButtonMax)
               {
                  this.m_restore_pos_change_page_init_list = true;
               }
               this.m_list_box.SelectList(this.m_list_box.GetSelectIndex() - 1,this.m_list_box.GetSelectButtonIndex() - 1);
            }
            else if(_loc2_ == ModeType_LobbyItem || _loc2_ == ModeType_LobbyCostume || _loc2_ == ModeType_LobbySkill || _loc2_ == ModeType_LobbyDictionary || _loc2_ == ModeType_Battle)
            {
               _loc20_ = false;
            }
            if(_loc20_)
            {
               this.requestList(_loc21_);
            }
            else
            {
               this.SetUserDataInt(ReceiveType_ListNumStart + _loc5_,_loc19_);
               this.changeListPage();
            }
         }
         Utility.SetAutoSizedText(this.m_timeline.kyoka.nest.sys_nmb,"" + _loc4_);
         if(_loc2_ == ModeType_MixAbility && _loc4_ >= 256)
         {
            this.endList(true);
         }
      }
      
      private function isValidListItem(param1:int) : Boolean
      {
         var _loc4_:int = 0;
         var _loc5_:int = 0;
         var _loc6_:int = 0;
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         var _loc3_:int = this.m_callback.GetUserDataInt(ReceiveType_ListType);
         if(ModeInfoTable[_loc2_].ListNum)
         {
            _loc4_ = this.m_callback.GetUserDataInt(ReceiveType_ListNumStart + param1);
            if(_loc3_ == ListType_Buy)
            {
               if(ItemNumMax <= _loc4_)
               {
                  return false;
               }
            }
            else if(_loc2_ == ModeType_CustomizeItem || _loc2_ == ModeType_CustomizeSkill || _loc2_ == ModeType_CustomizeCostume)
            {
               if(_loc4_ != Hide_NumberPlate)
               {
                  if(_loc4_ <= 0)
                  {
                     return false;
                  }
               }
            }
            else if(_loc4_ <= 0)
            {
               return false;
            }
         }
         if(_loc3_ == ListType_Buy || this.isNeedMixMaterial() && this.m_material_mix_counter == 1)
         {
            _loc5_ = this.m_callback.GetUserDataInt(ReceiveType_Zeny);
            _loc6_ = this.m_callback.GetUserDataInt(ReceiveType_ListPriceStart + param1);
            if(_loc5_ < _loc6_)
            {
               return false;
            }
         }
         return this.m_callback.GetUserDataInt(ReceiveType_ListValidStart + param1) == 1;
      }
      
      private function selectMixMaterial() : *
      {
         var _loc4_:Boolean = false;
         var _loc5_:Boolean = false;
         var _loc6_:Boolean = false;
         if(!this.isNeedMixMaterial())
         {
            return;
         }
         if(this.m_material_box.GetTextNum() > this.m_material_mix_counter)
         {
            this.m_material_box.PopText();
         }
         var _loc1_:int = this.m_list_box.GetSelectButtonIndex();
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveType_ListElemMax);
         var _loc3_:String = "";
         if(0 <= _loc1_ && _loc1_ < _loc2_)
         {
            _loc3_ = this.m_callback.GetUserDataString(ReceiveType_ListNameStart + _loc1_);
         }
         this.m_material_box.PushText(_loc3_);
         if(this.m_material_mix_counter == 0)
         {
            this.m_material_box.SetActive(true,true);
         }
         else
         {
            _loc4_ = this.m_callback.GetUserDataInt(ReceiveType_Material1Flag) == 0 ? false : true;
            _loc5_ = this.m_callback.GetUserDataInt(ReceiveType_Material2Flag) == 0 ? false : true;
            _loc6_ = this.m_callback.GetUserDataInt(ReceiveType_Material3Flag) == 0 ? false : true;
            this.m_material_box.SetActive(_loc4_,_loc5_,_loc6_);
         }
         this.m_material_box.PushResultText(this.m_callback.GetUserDataString(ReceiveType_Material3Name));
      }
      
      private function pushMixMaterial() : Boolean
      {
         if(!this.isNeedMixMaterial())
         {
            return false;
         }
         if(this.m_material_mix_counter < 2)
         {
            ++this.m_material_mix_counter;
         }
         if(this.m_material_box.IsFill())
         {
            return false;
         }
         this.m_callback.CallbackUserData("user",SendType_MaterialIndex,this.m_list_box.GetSelectIndex());
         this.requestList(true);
         return true;
      }
      
      private function popMixMaterial() : Boolean
      {
         var _loc1_:int = 0;
         var _loc2_:int = 0;
         var _loc3_:String = null;
         if(!this.isNeedMixMaterial())
         {
            return false;
         }
         if(this.m_material_mix_counter == 2)
         {
            this.m_material_mix_counter = 1;
            this.m_material_box.PopText();
            _loc1_ = this.m_list_box.GetSelectButtonIndex();
            _loc2_ = this.m_callback.GetUserDataInt(ReceiveType_ListElemMax);
            _loc3_ = "";
            if(0 <= _loc1_ && _loc1_ < _loc2_)
            {
               _loc3_ = this.m_callback.GetUserDataString(ReceiveType_ListNameStart + _loc1_);
            }
            this.m_material_box.PushText(_loc3_);
            return false;
         }
         if(this.m_material_mix_counter == 1)
         {
            this.m_material_box.PopText();
            this.m_material_box.PopResultText();
            this.m_material_mix_counter = 0;
            this.m_callback.CallbackUserData("user",SendType_MaterialIndex,-1);
            this.requestList(true);
            return true;
         }
         this.m_material_box.PopText();
         this.m_material_box.PopResultText();
         return false;
      }
      
      private function outRecipeMaterial() : void
      {
         if(!this.isNeedRecipeMaterial())
         {
            return;
         }
         this.m_material_box.SetActive(false,false,false);
      }
      
      private function requestRecipeMaterial() : void
      {
         if(!this.isNeedRecipeMaterial())
         {
            return;
         }
         this.m_callback.SetUserDataValidFlag(ReceiveType_Material1Name,false);
         this.m_callback.SetUserDataValidFlag(ReceiveType_Material2Name,false);
         this.m_callback.SetUserDataValidFlag(ReceiveType_Material1Flag,false);
         this.m_callback.SetUserDataValidFlag(ReceiveType_Material2Flag,false);
         var _loc1_:int = this.m_list_box.GetSelectIndex();
         if(_loc1_ < 0)
         {
            _loc1_ = 0;
         }
         this.m_callback.CallbackUserData("user",SendType_RequestMaterial,_loc1_);
      }
      
      private function checkSetupRecipeMaterial() : Boolean
      {
         if(!this.isNeedRecipeMaterial())
         {
            return true;
         }
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_Material1Name))
         {
            return false;
         }
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_Material2Name))
         {
            return false;
         }
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_Material1Flag))
         {
            return false;
         }
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_Material2Flag))
         {
            return false;
         }
         return true;
      }
      
      private function changeRecipeMaterial() : void
      {
         if(!this.isNeedRecipeMaterial())
         {
            return;
         }
         var _loc1_:int = this.m_list_box.GetSelectButtonIndex();
         var _loc2_:Boolean = this.m_callback.GetUserDataInt(ReceiveType_Material1Flag) == 0 ? false : true;
         var _loc3_:Boolean = this.m_callback.GetUserDataInt(ReceiveType_Material2Flag) == 0 ? false : true;
         this.m_material_box.SetActive(_loc2_,_loc3_);
         this.m_material_box.SetText(this.m_callback.GetUserDataString(ReceiveType_Material1Name),this.m_callback.GetUserDataString(ReceiveType_Material2Name),"");
      }
      
      private function isNeedMixMaterial() : Boolean
      {
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         return _loc1_ == ModeType_MixMaterial || _loc1_ == ModeType_MixAbility;
      }
      
      private function isNeedRecipeMaterial() : Boolean
      {
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         return _loc1_ == ModeType_MixRecipe;
      }
      
      private function isShopSkill() : Boolean
      {
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveType_PreModeIndex);
         return _loc1_ == ModeType_ShopSkill || _loc1_ == ModeType_EventShop && _loc2_ == ModeIndex_1;
      }
      
      private function isShopPet() : Boolean
      {
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveType_PreModeIndex);
         return _loc1_ == ModeType_EventShop && _loc2_ == ModeIndex_6;
      }
      
      private function updateSortInfo() : void
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_SortTypeName))
         {
            return;
         }
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_SortTypeArrow))
         {
            return;
         }
         var _loc1_:String = this.m_callback.GetUserDataString(ReceiveType_SortTypeName);
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveType_SortTypeArrow);
         this.m_callback.SetUserDataValidFlag(ReceiveType_SortTypeName,false);
         this.m_callback.SetUserDataValidFlag(ReceiveType_SortTypeArrow,false);
         this.SetSortInfo(_loc1_,_loc2_);
      }
      
      private function SetSortInfo(param1:String, param2:int) : void
      {
         var _loc6_:MovieClip = null;
         var _loc3_:Array = [this.m_timeline.item_list,this.m_timeline.custom1b,this.m_timeline.custom2];
         var _loc4_:int = param2 == 0 ? 1 : 2;
         var _loc5_:Number = 0;
         while(_loc5_ < _loc3_.length)
         {
            _loc6_ = _loc3_[_loc5_].nest.sort;
            if(_loc6_)
            {
               _loc6_.visible = true;
               Utility.SetAutoSizedhtmlText(_loc6_.sys_sort,param1);
               _loc6_.sort_arrow.gotoAndStop(_loc4_);
            }
            _loc5_++;
         }
      }
      
      private function requestListEnd() : void
      {
         this.cancelList();
      }
      
      private function pushU() : void
      {
         if(!this.changeListButton(-1))
         {
            return;
         }
         this.selectMixMaterial();
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
         this.requestList();
      }
      
      private function pushD() : void
      {
         if(!this.changeListButton(1))
         {
            return;
         }
         this.selectMixMaterial();
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
         this.requestList();
      }
      
      private function pushL() : void
      {
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         if(!this.changeListButton(-ModeInfoTable[_loc1_].ItemElemMax))
         {
            return;
         }
         this.selectMixMaterial();
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
         this.requestList();
      }
      
      private function pushR() : void
      {
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         if(!this.changeListButton(ModeInfoTable[_loc1_].ItemElemMax))
         {
            return;
         }
         this.selectMixMaterial();
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
         this.requestList();
      }
      
      private function pushL1() : void
      {
         if(!this.m_filter_button)
         {
            return;
         }
         if(!this.m_filter_button.IsLeftButtonVisible())
         {
            return;
         }
         this.m_filter_button.SelectPrevText();
         this.m_callback.CallbackSe(this.m_callback.SeTypePageChnage);
         this.requestList(true);
      }
      
      private function pushR1() : void
      {
         if(!this.m_filter_button)
         {
            return;
         }
         if(!this.m_filter_button.IsRightButtonVisible())
         {
            return;
         }
         this.m_filter_button.SelectNextText();
         this.m_callback.CallbackSe(this.m_callback.SeTypePageChnage);
         this.requestList(true);
      }
      
      private function pushL2() : void
      {
      }
      
      private function pushR2() : void
      {
         if(this.pauseList(PauseType_ChangeArrow))
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypePageChnage);
         }
      }
      
      private function pushEnter() : void
      {
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         if(_loc1_ == ModeType_LobbySkill || _loc1_ == ModeType_LobbyDictionary)
         {
            return;
         }
         if(this.decideList())
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeDecide);
         }
         else
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeNg);
         }
      }
      
      private function pushCancel() : void
      {
         if(this.cancelList())
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeCancel);
         }
      }
      
      private function pushX() : void
      {
         if(this.pauseList(PauseType_StartPopup))
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypePageChnage);
         }
      }
      
      private function checkKey(param1:KeyboardEvent) : void
      {
         if(!this.m_input_enable)
         {
            return;
         }
         switch(param1.keyCode)
         {
            case Keyboard.UP:
               this.pushU();
               break;
            case Keyboard.DOWN:
               this.pushD();
               break;
            case Keyboard.LEFT:
               this.pushL();
               break;
            case Keyboard.RIGHT:
               this.pushR();
               break;
            case 88:
               this.pushX();
               break;
            case Keyboard.DELETE:
               this.pushL1();
               break;
            case Keyboard.PAGE_DOWN:
               this.pushR1();
               break;
            case 219:
               this.pushL2();
               break;
            case Keyboard.PAGE_UP:
               this.pushR2();
               break;
            case Keyboard.ENTER:
               this.pushEnter();
               break;
            case Keyboard.ESCAPE:
               this.pushCancel();
         }
      }
      
      // ItemSearch: runs while the list is idle (from updateNewFlagNotify).
      private function checkSearchRefresh() : void
      {
         if(!this.m_search_ready || this.m_status_is_list_pause || !this.m_callback.GetUserDataValidFlag(ReceiveType_RequestResume))
         {
            return;
         }
         var request:int = this.m_callback.GetUserDataInt(ReceiveType_RequestResume);
         if((request & SearchRefreshTagMask) != SearchRefreshTag)
         {
            return;
         }
         this.m_callback.SetUserDataValidFlag(ReceiveType_RequestResume,false);
         var count:int = this.m_callback.GetUserDataInt(ReceiveType_ListElemMax);
         var row:int = request & SearchRowMask;
         this.m_favorite_count = request >> SearchFavoriteShift & SearchFavoriteMask;
         this.m_favorite_first = (request & SearchFavoriteAfterFirst) != 0 ? 1 : 0;
         if(row >= count)
         {
            row = 0;
         }
         this.m_list_box.SetItemNum(count);
         this.m_list_box.SelectList(row,-1);
         this.m_restore_pos_change_page_init_list = true;
         this.requestList();
      }
      
      // ItemSearch: a favorite's row name with a gold star after it.
      private function favoriteName(name:String, index:int) : String
      {
         if(index < this.m_favorite_first || index >= this.m_favorite_first + this.m_favorite_count)
         {
            return name;
         }
         return name + " <font color=\"#FFC800\">\u2605</font>";
      }
      
      private function updateNewFlagNotify(param1:Event) : void
      {
         var _loc2_:MovieClip = null;
         var _loc3_:Number = NaN;
         var _loc4_:int = 0;
         var _loc5_:MovieClip = null;
         this.checkSearchRefresh();
         if(!this.m_oneshot_newflag)
         {
            _loc2_ = this.GetMovieClip();
            _loc3_ = getTimer();
            if(_loc3_ - this.m_elapsed_time > 1 * 1000 / 60)
            {
               this.m_oneshot_newflag = true;
               _loc4_ = 0;
               while(_loc4_ < this.m_button_list.GetButtonNum())
               {
                  _loc5_ = this.m_button_list.GetButton(_loc4_);
                  if(!_loc5_)
                  {
                     break;
                  }
                  if(_loc4_ == this.m_list_box.GetSelectButtonIndex() && Boolean(_loc2_.nest["new_" + _loc4_].visible))
                  {
                     _loc2_.nest["new_" + _loc4_].visible = false;
                     this.SetUserDataInt(ReceiveType_ListNewStart + _loc4_,0);
                  }
                  _loc4_++;
               }
               this.m_callback.CallbackUserData("user",SendType_ListFoldNewFlag,this.m_list_box.GetSelectButtonIndex());
            }
         }
         if(this.checkSetupRecipeMaterial())
         {
            this.changeRecipeMaterial();
         }
      }
      
      public function SetUserDataInt(param1:int, param2:int) : *
      {
         this.m_callback.AddCallbackSetUserDataInt(param1,param2);
      }
      
      public function SetUserDataString(param1:int, param2:String) : *
      {
         this.m_callback.AddCallbackSetUserDataString(param1,param2);
      }
      
      private function GetMovieClip() : MovieClip
      {
         var _loc1_:* = this.m_timeline.item_list;
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         switch(_loc2_)
         {
            case ModeType_CustomizeItem:
               _loc1_ = this.m_timeline.custom1b;
               break;
            case ModeType_CustomizeSkill:
               _loc1_ = this.m_timeline.custom1;
               break;
            case ModeType_CustomizeCostume:
               _loc1_ = this.m_timeline.custom2;
               break;
            case ModeType_MixAbilityAddition:
               _loc1_ = this.m_timeline.custom2;
               break;
            case ModeType_FloatPetCustomize:
               _loc1_ = this.m_timeline.custom3;
         }
         return _loc1_;
      }
      
      private function isCancelExit() : Boolean
      {
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         switch(_loc1_)
         {
            case ModeType_ShopItem:
            case ModeType_ShopCostume:
            case ModeType_ShopAccessory:
            case ModeType_ShopSkill:
            case ModeType_MixMaterial:
            case ModeType_MixRecipe:
            case ModeType_EventShop:
               return false;
            default:
               return true;
         }
      }
      
      private function isShopMode() : Boolean
      {
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ModeType);
         switch(_loc1_)
         {
            case ModeType_ShopItem:
            case ModeType_ShopCostume:
            case ModeType_ShopAccessory:
            case ModeType_ShopSkill:
               return true;
            default:
               return false;
         }
      }
      
      public function TestDestroy() : void
      {
         this.m_callback = null;
         if(this.m_filter_button)
         {
            this.m_filter_button.Destroy();
            this.m_filter_button = null;
         }
         if(this.m_material_box)
         {
            this.m_material_box.Destroy();
            this.m_material_box = null;
         }
         this.m_list_box.ClearItem();
         this.m_list_box.SetScrlBar(null);
         this.m_list_box = null;
         this.m_button_list.ClearButton();
         this.m_button_list = null;
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStart);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStartList);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitOutList);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitSetupList);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitShowList);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitResumeList);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitRequestEnd);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEnd);
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_search_ready = false;
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.updateNewFlagNotify);
         this.m_timeline.item_list.nest.spin.user_add_property_req_filter_type = undefined;
         this.m_timeline.custom1.nest.spin.user_add_property_req_filter_type = undefined;
         this.m_timeline.custom1b.nest.spin.user_add_property_req_filter_type = undefined;
         this.m_timeline.visible = false;
         this.m_timeline = null;
         this.m_elapsed_time = 0;
         this.m_oneshot_newflag = false;
         this.m_material_mix_counter = 0;
         this.m_menuType_func = null;
      }
      
      public function TestCheckUserDataValidFlag(param1:int) : Boolean
      {
         return this.m_callback.GetUserDataValidFlag(param1);
      }
      
      public function TestGetListStartIndex() : int
      {
         return this.m_list_box.GetStartIndex();
      }
      
      public function TestGetListSelectIndex() : int
      {
         return this.m_list_box.GetSelectIndex();
      }
      
      public function TestGetFilterType() : int
      {
         if(!this.m_filter_button)
         {
            return -1;
         }
         return this.m_filter_button.GetSelectUserData();
      }
      
      public function TestGetMixMaterialFocus() : int
      {
         if(!this.m_material_box)
         {
            return -1;
         }
         return this.m_material_box.GetTextNum();
      }
      
      public function TestGetmenuTypeFunc() : Function
      {
         return this.m_menuType_func;
      }
   }
}

