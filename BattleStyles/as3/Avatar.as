package action_script
{
   import flash.display.MovieClip;
   import flash.events.Event;
   import flash.events.KeyboardEvent;
   import flash.external.ExternalInterface;
   import flash.geom.ColorTransform;
   import flash.ui.Keyboard;
   
   public class Avatar
   {
      
      internal static const EditMenuTabNum:int = 2;
      
      internal static const EditMenuItemNum:int = 9;
      
      internal static const ColorPartsMax:int = 4;
      
      internal static const EditMenuColorPartMax:int = EditMenuItemNum * ColorPartsMax;
      
      internal static const ButtonMax:int = EditMenuItemNum;
      
      public static const ReceiveTypeModeType:int = 0;
      
      public static const ReceiveTypeStartPage:int = 1;
      
      public static const ReceiveTypeConfirmResponse:int = 2;
      
      public static const ReceiveTypeFaceHairStr:int = 3;
      
      public static const ReceiveTypeFaceHairNum:int = 4;
      
      public static const ReceiveTypeFaceEyeStr:int = 5;
      
      public static const ReceiveTypeFaceEyeNum:int = 6;
      
      public static const ReceiveTypeFacePupilStr:int = 7;
      
      public static const ReceiveTypeFacePupilNum:int = 8;
      
      public static const ReceiveTypeFaceNoseStr:int = 9;
      
      public static const ReceiveTypeFaceNoseNum:int = 10;
      
      public static const ReceiveTypeFaceMouthStr:int = 11;
      
      public static const ReceiveTypeFaceMouthNum:int = 12;
      
      public static const ReceiveTypeFaceEarStr:int = 13;
      
      public static const ReceiveTypeFaceEarNum:int = 14;
      
      public static const ReceiveTypeClothTopStr:int = 15;
      
      public static const ReceiveTypeClothTopNum:int = 16;
      
      public static const ReceiveTypeClothBottomStr:int = 17;
      
      public static const ReceiveTypeClothBottomNum:int = 18;
      
      public static const ReceiveTypeClothGlovesStr:int = 19;
      
      public static const ReceiveTypeClothGlovesNum:int = 20;
      
      public static const ReceiveTypeClothShoesStr:int = 21;
      
      public static const ReceiveTypeClothShoesNum:int = 22;
      
      public static const ReceiveTypeClothAccessoryStr:int = 23;
      
      public static const ReceiveTypeClothAccessoryNum:int = 24;
      
      public static const ReceiveTypeClothTalismanStr:int = 25;
      
      public static const ReceiveTypeClothTalismanNum:int = 26;
      
      public static const ReceiveTypeClothMaterialStr:int = 27;
      
      public static const ReceiveTypeClothMaterialNum:int = 28;
      
      public static const ReceiveTypeSkillUlt1Str:int = 29;
      
      public static const ReceiveTypeSkillUlt1Num:int = 30;
      
      public static const ReceiveTypeSkillUlt2Str:int = 31;
      
      public static const ReceiveTypeSkillUlt2Num:int = 32;
      
      public static const ReceiveTypeSkillSpa1Str:int = 33;
      
      public static const ReceiveTypeSkillSpa1Num:int = 34;
      
      public static const ReceiveTypeSkillSpa2Str:int = 35;
      
      public static const ReceiveTypeSkillSpa2Num:int = 36;
      
      public static const ReceiveTypeSkillSpa3Str:int = 37;
      
      public static const ReceiveTypeSkillSpa3Num:int = 38;
      
      public static const ReceiveTypeSkillSpa4Str:int = 39;
      
      public static const ReceiveTypeSkillSpa4Num:int = 40;
      
      public static const ReceiveTypeSkillEscStr:int = 41;
      
      public static const ReceiveTypeSkillEscNum:int = 42;
      
      public static const ReceiveTypeSkillMetStr:int = 43;
      
      public static const ReceiveTypeSkillMetNum:int = 44;
      
      public static const ReceiveTypeDeformHeightData:int = 45;
      
      public static const ReceiveTypeDeformPhysiqueData:int = 46;
      
      public static const ReceiveTypeVoiceStr:int = 47;
      
      public static const ReceiveTypeVoiceNum:int = 48;
      
      public static const ReceiveTypeNameStr:int = 49;
      
      public static const ReceiveTypeBodyColorNum:int = 50;
      
      public static const ReceiveTypeFaceHairColorNum:int = 51;
      
      public static const ReceiveTypeFacePupilColorNum:int = 52;
      
      public static const ReceiveTypeClothTopColorNum:int = 53;
      
      public static const ReceiveTypeClothBottomColorNum:int = 54;
      
      public static const ReceiveTypeClothGlovesColorNum:int = 55;
      
      public static const ReceiveTypeClothShoesColorNum:int = 56;
      
      public static const ReceiveTypeFacePaintColorNum:int = 57;
      
      public static const ReceiveTypeColorDataStart:int = 58;
      
      public static const ReceiveTypeColorDataEnd:int = ReceiveTypeColorDataStart + EditMenuColorPartMax - 1;
      
      public static const ReceiveTypeModeOptionEnablePause:int = ReceiveTypeColorDataEnd + 1;
      
      public static const ReceiveTypePauseEnd:int = ReceiveTypeModeOptionEnablePause + 1;
      
      // BattleStyles: user-data slots for the Battle Style item (the DLL
      // raises EditChara's slot count to match).
      public static const ReceiveTypeBattleStyleStr:int = ReceiveTypePauseEnd + 1;
      
      public static const ReceiveTypeBattleStyleNum:int = ReceiveTypeBattleStyleStr + 1;
      
      public static const ReceiveTypeNum:int = ReceiveTypeBattleStyleNum + 1;
      
      public static const SendTypeRequestConfirmType:int = 0;
      
      public static const SendTypeRequestMenuBase:int = 100;
      
      public static const SendTypeRequestMenuColor:int = 101;
      
      public static const SendTypeRequestMenuName:int = 102;
      
      public static const SendTypeRequestMenuCloth:int = 103;
      
      public static const SendTypeRequestMenuClothColor:int = 104;
      
      public static const SendTypeRequestMenuSkill:int = 105;
      
      public static const SendTypeRequestMenuVoice:int = 106;
      
      public static const SendTypeRequestMenuCusCloth:int = 107;
      
      public static const SendTypeRequestMenuCusSkill:int = 108;
      
      public static const SendTypeRequestMenuBarber:int = 109;
      
      public static const SendTypeRequestListFaceHair:int = 200;
      
      public static const SendTypeRequestListFaceEye:int = 201;
      
      public static const SendTypeRequestListFacePupil:int = 202;
      
      public static const SendTypeRequestListFaceNose:int = 203;
      
      public static const SendTypeRequestListFaceMouth:int = 204;
      
      public static const SendTypeRequestListFaceEar:int = 205;
      
      public static const SendTypeRequestListClothTop:int = 206;
      
      public static const SendTypeRequestListClothBottom:int = 207;
      
      public static const SendTypeRequestListClothGloves:int = 208;
      
      public static const SendTypeRequestListClothShoes:int = 209;
      
      public static const SendTypeRequestListClothAccessory:int = 210;
      
      public static const SendTypeRequestListClothTalisman:int = 211;
      
      public static const SendTypeRequestListClothMaterial:int = 212;
      
      public static const SendTypeRequestListSkillUlt1:int = 213;
      
      public static const SendTypeRequestListSkillUlt2:int = 214;
      
      public static const SendTypeRequestListSkillSpa1:int = 215;
      
      public static const SendTypeRequestListSkillSpa2:int = 216;
      
      public static const SendTypeRequestListSkillSpa3:int = 217;
      
      public static const SendTypeRequestListSkillSpa4:int = 218;
      
      public static const SendTypeRequestListSkillEsc:int = 219;
      
      public static const SendTypeRequestListSkillMet:int = 220;
      
      public static const SendTypeRequestListVoice:int = 221;
      
      // BattleStyles: opens the style list (vanilla send types end at 0xe6).
      public static const SendTypeRequestListBattleStyle:int = 240;
      
      public static const SendTypeRequestListBodyColor:int = 222;
      
      public static const SendTypeRequestListFaceHairColor:int = 223;
      
      public static const SendTypeRequestListFacePupilColor:int = 224;
      
      public static const SendTypeRequestListClothTopColor:int = 225;
      
      public static const SendTypeRequestListClothBottomColor:int = 226;
      
      public static const SendTypeRequestListClothGlovesColor:int = 227;
      
      public static const SendTypeRequestListClothShoesColor:int = 228;
      
      public static const SendTypeRequestListFacePaintColor:int = 229;
      
      public static const SendTypeRequestInputName:int = 230;
      
      public static const SendTypeDataDeformHeight:int = 300;
      
      public static const SendTypeDataDeformPhysique:int = 301;
      
      public static const SendTypeRequestEditReset:int = 400;
      
      public static const SendTypeRequestEditPause:int = 401;
      
      internal static const ConfirmTypeCancelCustomizeCloth:int = 0;
      
      internal static const ConfirmTypeFinishCustomizeCloth:int = 1;
      
      internal static const ConfirmTypeCancelCustomizeSkill:int = 2;
      
      internal static const ConfirmTypeFinishCustomizeSkill:int = 3;
      
      public static const EditTypeMenuItem:int = 0;
      
      public static const EditTypeMenuList:int = 1;
      
      public static const EditTypeMenuColor:int = 2;
      
      public static const EditTypeMenuInput:int = 3;
      
      public static const EditTypeNum:int = 4;
      
      public static const ModeTypeWait:int = 0;
      
      public static const ModeTypeCreateFigure:int = 1;
      
      public static const ModeTypeCreateName:int = 2;
      
      public static const ModeTypeCustomizeCloth:int = 3;
      
      public static const ModeTypeCustomizeSkill:int = 4;
      
      public static const ModeTypeReincarnationFigure:int = 5;
      
      public static const ModeTypeReincarnationVoice:int = 6;
      
      public static const ModeTypeCustomize:int = 7;
      
      public static const ModeTypeBarber:int = 8;
      
      private static const ModeFrameInfoList:Array = [-1,2,6,1,5,2,6,2,3];
      
      private static const StringList:Object = {
         "TabText_CreateBase":"ACL_STR_10_000",
         "TabText_CreateColor":"ACL_STR_10_001",
         "TabText_ClothEquip":"CCS_BTN_00_000",
         "TabText_ClothColor":"CCS_BTN_01_000",
         "MenuItem_FaceHair":"ACL_STR_00_001",
         "MenuItem_FaceHairColor":"ACL_STR_01_001",
         "MenuItem_FaceEyes":"ACL_STR_00_002",
         "MenuItem_FacePupil":"ACL_STR_00_007",
         "MenuItem_FacePupilColor":"ACL_STR_01_006",
         "MenuItem_FaceNose":"ACL_STR_00_003",
         "MenuItem_FaceMouth":"ACL_STR_00_004",
         "MenuItem_FaceEars":"ACL_STR_00_005",
         "MenuItem_FacePaintColor":"ACL_STR_10_003",
         "MenuItem_ClothTop":"CSC_STR_00_000",
         "MenuItem_ClothTopColor":"ACL_STR_01_002",
         "MenuItem_ClothBottom":"CCS_STR_00_001",
         "MenuItem_ClothBottomColor":"ACL_STR_01_003",
         "MenuItem_ClothGloves":"CCS_STR_00_002",
         "MenuItem_ClothGlovesColor":"ACL_STR_01_004",
         "MenuItem_ClothShoes":"CCS_STR_00_003",
         "MenuItem_ClothShoesColor":"ACL_STR_01_005",
         "MenuItem_ClothAccessory":"CCS_STR_00_004",
         "MenuItem_ClothTalisman":"CCS_STR_00_005",
         "MenuItem_ClothMaterial":"CCS_STR_00_006",
         "MenuItem_SkillUlt1":"SCS_STR_00_000",
         "MenuItem_SkillUlt2":"SCS_STR_00_001",
         "MenuItem_SkillSpa1":"SCS_STR_00_002",
         "MenuItem_SkillSpa2":"SCS_STR_00_003",
         "MenuItem_SkillSpa3":"SCS_STR_00_004",
         "MenuItem_SkillSpa4":"SCS_STR_00_005",
         "MenuItem_SkillEsc":"SCS_STR_00_006",
         "MenuItem_SkillMet":"SCS_STR_00_007",
         "MenuItem_DeformHeight":"ACL_STR_00_006",
         "MenuItem_DeformPhysique":"ACL_STR_00_000",
         "MenuItem_BodyColor":"ACL_STR_01_000",
         "MenuItem_Voice":"ACL_STR_02_000",
         "MenuItem_Name":"ACL_STR_02_001",
         "Invalid":null
      };
      
      internal static const EditMenuBaseInfoList:Array = [EditMenuTypeDeformHeight,EditMenuTypeDeformPhysique,EditMenuTypeFaceHair,EditMenuTypeFaceEyes,EditMenuTypeFacePupil,EditMenuTypeFaceNose,EditMenuTypeFaceMouth,EditMenuTypeFaceEars];
      
      internal static const EditMenuColorInfoList:Array = [EditMenuTypeBodyColor,EditMenuTypeFaceHairColor,EditMenuTypeFacePupilColor,EditMenuTypeFacePaintColor,EditMenuTypeClothTopColor,EditMenuTypeClothBottomColor,EditMenuTypeClothGlovesColor,EditMenuTypeClothShoesColor];
      
      internal static const EditMenuNameInfoList:Array = [EditMenuTypeVoice,EditMenuTypeName];
      
      // BattleStyles: + EditMenuTypeBattleStyle (Customize runs in mode 3, seen live).
      internal static const EditMenuClothInfoList:Array = [EditMenuTypeClothTop,EditMenuTypeClothBottom,EditMenuTypeClothGloves,EditMenuTypeClothShoes,EditMenuTypeClothAccessory,EditMenuTypeClothMaterial,EditMenuTypeClothTalisman,EditMenuTypeBattleStyle];
      
      internal static const EditMenuClothColorInfoList:Array = [EditMenuTypeClothTopColor,EditMenuTypeClothBottomColor,EditMenuTypeClothGlovesColor,EditMenuTypeClothShoesColor];
      
      internal static const EditMenuSkillInfoList:Array = [EditMenuTypeSkillSpa2,EditMenuTypeSkillSpa1,EditMenuTypeSkillSpa3,EditMenuTypeSkillSpa4,EditMenuTypeSkillUlt2,EditMenuTypeSkillUlt1,EditMenuTypeSkillMet,EditMenuTypeSkillEsc];
      
      internal static const EditMenuVoiceInfoList:Array = [EditMenuTypeVoice,EditMenuTypeNameNoEdit];
      
      internal static const EditMenuBarberInfoList:Array = [EditMenuTypeFaceHair,EditMenuTypeBodyColor,EditMenuTypeFaceHairColor];
      
      internal static const EditMenuTabInfoTypeSetupInfoList:int = 0;
      
      internal static const EditMenuTabInfoTypeRequestType:int = 1;
      
      internal static const EditMenuTabInfoTypeTabText:int = 2;
      
      // BattleStyles: Battle Style under Super Soul (btn_8) on both equip tabs: mode 3
      // (EditMenuClothInfoList) is the Customize menu in game, 7 kept for safety.
      internal static const EditMenuCusClothInfoList:Array = [EditMenuTypeClothTop,EditMenuTypeClothBottom,EditMenuTypeClothGloves,EditMenuTypeClothShoes,EditMenuTypeClothAccessory,EditMenuTypeClothMaterial,EditMenuTypeClothTalisman,EditMenuTypeBattleStyle];
      
      internal static const EditMenuTabInfoList:Array = [[null],[[EditMenuBaseInfoList,SendTypeRequestMenuBase,StringList.TabText_CreateBase],[EditMenuColorInfoList,SendTypeRequestMenuColor,StringList.TabText_CreateColor]],[[EditMenuNameInfoList,SendTypeRequestMenuName,null]],[[EditMenuClothInfoList,SendTypeRequestMenuCloth,StringList.TabText_ClothEquip],[EditMenuClothColorInfoList,SendTypeRequestMenuClothColor,StringList.TabText_ClothColor]],[[EditMenuSkillInfoList,SendTypeRequestMenuSkill,null]],[[EditMenuBaseInfoList,SendTypeRequestMenuBase,StringList.TabText_CreateBase],[EditMenuColorInfoList,SendTypeRequestMenuColor,StringList.TabText_CreateColor]],[[EditMenuVoiceInfoList,SendTypeRequestMenuVoice,null]],[[EditMenuCusClothInfoList,SendTypeRequestMenuCusCloth,StringList.TabText_ClothEquip],[EditMenuSkillInfoList,SendTypeRequestMenuCusSkill,"スキル"]],[[EditMenuBarberInfoList,SendTypeRequestMenuBarber,null]]];
      
      internal static const EditMenuTypeFaceHair:int = 0;
      
      internal static const EditMenuTypeFaceHairColor:int = 1;
      
      internal static const EditMenuTypeFaceEyes:int = 2;
      
      internal static const EditMenuTypeFacePupil:int = 3;
      
      internal static const EditMenuTypeFacePupilColor:int = 4;
      
      internal static const EditMenuTypeFaceNose:int = 5;
      
      internal static const EditMenuTypeFaceMouth:int = 6;
      
      internal static const EditMenuTypeFaceEars:int = 7;
      
      internal static const EditMenuTypeFacePaintColor:int = 8;
      
      internal static const EditMenuTypeClothTop:int = 9;
      
      internal static const EditMenuTypeClothTopColor:int = 10;
      
      internal static const EditMenuTypeClothBottom:int = 11;
      
      internal static const EditMenuTypeClothBottomColor:int = 12;
      
      internal static const EditMenuTypeClothGloves:int = 13;
      
      internal static const EditMenuTypeClothGlovesColor:int = 14;
      
      internal static const EditMenuTypeClothShoes:int = 15;
      
      internal static const EditMenuTypeClothShoesColor:int = 16;
      
      internal static const EditMenuTypeClothAccessory:int = 17;
      
      internal static const EditMenuTypeClothTalisman:int = 18;
      
      internal static const EditMenuTypeClothMaterial:int = 19;
      
      internal static const EditMenuTypeSkillUlt1:int = 20;
      
      internal static const EditMenuTypeSkillUlt2:int = 21;
      
      internal static const EditMenuTypeSkillSpa1:int = 22;
      
      internal static const EditMenuTypeSkillSpa2:int = 23;
      
      internal static const EditMenuTypeSkillSpa3:int = 24;
      
      internal static const EditMenuTypeSkillSpa4:int = 25;
      
      internal static const EditMenuTypeSkillEsc:int = 26;
      
      internal static const EditMenuTypeSkillMet:int = 27;
      
      internal static const EditMenuTypeDeformHeight:int = 28;
      
      internal static const EditMenuTypeDeformPhysique:int = 29;
      
      internal static const EditMenuTypeBodyColor:int = 30;
      
      internal static const EditMenuTypeVoice:int = 31;
      
      internal static const EditMenuTypeName:int = 32;
      
      internal static const EditMenuTypeNameNoEdit:int = 33;
      
      internal static const EditMenuTypeBattleStyle:int = 34;
      
      internal static const EditMenuTypeNum:int = 35;
      
      internal static const EditMenuItemTypePart:int = 0;
      
      internal static const EditMenuItemTypeColor:int = 1;
      
      internal static const EditMenuItemTypeInput:int = 2;
      
      internal static const EditMenuItemTypeDeform:int = 3;
      
      internal static const EditMenuItemInfoTypeHeader:int = 0;
      
      internal static const EditMenuItemInfoTypeItemType:int = 1;
      
      internal static const EditMenuItemInfoTypeReceiveTypeData:int = 2;
      
      internal static const EditMenuItemInfoTypeReceiveTypeNum:int = 3;
      
      internal static const EditMenuItemInfoTypeRequestType:int = 4;
      
      internal static const EditMenuItemInfoTypeFlagActive:int = 5;
      
      internal static const EditMenuItemInfoList:Array = [[StringList.MenuItem_FaceHair,EditMenuItemTypePart,ReceiveTypeFaceHairStr,ReceiveTypeFaceHairNum,SendTypeRequestListFaceHair,true],[StringList.MenuItem_FaceHairColor,EditMenuItemTypeColor,ReceiveTypeColorDataStart + ColorPartsMax * 1,ReceiveTypeFaceHairColorNum,SendTypeRequestListFaceHairColor,true],[StringList.MenuItem_FaceEyes,EditMenuItemTypePart,ReceiveTypeFaceEyeStr,ReceiveTypeFaceEyeNum,SendTypeRequestListFaceEye,true],[StringList.MenuItem_FacePupil,EditMenuItemTypePart,ReceiveTypeFacePupilStr,ReceiveTypeFacePupilNum,SendTypeRequestListFacePupil,true],[StringList.MenuItem_FacePupilColor,EditMenuItemTypeColor,ReceiveTypeColorDataStart + ColorPartsMax * 2,ReceiveTypeFacePupilColorNum,SendTypeRequestListFacePupilColor,true],[StringList.MenuItem_FaceNose,EditMenuItemTypePart,ReceiveTypeFaceNoseStr,ReceiveTypeFaceNoseNum,SendTypeRequestListFaceNose,true],[StringList.MenuItem_FaceMouth,EditMenuItemTypePart,ReceiveTypeFaceMouthStr,ReceiveTypeFaceMouthNum
      ,SendTypeRequestListFaceMouth,true],[StringList.MenuItem_FaceEars,EditMenuItemTypePart,ReceiveTypeFaceEarStr,ReceiveTypeFaceEarNum,SendTypeRequestListFaceEar,true],[StringList.MenuItem_FacePaintColor,EditMenuItemTypeColor,ReceiveTypeColorDataStart + ColorPartsMax * 7,ReceiveTypeFacePaintColorNum,SendTypeRequestListFacePaintColor,true],[StringList.MenuItem_ClothTop,EditMenuItemTypePart,ReceiveTypeClothTopStr,ReceiveTypeClothTopNum,SendTypeRequestListClothTop,true],[StringList.MenuItem_ClothTopColor,EditMenuItemTypeColor,ReceiveTypeColorDataStart + ColorPartsMax * 3,ReceiveTypeClothTopColorNum,SendTypeRequestListClothTopColor,true],[StringList.MenuItem_ClothBottom,EditMenuItemTypePart,ReceiveTypeClothBottomStr,ReceiveTypeClothBottomNum,SendTypeRequestListClothBottom,true],[StringList.MenuItem_ClothBottomColor,EditMenuItemTypeColor,ReceiveTypeColorDataStart + ColorPartsMax * 4,ReceiveTypeClothBottomColorNum,SendTypeRequestListClothBottomColor,true],[StringList.MenuItem_ClothGloves,EditMenuItemTypePart
      ,ReceiveTypeClothGlovesStr,ReceiveTypeClothGlovesNum,SendTypeRequestListClothGloves,true],[StringList.MenuItem_ClothGlovesColor,EditMenuItemTypeColor,ReceiveTypeColorDataStart + ColorPartsMax * 5,ReceiveTypeClothGlovesColorNum,SendTypeRequestListClothGlovesColor,true],[StringList.MenuItem_ClothShoes,EditMenuItemTypePart,ReceiveTypeClothShoesStr,ReceiveTypeClothShoesNum,SendTypeRequestListClothShoes,true],[StringList.MenuItem_ClothShoesColor,EditMenuItemTypeColor,ReceiveTypeColorDataStart + ColorPartsMax * 6,ReceiveTypeClothShoesColorNum,SendTypeRequestListClothShoesColor,true],[StringList.MenuItem_ClothAccessory,EditMenuItemTypePart,ReceiveTypeClothAccessoryStr,ReceiveTypeClothAccessoryNum,SendTypeRequestListClothAccessory,true],[StringList.MenuItem_ClothTalisman,EditMenuItemTypePart,ReceiveTypeClothTalismanStr,ReceiveTypeClothTalismanNum,SendTypeRequestListClothTalisman,true],[StringList.MenuItem_ClothMaterial,EditMenuItemTypePart,ReceiveTypeClothMaterialStr,ReceiveTypeClothMaterialNum
      ,SendTypeRequestListClothMaterial,true],[StringList.MenuItem_SkillUlt1,EditMenuItemTypePart,ReceiveTypeSkillUlt1Str,ReceiveTypeSkillUlt1Num,SendTypeRequestListSkillUlt1,true],[StringList.MenuItem_SkillUlt2,EditMenuItemTypePart,ReceiveTypeSkillUlt2Str,ReceiveTypeSkillUlt2Num,SendTypeRequestListSkillUlt2,true],[StringList.MenuItem_SkillSpa1,EditMenuItemTypePart,ReceiveTypeSkillSpa1Str,ReceiveTypeSkillSpa1Num,SendTypeRequestListSkillSpa1,true],[StringList.MenuItem_SkillSpa2,EditMenuItemTypePart,ReceiveTypeSkillSpa2Str,ReceiveTypeSkillSpa2Num,SendTypeRequestListSkillSpa2,true],[StringList.MenuItem_SkillSpa3,EditMenuItemTypePart,ReceiveTypeSkillSpa3Str,ReceiveTypeSkillSpa3Num,SendTypeRequestListSkillSpa3,true],[StringList.MenuItem_SkillSpa4,EditMenuItemTypePart,ReceiveTypeSkillSpa4Str,ReceiveTypeSkillSpa4Num,SendTypeRequestListSkillSpa4,true],[StringList.MenuItem_SkillEsc,EditMenuItemTypePart,ReceiveTypeSkillEscStr,ReceiveTypeSkillEscNum,SendTypeRequestListSkillEsc,true],[StringList.MenuItem_SkillMet
      ,EditMenuItemTypePart,ReceiveTypeSkillMetStr,ReceiveTypeSkillMetNum,SendTypeRequestListSkillMet,true],[StringList.MenuItem_DeformHeight,EditMenuItemTypeDeform,ReceiveTypeDeformHeightData,4,SendTypeDataDeformHeight,true],[StringList.MenuItem_DeformPhysique,EditMenuItemTypeDeform,ReceiveTypeDeformPhysiqueData,3,SendTypeDataDeformPhysique,true],[StringList.MenuItem_BodyColor,EditMenuItemTypeColor,ReceiveTypeColorDataStart + ColorPartsMax * 0,ReceiveTypeBodyColorNum,SendTypeRequestListBodyColor,true],[StringList.MenuItem_Voice,EditMenuItemTypePart,ReceiveTypeVoiceStr,ReceiveTypeVoiceNum,SendTypeRequestListVoice,true],[StringList.MenuItem_Name,EditMenuItemTypeInput,ReceiveTypeNameStr,-1,SendTypeRequestInputName,true],[StringList.MenuItem_Name,EditMenuItemTypeInput,ReceiveTypeNameStr,-1,-1,false],["Battle Style	",EditMenuItemTypePart,ReceiveTypeBattleStyleStr,ReceiveTypeBattleStyleNum,SendTypeRequestListBattleStyle,true]];
      
      internal static const FlagNextTypeBack:int = 0;
      
      internal static const FlagNextTypeNext:int = 1;
      
      internal static const FlagNextTypeChangeTab:int = 2;
      
      internal static const FlagNextTypeFinish:int = 3;
      
      internal static const FlagNextTypePause:int = 4;
      
      private var m_callback:Callback = null;
      
      private var m_button_list:ButtonList = null;
      
      private var m_tab_list:ButtonList = null;
      
      private var m_timeline:MovieClip = null;
      
      private var m_iconsUpdater:IconsUpdater = null;
      
      private var m_select_edit_type:int;
      
      private var m_select_tab_type:int;
      
      private var m_select_edit_menu:int;
      
      private var m_select_color:int;
      
      private var m_flag_next:int;
      
      private var m_is_enable_pause:Boolean = false;
      
      private var m_is_request_quit_menu:Boolean = false;
      
      private var m_is_start_wait:Boolean = false;
      
      public function Avatar()
      {
         super();
         this.m_callback = new Callback(ReceiveTypeNum);
         this.m_button_list = new ButtonList(ButtonMax);
         this.m_tab_list = new ButtonList(EditMenuTabNum);
         this.m_timeline = null;
         this.m_iconsUpdater = new IconsUpdater();
         try
         {
            ExternalInterface.addCallback("CallQuitMenu",this.CallQuitMenu);
            ExternalInterface.addCallback("UpdateIcons",this.UpdateIcons);
         }
         catch(e:Error)
         {
            trace("addCallback - Error");
         }
         this.m_select_edit_type = EditTypeMenuItem;
         this.m_select_tab_type = 0;
         this.m_select_edit_menu = 0;
         this.m_select_color = 0;
         this.m_flag_next = FlagNextTypeBack;
         this.m_is_start_wait = false;
      }
      
      public function Initialize(param1:MovieClip) : void
      {
         this.m_timeline = param1;
         this.m_timeline.visible = false;
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitStart) as MovieClip;
         var _loc2_:MovieClip = this.m_timeline.edit.editmenu.tab._CMN_M_B_mc_l1;
         var _loc3_:MovieClip = this.m_timeline.edit.editmenu.tab._CMN_M_B_mc_r1;
         this.m_iconsUpdater.AddButton(_loc2_.getChildAt(3) as MovieClip,_loc2_.getChildAt(2) as MovieClip,_loc2_.getChildAt(1) as MovieClip,_loc2_.getChildAt(0) as MovieClip);
         this.m_iconsUpdater.AddButton(_loc3_.getChildAt(3) as MovieClip,_loc3_.getChildAt(2) as MovieClip,_loc3_.getChildAt(1) as MovieClip,_loc3_.getChildAt(0) as MovieClip);
      }
      
      public function UpdateIcons(param1:int, param2:int, param3:int, param4:int, param5:int, param6:int) : void
      {
         var _loc7_:int = this.m_callback.GetUserDataInt(ReceiveTypeModeType);
         if(_loc7_ == ModeTypeCustomizeSkill || _loc7_ == ModeTypeCustomize)
         {
            this.UpdateSkillCustMenu();
         }
         this.m_iconsUpdater.updateButtons(param1,param2,param3,param4,param5,param6);
      }
      
      private function UpdateSkillCustMenu() : *
      {
         var _loc4_:MovieClip = null;
         var _loc5_:int = 0;
         var _loc6_:String = null;
         var _loc7_:int = 0;
         var _loc8_:Boolean = false;
         var _loc9_:int = 0;
         var _loc10_:int = 0;
         var _loc11_:String = null;
         var _loc12_:int = 0;
         var _loc13_:Array = null;
         var _loc1_:Array = this.getMenuTabInfoList(this.m_select_tab_type);
         if(!_loc1_)
         {
            return;
         }
         var _loc2_:Array = _loc1_[EditMenuTabInfoTypeSetupInfoList];
         if(!_loc2_)
         {
            return;
         }
         var _loc3_:int = 0;
         for(; EditMenuItemNum > _loc3_; _loc3_++)
         {
            _loc4_ = null;
            if(_loc2_.length <= _loc3_)
            {
               _loc4_ = this.getButtonEditMenu(_loc3_);
               if(!_loc4_)
               {
                  return;
               }
               _loc4_.visible = false;
               continue;
            }
            _loc5_ = int(_loc2_[_loc3_]);
            _loc4_ = this.getButtonEditMenu(_loc3_);
            if(!_loc4_)
            {
               return;
            }
            _loc6_ = EditMenuItemInfoList[_loc5_][EditMenuItemInfoTypeHeader];
            _loc7_ = int(EditMenuItemInfoList[_loc5_][EditMenuItemInfoTypeItemType]);
            _loc8_ = Boolean(EditMenuItemInfoList[_loc5_][EditMenuItemInfoTypeFlagActive]);
            _loc9_ = int(EditMenuItemInfoList[_loc5_][EditMenuItemInfoTypeReceiveTypeData]);
            _loc10_ = int(EditMenuItemInfoList[_loc5_][EditMenuItemInfoTypeReceiveTypeNum]);
            _loc11_ = this.m_callback.GetUserDataString(_loc9_);
            _loc12_ = this.m_callback.GetUserDataInt(_loc10_);
            if(_loc12_ <= 0)
            {
               _loc11_ = "";
               _loc8_ = false;
            }
            _loc4_.visible = true;
            Utility.SetAutoSizedText(_loc4_.sys_header.sys,_loc6_);
            _loc13_ = null;
            switch(_loc6_)
            {
               case StringList.MenuItem_SkillUlt1:
               case StringList.MenuItem_SkillUlt2:
               case StringList.MenuItem_SkillSpa1:
               case StringList.MenuItem_SkillSpa2:
               case StringList.MenuItem_SkillSpa3:
               case StringList.MenuItem_SkillSpa4:
               case StringList.MenuItem_SkillEsc:
               case StringList.MenuItem_SkillMet:
                  _loc13_ = _loc11_.split("^");
                  if(2 < _loc13_.length)
                  {
                     _loc4_.sys_header_imtg.sys.htmlText = _loc13_[0];
                     _loc4_.sys_cont_imtg.sys.htmlText = _loc13_[1];
                     _loc11_ = _loc13_[2];
                  }
            }
         }
      }
      
      private function waitStart(param1:Event) : void
      {
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveTypeModeType);
         switch(_loc2_)
         {
            case ModeTypeWait:
               return;
            default:
               this.Start();
               this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStart);
               return;
         }
      }
      
      public function Start() : void
      {
         this.m_timeline.visible = true;
         this.m_timeline.edit.visible = false;
         this.m_flag_next = FlagNextTypeNext;
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveTypeModeType);
         this.reflectEditMode(_loc1_);
         this.m_is_enable_pause = false;
         if(this.m_callback.GetUserDataValidFlag(ReceiveTypeModeOptionEnablePause))
         {
            this.m_is_enable_pause = this.m_callback.GetUserDataInt(ReceiveTypeModeOptionEnablePause) != 0;
         }
         this.requestEditMenu(false);
      }
      
      private function reflectEditMode(param1:int) : void
      {
         var _loc2_:int = int(ModeFrameInfoList[param1]);
         if(0 < _loc2_)
         {
            this.m_timeline.edit.editmenu.gotoAndStop(_loc2_);
         }
      }
      
      private function setEditType(param1:int) : void
      {
         if(param1 >= EditTypeNum || param1 < 0)
         {
            return;
         }
         this.m_select_edit_type = param1;
      }
      
      private function createMenuTab() : void
      {
         var _loc3_:MovieClip = null;
         var _loc4_:Array = null;
         var _loc5_:String = null;
         var _loc1_:int = 0;
         if(this.m_callback.GetUserDataValidFlag(ReceiveTypeStartPage))
         {
            _loc1_ = this.m_callback.GetUserDataInt(ReceiveTypeStartPage);
         }
         this.m_tab_list.ClearButton();
         var _loc2_:int = 0;
         while(EditMenuTabNum > _loc2_)
         {
            _loc3_ = this.getMenuTab(_loc2_);
            if(_loc3_)
            {
               _loc3_.gotoAndPlay("off");
               _loc4_ = this.getMenuTabInfoList(_loc2_);
               if(_loc4_)
               {
                  _loc5_ = _loc4_[EditMenuTabInfoTypeTabText];
                  if(_loc5_)
                  {
                     _loc3_.sys_tb.sys.text = _loc5_;
                  }
                  this.m_tab_list.AddButton(_loc3_,_loc2_);
               }
            }
            _loc2_++;
         }
         this.m_tab_list.ChangeButton(_loc1_);
         this.setSelectTabType(_loc1_);
      }
      
      private function setSelectTabType(param1:int) : *
      {
         if(param1 < 0 || EditMenuTabNum <= param1)
         {
            return;
         }
         this.m_select_tab_type = param1;
      }
      
      private function changeMenuTabNext() : Boolean
      {
         if(this.m_tab_list.GetButtonNum() < 2)
         {
            return false;
         }
         this.m_tab_list.SelectNextButton();
         this.m_timeline.edit.editmenu.tab._CMN_M_B_mc_r1.gotoAndPlay("push");
         var _loc1_:int = this.m_tab_list.GetSelectUserData();
         this.setSelectTabType(_loc1_);
         return true;
      }
      
      private function changeMenuTabPrev() : Boolean
      {
         if(this.m_tab_list.GetButtonNum() < 2)
         {
            return false;
         }
         this.m_tab_list.SelectPrevButton();
         this.m_timeline.edit.editmenu.tab._CMN_M_B_mc_l1.gotoAndPlay("push");
         var _loc1_:int = this.m_tab_list.GetSelectUserData();
         this.setSelectTabType(_loc1_);
         return true;
      }
      
      private function getMenuTabInfoList(param1:int) : Array
      {
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveTypeModeType);
         if(EditMenuTabInfoList[_loc2_].length <= param1)
         {
            return null;
         }
         return EditMenuTabInfoList[_loc2_][param1];
      }
      
      private function getMenuTab(param1:int) : MovieClip
      {
         if(param1 < 0 || EditMenuTabNum <= param1)
         {
            return null;
         }
         if(!this.m_timeline.edit.editmenu.tab)
         {
            return null;
         }
         return this.m_timeline.edit.editmenu.tab["tab_" + (param1 + 1)];
      }
      
      private function getButtonEditMenu(param1:int) : MovieClip
      {
         if(param1 < 0 || param1 >= EditMenuItemNum)
         {
            return null;
         }
         return this.m_timeline.edit.editmenu.edit_menu["btn_" + (param1 + 1)];
      }
      
      private function setupEditMenuPart(param1:int, param2:int, param3:String, param4:String, param5:int, param6:Boolean) : void
      {
         var _loc7_:MovieClip = this.getButtonEditMenu(param1);
         if(!_loc7_)
         {
            return;
         }
         if(param5 <= 0)
         {
            param4 = "";
            param6 = false;
         }
         _loc7_.visible = true;
         Utility.SetAutoSizedText(_loc7_.sys_header.sys,param3);
         var _loc8_:Array = null;
         switch(param3)
         {
            case StringList.MenuItem_ClothTop:
            case StringList.MenuItem_ClothBottom:
            case StringList.MenuItem_ClothGloves:
            case StringList.MenuItem_ClothShoes:
            case StringList.MenuItem_ClothAccessory:
            case StringList.MenuItem_ClothTalisman:
            case StringList.MenuItem_ClothMaterial:
               _loc8_ = param4.split("^");
               if(1 < _loc8_.length)
               {
                  _loc7_.sys_header_imtg.sys.htmlText = "";
                  _loc7_.sys_cont_imtg.sys.htmlText = _loc8_[0];
                  param4 = _loc8_[1];
               }
               break;
            case StringList.MenuItem_SkillUlt1:
            case StringList.MenuItem_SkillUlt2:
            case StringList.MenuItem_SkillSpa1:
            case StringList.MenuItem_SkillSpa2:
            case StringList.MenuItem_SkillSpa3:
            case StringList.MenuItem_SkillSpa4:
            case StringList.MenuItem_SkillEsc:
            case StringList.MenuItem_SkillMet:
               _loc8_ = param4.split("^");
               if(2 < _loc8_.length)
               {
                  _loc7_.sys_header_imtg.sys.htmlText = _loc8_[0];
                  _loc7_.sys_cont_imtg.sys.htmlText = _loc8_[1];
                  param4 = _loc8_[2];
               }
               break;
            default:
               _loc7_.sys_header_imtg.sys.htmlText = "";
               _loc7_.sys_cont_imtg.sys.htmlText = "";
         }
         Utility.SetAutoSizedhtmlText(_loc7_.sys_cont.sys,param4);
         _loc7_.sys_header_imtg.visible = true;
         _loc7_.sys_cont_imtg.visible = true;
         _loc7_.sys_cont.visible = true;
         _loc7_.parts.visible = false;
         _loc7_.arw_l.visible = false;
         _loc7_.arw_r.visible = false;
         _loc7_.btn_height.visible = false;
         _loc7_.btn_body.visible = false;
         _loc7_.gotoAndPlay("inv");
         if(!param6)
         {
            return;
         }
         _loc7_.gotoAndPlay("off");
         this.m_button_list.AddButton(_loc7_,param2);
         if(this.m_select_edit_menu == param2)
         {
            this.m_button_list.ChangeButton(this.m_button_list.GetButtonNum() - 1);
         }
      }
      
      private function setupEditMenuColor(param1:int, param2:int, param3:String, param4:Array, param5:Boolean) : void
      {
         var _loc6_:MovieClip = this.getButtonEditMenu(param1);
         if(!_loc6_)
         {
            return;
         }
         if(param4.length <= 0)
         {
            param5 = false;
         }
         _loc6_.visible = true;
         Utility.SetAutoSizedText(_loc6_.sys_header.sys,param3);
         _loc6_.sys_header_imtg.visible = false;
         _loc6_.sys_cont_imtg.visible = false;
         _loc6_.sys_cont.visible = false;
         _loc6_.parts.visible = true;
         if(param4.length <= 1)
         {
            _loc6_.arw_l.visible = false;
            _loc6_.arw_r.visible = false;
         }
         else
         {
            _loc6_.arw_l.visible = true;
            _loc6_.arw_r.visible = true;
         }
         _loc6_.btn_height.visible = false;
         _loc6_.btn_body.visible = false;
         _loc6_.gotoAndPlay("inv");
         if(!param5)
         {
            _loc6_.parts.visible = false;
            return;
         }
         _loc6_.parts.gotoAndStop(param4.length + "parts");
         var _loc7_:int = 0;
         var _loc8_:int = 0;
         while(_loc8_ < param4.length)
         {
            if(this.setEditMenuButtonColor(_loc6_,_loc8_,param4[_loc8_]))
            {
               _loc7_++;
            }
            _loc8_++;
         }
         if(_loc7_ == 0)
         {
            return;
         }
         if(_loc7_ == 1)
         {
            _loc6_.arw_l.visible = false;
            _loc6_.arw_r.visible = false;
         }
         _loc6_.gotoAndPlay("off");
         _loc6_.user_add_property_list_num = param4.length;
         this.m_button_list.AddButton(_loc6_,param2);
         if(this.m_select_edit_menu == param2)
         {
            this.m_button_list.ChangeButton(this.m_button_list.GetButtonNum() - 1);
            this.changeEditMenuColor(0);
         }
      }
      
      private function setEditMenuButtonColor(param1:MovieClip, param2:int, param3:uint) : Boolean
      {
         var _loc6_:ColorTransform = null;
         var _loc4_:MovieClip = param1.parts["palette_mini" + (param2 + 1)];
         var _loc5_:Boolean = 16777215 < param3;
         if(_loc5_)
         {
            _loc4_.plt1.visible = false;
            _loc4_.plt2.visible = true;
            _loc4_.coloring.visible = false;
         }
         else
         {
            _loc4_.plt1.visible = true;
            _loc4_.plt2.visible = false;
            _loc4_.coloring.visible = true;
            _loc6_ = new ColorTransform();
            _loc6_.color = param3;
            _loc4_.coloring.transform.colorTransform = _loc6_;
         }
         _loc4_.gotoAndPlay("off");
         if(_loc5_)
         {
            _loc4_.user_add_property_is_valid = false;
         }
         else
         {
            _loc4_.user_add_property_is_valid = true;
         }
         return _loc4_.user_add_property_is_valid;
      }
      
      private function changeEditMenuColor(param1:int) : Boolean
      {
         var _loc2_:MovieClip = this.m_button_list.GetButton(this.m_button_list.GetSelectButton());
         if(!_loc2_)
         {
            return false;
         }
         var _loc3_:int = this.m_select_color;
         var _loc4_:int = int(_loc2_.user_add_property_list_num);
         var _loc5_:int = param1;
         var _loc6_:Boolean = false;
         do
         {
            _loc3_ += _loc5_;
            if(_loc4_ <= _loc3_)
            {
               _loc3_ = 0;
            }
            else if(_loc3_ < 0)
            {
               _loc3_ = _loc4_ - 1;
            }
            _loc6_ = Boolean(_loc2_.parts["palette_mini" + (_loc3_ + 1)].user_add_property_is_valid);
            if(param1 == 0)
            {
               _loc5_ = 1;
            }
         }
         while(!_loc6_);
         if(param1 != 0 && _loc3_ == this.m_select_color)
         {
            return false;
         }
         if(0 < param1)
         {
            _loc2_.gotoAndPlay("push_r");
         }
         else if(param1 < 0)
         {
            _loc2_.gotoAndPlay("push_l");
         }
         _loc2_.parts["palette_mini" + (this.m_select_color + 1)].gotoAndStop("off");
         _loc2_.parts["palette_mini" + (_loc3_ + 1)].gotoAndPlay("on");
         this.m_select_color = _loc3_;
         return true;
      }
      
      private function setupEditMenuDeform(param1:int, param2:int, param3:String, param4:int, param5:int, param6:Boolean) : void
      {
         var _loc7_:MovieClip = this.getButtonEditMenu(param1);
         if(!_loc7_)
         {
            return;
         }
         _loc7_.visible = true;
         Utility.SetAutoSizedText(_loc7_.sys_header.sys,param3);
         _loc7_.sys_header_imtg.visible = false;
         _loc7_.sys_cont_imtg.visible = false;
         _loc7_.sys_cont.visible = false;
         _loc7_.parts.visible = false;
         _loc7_.arw_l.visible = false;
         _loc7_.arw_r.visible = false;
         if(0 < param4)
         {
            _loc7_.arw_l.visible = true;
         }
         if(param4 < param5 - 1)
         {
            _loc7_.arw_r.visible = true;
         }
         _loc7_.btn_height.visible = false;
         _loc7_.btn_body.visible = false;
         var _loc8_:MovieClip = null;
         if(param5 == 3)
         {
            _loc8_ = _loc7_.btn_body;
         }
         else if(param5 == 4)
         {
            _loc8_ = _loc7_.btn_height;
         }
         if(_loc8_)
         {
            _loc8_.visible = true;
            _loc8_.gotoAndStop(param4 + 1);
         }
         _loc7_.gotoAndPlay("off");
         if(!param6)
         {
            return;
         }
         this.m_button_list.AddButton(_loc7_,param2);
         if(this.m_select_edit_menu == param2)
         {
            this.m_button_list.ChangeButton(this.m_button_list.GetButtonNum() - 1);
         }
      }
      
      private function changeEditMenuDeform(param1:int) : Boolean
      {
         var _loc2_:MovieClip = this.m_button_list.GetButton(this.m_button_list.GetSelectButton());
         if(!_loc2_)
         {
            return false;
         }
         var _loc3_:int = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeReceiveTypeData]);
         var _loc4_:int = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeReceiveTypeNum]);
         var _loc5_:int = this.m_callback.GetUserDataInt(_loc3_);
         var _loc6_:int = _loc5_ + param1;
         if(_loc6_ < 0)
         {
            _loc6_ = 0;
         }
         else if(_loc4_ <= _loc6_)
         {
            _loc6_ = _loc4_ - 1;
         }
         if(_loc6_ == _loc5_)
         {
            return false;
         }
         var _loc7_:MovieClip = null;
         if(_loc4_ == 3)
         {
            _loc7_ = _loc2_.btn_body;
         }
         else if(_loc4_ == 4)
         {
            _loc7_ = _loc2_.btn_height;
         }
         if(!_loc7_)
         {
            return false;
         }
         _loc7_.gotoAndStop(_loc6_ + 1);
         if(0 < param1)
         {
            _loc2_.gotoAndPlay("push_r");
         }
         else if(param1 < 0)
         {
            _loc2_.gotoAndPlay("push_l");
         }
         _loc2_.arw_l.visible = false;
         _loc2_.arw_r.visible = false;
         if(0 < _loc6_)
         {
            _loc2_.arw_l.visible = true;
         }
         if(_loc6_ < _loc4_ - 1)
         {
            _loc2_.arw_r.visible = true;
         }
         this.SetUserDataInt(_loc3_,_loc6_);
         var _loc8_:int = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeRequestType]);
         this.m_callback.CallbackUserData("user",_loc8_,_loc6_);
         return true;
      }
      
      private function setupEditMenuItemBase(param1:Array) : void
      {
         var _loc3_:int = 0;
         var _loc4_:int = 0;
         var _loc5_:Boolean = false;
         var _loc6_:String = null;
         var _loc7_:int = 0;
         var _loc8_:int = 0;
         var _loc9_:MovieClip = null;
         var _loc10_:String = null;
         var _loc11_:int = 0;
         var _loc12_:Array = null;
         var _loc13_:String = null;
         var _loc14_:int = 0;
         var _loc15_:int = 0;
         this.m_button_list.ClearButton();
         var _loc2_:int = 0;
         for(; EditMenuItemNum > _loc2_; _loc2_++)
         {
            if(param1.length <= _loc2_)
            {
               _loc9_ = this.getButtonEditMenu(_loc2_);
               if(!_loc9_)
               {
                  return;
               }
               _loc9_.visible = false;
               continue;
            }
            _loc3_ = int(param1[_loc2_]);
            _loc4_ = int(EditMenuItemInfoList[_loc3_][EditMenuItemInfoTypeItemType]);
            _loc5_ = Boolean(EditMenuItemInfoList[_loc3_][EditMenuItemInfoTypeFlagActive]);
            _loc6_ = EditMenuItemInfoList[_loc3_][EditMenuItemInfoTypeHeader];
            _loc7_ = int(EditMenuItemInfoList[_loc3_][EditMenuItemInfoTypeReceiveTypeData]);
            _loc8_ = int(EditMenuItemInfoList[_loc3_][EditMenuItemInfoTypeReceiveTypeNum]);
            switch(_loc4_)
            {
               case EditMenuItemTypePart:
                  _loc10_ = this.m_callback.GetUserDataString(_loc7_);
                  _loc11_ = this.m_callback.GetUserDataInt(_loc8_);
                  this.setupEditMenuPart(_loc2_,_loc3_,_loc6_,_loc10_,_loc11_,_loc5_);
                  break;
               case EditMenuItemTypeColor:
                  _loc12_ = new Array(this.m_callback.GetUserDataInt(_loc8_));
                  _loc15_ = 0;
                  while(_loc15_ < _loc12_.length)
                  {
                     _loc12_[_loc15_] = this.m_callback.GetUserDataInt(_loc7_ + _loc15_) as uint;
                     _loc15_++;
                  }
                  this.setupEditMenuColor(_loc2_,_loc3_,_loc6_,_loc12_,_loc5_);
                  break;
               case EditMenuItemTypeInput:
                  _loc13_ = Utility.GetNotReplacedString(this.m_callback.GetUserDataString(_loc7_));
                  this.setupEditMenuPart(_loc2_,_loc3_,_loc6_,_loc13_,1,_loc5_);
                  break;
               case EditMenuItemTypeDeform:
                  _loc14_ = this.m_callback.GetUserDataInt(_loc7_);
                  this.setupEditMenuDeform(_loc2_,_loc3_,_loc6_,_loc14_,_loc8_,_loc5_);
            }
         }
      }
      
      private function setupEditMenuItem() : void
      {
         var _loc1_:Array = this.getMenuTabInfoList(this.m_select_tab_type);
         if(!_loc1_)
         {
            return;
         }
         var _loc2_:Array = _loc1_[EditMenuTabInfoTypeSetupInfoList];
         if(!_loc2_)
         {
            return;
         }
         this.setupEditMenuItemBase(_loc2_);
         if(this.m_button_list.GetSelectButton() < 0 && 0 < this.m_button_list.GetButtonNum())
         {
            this.m_button_list.ChangeButton(0);
            this.changeEditMenuItem();
         }
      }
      
      private function changeEditMenuItem() : void
      {
         var _loc1_:int = 0;
         var _loc3_:int = 0;
         var _loc4_:MovieClip = null;
         _loc1_ = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeItemType]);
         if(_loc1_ == EditMenuItemTypeColor)
         {
            _loc3_ = this.m_button_list.SearchButton(this.m_select_edit_menu,0);
            _loc4_ = this.m_button_list.GetButton(_loc3_);
            if(_loc4_)
            {
               _loc4_.parts["palette_mini" + (this.m_select_color + 1)].gotoAndPlay("off");
            }
         }
         var _loc2_:int = this.m_button_list.GetSelectUserData();
         if(0 <= _loc2_ && _loc2_ < EditMenuTypeNum)
         {
            this.m_select_edit_menu = _loc2_;
         }
         _loc1_ = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeItemType]);
         if(_loc1_ == EditMenuItemTypeColor)
         {
            this.changeEditMenuColor(0);
         }
      }
      
      private function checkEditMenuItemValid() : Boolean
      {
         if(this.m_select_edit_menu != this.m_button_list.GetSelectUserData())
         {
            return false;
         }
         var _loc1_:int = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeItemType]);
         if(_loc1_ == EditMenuItemTypeDeform)
         {
            return false;
         }
         return true;
      }
      
      private function changeEditMenuItemNext() : Boolean
      {
         var _loc1_:Boolean = false;
         var _loc2_:int = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeItemType]);
         switch(_loc2_)
         {
            case EditMenuItemTypeColor:
               _loc1_ = this.changeEditMenuColor(1);
               break;
            case EditMenuItemTypeDeform:
               _loc1_ = this.changeEditMenuDeform(1);
         }
         return _loc1_;
      }
      
      private function changeEditMenuItemPrev() : Boolean
      {
         var _loc1_:Boolean = false;
         var _loc2_:int = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeItemType]);
         switch(_loc2_)
         {
            case EditMenuItemTypeColor:
               _loc1_ = this.changeEditMenuColor(-1);
               break;
            case EditMenuItemTypeDeform:
               _loc1_ = this.changeEditMenuDeform(-1);
         }
         return _loc1_;
      }
      
      private function requestEditMenu(param1:Boolean = true) : void
      {
         var _loc2_:Array = null;
         var _loc3_:int = 0;
         if(param1)
         {
            _loc2_ = this.getMenuTabInfoList(this.m_select_tab_type);
            if(_loc2_)
            {
               _loc3_ = int(_loc2_[EditMenuTabInfoTypeRequestType]);
               this.m_callback.SetUserDataValidFlag(ReceiveTypeModeType,false);
               this.m_callback.CallbackUserData("user",_loc3_,-1);
            }
         }
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitReceiveEditMenu);
      }
      
      private function waitReceiveEditMenu(param1:Event) : void
      {
         if(!this.checkReceiveEditMenu())
         {
            return;
         }
         this.startEditMenuItem();
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitReceiveEditMenu);
      }
      
      private function checkReceiveEditMenu() : Boolean
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveTypeModeType))
         {
            return false;
         }
         return true;
      }
      
      private function startEditMenuItem() : void
      {
         this.m_timeline.edit.visible = true;
         var _loc1_:Array = null;
         if(this.m_flag_next == FlagNextTypeNext)
         {
            this.m_timeline.edit.gotoAndPlay("start");
            this.createMenuTab();
            _loc1_ = this.getMenuTabInfoList(this.m_select_tab_type);
            if(_loc1_)
            {
               this.m_select_edit_menu = _loc1_[EditMenuTabInfoTypeSetupInfoList][0];
            }
            this.m_timeline.edit.editmenu.edit_menu.gotoAndPlay("in");
         }
         else if(this.m_flag_next == FlagNextTypeChangeTab)
         {
            this.m_timeline.edit.editmenu.edit_menu.gotoAndPlay("in");
            _loc1_ = this.getMenuTabInfoList(this.m_select_tab_type);
            if(_loc1_)
            {
               this.m_select_edit_menu = _loc1_[EditMenuTabInfoTypeSetupInfoList][0];
            }
            this.m_select_color = 0;
         }
         else if(this.m_flag_next == FlagNextTypeBack)
         {
         }
         this.setEditType(EditTypeMenuItem);
         this.m_flag_next = FlagNextTypeBack;
         this.setupEditMenuItem();
         this.m_timeline.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitEditMenuItem);
      }
      
      private function waitEditMenuItem() : void
      {
         trace("waitEditMenuItem");
         this.m_is_start_wait = true;
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEditMenuItem);
      }
      
      private function endEditMenuItem() : void
      {
         var _loc1_:int = 0;
         var _loc2_:int = 0;
         this.m_callback.SetUserDataValidFlag(ReceiveTypeConfirmResponse,false);
         if(this.m_flag_next == FlagNextTypeBack || this.m_flag_next == FlagNextTypeFinish)
         {
            _loc1_ = -1;
            _loc2_ = this.m_callback.GetUserDataInt(ReceiveTypeModeType);
            switch(_loc2_)
            {
               case ModeTypeCreateFigure:
               case ModeTypeCreateName:
               case ModeTypeReincarnationFigure:
               case ModeTypeReincarnationVoice:
                  this.SetUserDataInt(ReceiveTypeConfirmResponse,1);
                  break;
               case ModeTypeCustomizeCloth:
                  if(this.m_flag_next == FlagNextTypeBack)
                  {
                     _loc1_ = ConfirmTypeCancelCustomizeCloth;
                  }
                  else
                  {
                     _loc1_ = ConfirmTypeFinishCustomizeCloth;
                  }
                  break;
               case ModeTypeCustomizeSkill:
                  if(this.m_flag_next == FlagNextTypeBack)
                  {
                     _loc1_ = ConfirmTypeCancelCustomizeSkill;
                  }
                  else
                  {
                     _loc1_ = ConfirmTypeFinishCustomizeSkill;
                  }
                  break;
               case ModeTypeCustomize:
                  this.SetUserDataInt(ReceiveTypeConfirmResponse,1);
                  break;
               case ModeTypeBarber:
                  this.SetUserDataInt(ReceiveTypeConfirmResponse,1);
            }
            if(0 <= _loc1_)
            {
               this.m_callback.CallbackUserData("user",SendTypeRequestConfirmType,_loc1_);
            }
         }
         else if(this.m_flag_next == FlagNextTypePause)
         {
            this.SetUserDataInt(ReceiveTypeConfirmResponse,1);
         }
         else
         {
            this.SetUserDataInt(ReceiveTypeConfirmResponse,1);
         }
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitConfirmEndEditMenuItem);
      }
      
      private function checkConfirmEndEditMenuItem() : Boolean
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveTypeConfirmResponse))
         {
            return false;
         }
         return true;
      }
      
      private function waitConfirmEndEditMenuItem(param1:Event) : void
      {
         var _loc2_:MovieClip = null;
         if(!this.checkConfirmEndEditMenuItem())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitConfirmEndEditMenuItem);
         if(!this.m_callback.GetUserDataInt(ReceiveTypeConfirmResponse))
         {
            _loc2_ = this.m_button_list.GetButton(this.m_button_list.GetSelectButton());
            if(_loc2_)
            {
               _loc2_.gotoAndPlay("on");
            }
            this.m_timeline.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
            return;
         }
         if(this.m_flag_next == FlagNextTypeBack || this.m_flag_next == FlagNextTypeFinish)
         {
            this.m_timeline.edit.gotoAndPlay("end");
            this.m_timeline.edit.editmenu.edit_menu.gotoAndPlay("out");
         }
         else if(this.m_flag_next != FlagNextTypeNext)
         {
            if(this.m_flag_next == FlagNextTypeChangeTab)
            {
               this.m_timeline.edit.editmenu.edit_menu.gotoAndPlay("out");
            }
            else if(this.m_flag_next == FlagNextTypePause)
            {
            }
         }
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitCompEditMenuItem);
      }
      
      private function checkCompEditMenuItem() : Boolean
      {
         if(this.m_timeline.edit.editmenu.edit_menu.currentLabel == "out")
         {
            return false;
         }
         return true;
      }
      
      private function waitCompEditMenuItem(param1:Event) : void
      {
         var _loc2_:int = 0;
         if(!this.checkCompEditMenuItem())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitCompEditMenuItem);
         if(this.m_flag_next == FlagNextTypeNext)
         {
            _loc2_ = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeItemType]);
            switch(_loc2_)
            {
               case EditMenuItemTypePart:
                  this.startEditMenuList();
                  break;
               case EditMenuItemTypeColor:
                  this.startEditMenuColor();
                  break;
               case EditMenuItemTypeInput:
                  this.startEditMenuInput();
            }
         }
         else if(this.m_flag_next == FlagNextTypeChangeTab)
         {
            this.requestEditMenu();
         }
         else if(this.m_flag_next == FlagNextTypeFinish)
         {
            if(this.m_callback.GetUserDataInt(ReceiveTypeConfirmResponse) == 1)
            {
               this.m_callback.CallbackDecide(0);
            }
            else
            {
               this.m_callback.CallbackCancel();
            }
            this.m_callback.CallbackExit();
         }
         else if(this.m_flag_next == FlagNextTypePause)
         {
            this.startEditMenuPause();
         }
         else
         {
            this.m_callback.CallbackCancel();
            this.m_callback.CallbackExit();
         }
      }
      
      private function startEditMenuList() : void
      {
         this.setEditType(EditTypeMenuList);
         var _loc1_:int = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeReceiveTypeData]);
         this.m_callback.SetUserDataValidFlag(_loc1_,false);
         var _loc2_:int = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeRequestType]);
         this.m_callback.CallbackUserData("user",_loc2_,-1);
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitEditMenuList);
      }
      
      private function waitEditMenuList(param1:Event) : void
      {
         if(!this.checkEditMenuList())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEditMenuList);
         this.m_flag_next = FlagNextTypeBack;
         this.startEditMenuItem();
      }
      
      private function checkEditMenuList() : Boolean
      {
         var _loc1_:int = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeReceiveTypeData]);
         if(!this.m_callback.GetUserDataValidFlag(_loc1_))
         {
            return false;
         }
         return true;
      }
      
      private function startEditMenuColor() : void
      {
         this.setEditType(EditTypeMenuColor);
         var _loc1_:int = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeReceiveTypeData]);
         _loc1_ += this.m_select_color;
         this.m_callback.SetUserDataValidFlag(_loc1_,false);
         var _loc2_:int = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeRequestType]);
         this.m_callback.CallbackUserData("user",_loc2_,this.m_select_color);
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitEditMenuColor);
      }
      
      private function waitEditMenuColor(param1:Event) : void
      {
         if(!this.checkEditMenuColor())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEditMenuColor);
         this.m_flag_next = FlagNextTypeBack;
         this.startEditMenuItem();
      }
      
      private function checkEditMenuColor() : Boolean
      {
         var _loc1_:int = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeReceiveTypeData]);
         _loc1_ += this.m_select_color;
         if(!this.m_callback.GetUserDataValidFlag(_loc1_))
         {
            return false;
         }
         return true;
      }
      
      private function startEditMenuInput() : void
      {
         this.setEditType(EditTypeMenuInput);
         var _loc1_:int = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeReceiveTypeData]);
         this.m_callback.SetUserDataValidFlag(_loc1_,false);
         var _loc2_:int = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeRequestType]);
         this.m_callback.CallbackUserData("user",_loc2_,-1);
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitEditMenuInput);
      }
      
      private function waitEditMenuInput(param1:Event) : void
      {
         if(!this.checkEditMenuInput())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEditMenuInput);
         this.m_flag_next = FlagNextTypeBack;
         this.startEditMenuItem();
      }
      
      private function checkEditMenuInput() : Boolean
      {
         var _loc1_:int = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeReceiveTypeData]);
         if(!this.m_callback.GetUserDataValidFlag(_loc1_))
         {
            return false;
         }
         return true;
      }
      
      private function startEditMenuPause() : void
      {
         this.m_callback.CallbackUserData("user",SendTypeRequestEditPause,-1);
         this.m_callback.SetUserDataValidFlag(ReceiveTypePauseEnd,false);
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitEditMenuPause);
      }
      
      private function waitEditMenuPause(param1:Event) : void
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveTypePauseEnd))
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEditMenuPause);
         this.m_flag_next = FlagNextTypeBack;
         this.startEditMenuItem();
      }
      
      private function pushU() : void
      {
         this.m_button_list.SelectPrevButton();
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
         this.changeEditMenuItem();
      }
      
      private function pushD() : void
      {
         this.m_button_list.SelectNextButton();
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
         this.changeEditMenuItem();
      }
      
      private function pushL() : void
      {
         if(this.changeEditMenuItemPrev())
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
         }
      }
      
      private function pushR() : void
      {
         if(this.changeEditMenuItemNext())
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
         }
      }
      
      private function pushTabL() : void
      {
         if(this.changeMenuTabPrev())
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
            this.m_flag_next = FlagNextTypeChangeTab;
            this.endEditMenuItem();
         }
      }
      
      private function pushTabR() : void
      {
         if(this.changeMenuTabNext())
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
            this.m_flag_next = FlagNextTypeChangeTab;
            this.endEditMenuItem();
         }
      }
      
      private function pushX() : void
      {
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveTypeModeType);
         if(_loc1_ == ModeTypeBarber)
         {
            this.m_callback.CallbackUserData("user",SendTypeRequestEditReset,-1);
            this.requestEditMenu();
         }
         if(_loc1_ == ModeTypeCustomizeSkill)
         {
            if(this.m_is_enable_pause)
            {
               this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
               this.m_flag_next = FlagNextTypePause;
               this.endEditMenuItem();
            }
         }
      }
      
      private function pushEnter() : void
      {
         if(this.checkEditMenuItemValid())
         {
            this.m_button_list.PushButton();
            this.m_callback.CallbackSe(this.m_callback.SeTypeDecide);
            this.m_flag_next = FlagNextTypeNext;
            this.endEditMenuItem();
         }
      }
      
      private function pushCancel() : void
      {
         this.m_callback.CallbackSe(this.m_callback.SeTypeCancel);
         this.m_flag_next = FlagNextTypeFinish;
         this.endEditMenuItem();
      }
      
      private function checkKey(param1:KeyboardEvent) : void
      {
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
            case Keyboard.DELETE:
               this.pushTabL();
               break;
            case Keyboard.PAGE_DOWN:
               this.pushTabR();
               break;
            case Keyboard.ESCAPE:
               this.pushCancel();
               break;
            case 88:
               this.pushX();
               break;
            case Keyboard.ENTER:
               this.pushEnter();
         }
      }
      
      public function SetUserDataInt(param1:int, param2:int) : *
      {
         this.m_callback.AddCallbackSetUserDataInt(param1,param2);
      }
      
      public function SetUserDataString(param1:int, param2:String) : *
      {
         trace("[AVATAR]::setUserDataString = " + param2);
         this.m_callback.AddCallbackSetUserDataString(param1,param2);
      }
      
      public function CallQuitMenu() : void
      {
         if(this.m_is_start_wait && !this.m_is_request_quit_menu)
         {
            this.pushCancel();
            this.m_is_request_quit_menu = true;
         }
      }
      
      public function TestDestroy() : void
      {
         this.m_callback = null;
         this.m_button_list.ClearButton();
         this.m_button_list = null;
         this.m_tab_list.ClearButton();
         this.m_tab_list = null;
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStart);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitReceiveEditMenu);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitConfirmEndEditMenuItem);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitCompEditMenuItem);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEditMenuList);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEditMenuColor);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEditMenuInput);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEditMenuItem);
         this.m_timeline.visible = false;
         this.m_timeline = null;
      }
      
      public function TestCheckUserDataValidFlag(param1:int) : Boolean
      {
         return this.m_callback.GetUserDataValidFlag(param1);
      }
      
      public function TestGetUserDataInt(param1:int) : int
      {
         return this.m_callback.GetUserDataInt(param1);
      }
      
      public function TestGetUserDataString(param1:int) : String
      {
         return this.m_callback.GetUserDataString(param1);
      }
      
      public function TestGetEditType() : int
      {
         return this.m_select_edit_type;
      }
      
      public function TestGetTabRequestType(param1:int) : int
      {
         var _loc2_:Boolean = this.TestCheckUserDataValidFlag(ReceiveTypeModeType);
         var _loc3_:int = this.m_callback.GetUserDataInt(ReceiveTypeModeType);
         this.SetUserDataInt(ReceiveTypeModeType,param1);
         var _loc4_:Array = this.getMenuTabInfoList(this.m_select_tab_type);
         this.SetUserDataInt(ReceiveTypeModeType,_loc3_);
         this.m_callback.SetUserDataValidFlag(ReceiveTypeModeType,_loc2_);
         if(!_loc4_)
         {
            return -1;
         }
         return _loc4_[EditMenuTabInfoTypeRequestType];
      }
      
      public function TestGetItemWaitUserDataType() : int
      {
         var _loc1_:int = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeReceiveTypeData]);
         var _loc2_:int = int(EditMenuItemInfoList[this.m_select_edit_menu][EditMenuItemInfoTypeItemType]);
         if(_loc2_ == EditMenuItemTypeColor)
         {
            _loc1_ += this.m_select_color;
         }
         return _loc1_;
      }
   }
}

