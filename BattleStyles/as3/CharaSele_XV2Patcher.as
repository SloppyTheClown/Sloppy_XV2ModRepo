package action_script
{
   import flash.display.Bitmap;
   import flash.display.DisplayObject;
   import flash.display.DisplayObjectContainer;
   import flash.display.MovieClip;
   import flash.events.Event;
   import flash.events.KeyboardEvent;
   import flash.external.ExternalInterface;
   import flash.geom.Rectangle;
   import flash.text.TextField;
   import flash.ui.Keyboard;
   
   public class CharaSele
   {
      
      private static const ButtonMax:int = 2;

      // BattleStyles: for a CaC the mod appends the picked preset's battle
      // style to the Super Soul name (user data ReceiveType_TarismanNameStr)
      // after a tab; the popup's layout goes to the mod's log once (sendType
      // below, a string).
      internal static const StyleSeparator:String = "\t";

      internal static const SendType_StyleLayoutDump:int = 9901;

      // The header's text ends in a tab, which keeps the game's msg-id
      // replacement off it (as on the Customize screen's Battle Style slot).
      internal static const StyleRowHeader:String = "Battle Style\t";

      // The Super Soul name is drawn this much narrower than the skill names
      // (updateSkill); the style row does the same.
      internal static const TarismanNameShrink:Number = 0.15;
      
      public static const PlayerNumFri:int = 2;
      
      public static const PlayerNumEnm:int = 3;
      
      public static const PlayerMax:int = 1 + PlayerNumFri + PlayerNumEnm;
      
      public static const SkillMax:int = 8;
      
      public static const CharaVarIndexNum:int = 32;
      
      public static const CharacterMax:int = 850;
      
      public static const CustomListMax:int = 640;
      
      public static const CharacterTableData:int = 18;
      
      public static const CharacterTableMax:int = 512;
      
      public static var VarTypeCode:int = 0;
      
      public static var VarTypeMid:int = 1;
      
      public static var VarTypeModelPreset:int = 2;
      
      public static var VarTypeUnlockIndex:int = 3;
      
      public static var VarTypeFlag_GK2:int = 4;
      
      public static var VarTypeVoiceIdList:int = 5;
      
      public static var VarTypeDlcKeyFlag:int = 6;
      
      public static var VarTypeDlcKeyFlag2:int = 7;
      
      public static var VarTypeCustomCostume:int = 8;
      
      public static var VarTypeAvatarSlotID:int = 9;
      
      public static var VarTypeAfterTU9Order:int = 10;
      
      public static var VarTypeCustomCostumeEx:int = 11;
      
      public static var VarTypeFlag_CGK:int = 12;
      
      public static var VarTypeCharaSingleUnlockFlag:int = 13;
      
      public static var VarTypeCharaSingleUnlockFlag2:int = 14;
      
      public static var VarTypeFlag_KFK:int = 15;
      
      public static var VarTypeNum:int = 16;
      
      public static var InvalidCode:String = "";
      
      public static var AvatarCode:String = "HUM";
      
      public static const ReceiveType_FlagUseCancel:int = 0;
      
      public static const ReceiveType_PlayerFriNum:int = 1;
      
      public static const ReceiveType_PlayerEnmNum:int = 2;
      
      public static const ReceiveType_PartyNpcNum:int = 3;
      
      public static const ReceiveType_FlagSelectAvatar:int = 4;
      
      public static const ReceiveType_FlagLocalBattle:int = 5;
      
      public static const ReceiveType_Flag2pController:int = 6;
      
      public static const ReceiveType_Str2pController:int = 7;
      
      public static const ReceiveType_Time:int = 8;
      
      public static const ReceiveType_CharaNameStr:int = 9;
      
      public static const ReceiveType_NameOption_GK2:int = 10;
      
      public static const ReceiveType_NameOption_CGK:int = 11;
      
      public static const ReceiveType_NameOption_KFK:int = 12;
      
      public static const ReceiveType_NameOption_LvText:int = 13;
      
      public static const ReceiveType_NameOption_LvNum:int = 14;
      
      public static const ReceiveType_VariationNameStr:int = 15;
      
      public static const ReceiveType_TarismanHeaderStr:int = 16;
      
      public static const ReceiveType_TarismanNameStr:int = 17;
      
      public static const ReceiveType_ImageStrStart:int = 18;
      
      public static const ReceiveType_ImageStrEnd:int = ReceiveType_ImageStrStart + CharacterMax - 1;
      
      public static const ReceiveType_UnlockVarStart:int = ReceiveType_ImageStrEnd + 1;
      
      public static const ReceiveType_UnlockVarEnd:int = ReceiveType_UnlockVarStart + CharaVarIndexNum * CharacterMax - 1;
      
      public static const ReceiveType_KeyStrL1:int = ReceiveType_UnlockVarEnd + 1;
      
      public static const ReceiveType_KeyStrR1:int = ReceiveType_KeyStrL1 + 1;
      
      public static const ReceiveType_KeyStrL2:int = ReceiveType_KeyStrR1 + 1;
      
      public static const ReceiveType_KeyStrR2:int = ReceiveType_KeyStrL2 + 1;
      
      public static const ReceiveType_KeyStrRU:int = ReceiveType_KeyStrR2 + 1;
      
      public static const ReceiveType_KeyStrRD:int = ReceiveType_KeyStrRU + 1;
      
      public static const ReceiveType_KeyStrRL:int = ReceiveType_KeyStrRD + 1;
      
      public static const ReceiveType_KeyStrRR:int = ReceiveType_KeyStrRL + 1;
      
      public static const ReceiveType_KeyStrSingleLS:int = ReceiveType_KeyStrRR + 1;
      
      public static const ReceiveType_KeyStrSingleRS:int = ReceiveType_KeyStrSingleLS + 1;
      
      public static const ReceiveType_KeyStrSingleU:int = ReceiveType_KeyStrSingleRS + 1;
      
      public static const ReceiveType_KeyStrSingleD:int = ReceiveType_KeyStrSingleU + 1;
      
      public static const ReceiveType_KeyStrSingleL:int = ReceiveType_KeyStrSingleD + 1;
      
      public static const ReceiveType_KeyStrSingleR:int = ReceiveType_KeyStrSingleL + 1;
      
      public static const ReceiveType_SkillNameStrStart:int = ReceiveType_KeyStrSingleR + 1;
      
      public static const ReceiveType_SkillNameStrEnd:int = ReceiveType_SkillNameStrStart + SkillMax - 1;
      
      public static const ReceiveType_ImageStrNpcStart:int = ReceiveType_SkillNameStrEnd + 1;
      
      public static const ReceiveType_ImageStrNpcEnd:int = ReceiveType_ImageStrNpcStart + PlayerNumFri - 1;
      
      public static const ReceiveType_CharaSelectedStart:int = ReceiveType_ImageStrNpcEnd + 1;
      
      public static const ReceiveType_CharaSelectedEnd:int = ReceiveType_CharaSelectedStart + CharacterMax - 1;
      
      public static const ReceiveType_CharaVariationStart:int = ReceiveType_CharaSelectedEnd + 1;
      
      public static const ReceiveType_CharaVariationEnd:int = ReceiveType_CharaVariationStart + CharaVarIndexNum - 1;
      
      public static const ReceiveType_DLCUnlockFlag:int = ReceiveType_CharaVariationEnd + 1;
      
      public static const ReceiveType_DLCUnlockFlag2:int = ReceiveType_DLCUnlockFlag + 1;
      
      public static const ReceiveType_CharaSingleUnlock:int = ReceiveType_DLCUnlockFlag2 + 1;
      
      public static const ReceiveType_CharaSingleUnlock2:int = ReceiveType_CharaSingleUnlock + 1;
      
      public static const ReceiveType_JoyConSingleFlag:int = ReceiveType_CharaSingleUnlock2 + 1;
      
      public static const ReceiveType_WaitLoadNum:int = ReceiveType_JoyConSingleFlag;
      
      public static const ReceiveType_CostumeNum:int = ReceiveType_JoyConSingleFlag + 1;
      
      public static const ReceiveType_CharacterTableStart:int = ReceiveType_CostumeNum + 1;
      
      public static const ReceiveType_CostumeID:int = ReceiveType_CharacterTableStart;
      
      public static const ReceiveType_CID:int = ReceiveType_CostumeID + 1;
      
      public static const ReceiveType_MID:int = ReceiveType_CID + 1;
      
      public static const ReceiveType_PID:int = ReceiveType_MID + 1;
      
      public static const ReceiveType_UnlockNum:int = ReceiveType_PID + 1;
      
      public static const ReceiveType_Gokuaku:int = ReceiveType_UnlockNum + 1;
      
      public static const ReceiveType_SelectVoice1:int = ReceiveType_Gokuaku + 1;
      
      public static const ReceiveType_SelectVoice2:int = ReceiveType_SelectVoice1 + 1;
      
      public static const ReceiveType_DlcKey:int = ReceiveType_SelectVoice2 + 1;
      
      public static const ReceiveType_DlcKey2:int = ReceiveType_DlcKey + 1;
      
      public static const ReceiveType_CustomCostume:int = ReceiveType_DlcKey2 + 1;
      
      public static const ReceiveType_AvatarSlotID:int = ReceiveType_CustomCostume + 1;
      
      public static const ReceiveType_AfterTU9Order:int = ReceiveType_AvatarSlotID + 1;
      
      public static const ReceiveType_CustomCostumeEx:int = ReceiveType_AfterTU9Order + 1;
      
      public static const ReceiveType_ChouGokuaku:int = ReceiveType_CustomCostumeEx + 1;
      
      public static const ReceiveType_CharaSingleUnlockKey:int = ReceiveType_ChouGokuaku + 1;
      
      public static const ReceiveType_CharaSingleUnlockKey2:int = ReceiveType_CharaSingleUnlockKey + 1;
      
      public static const ReceiveType_Kokuhuku:int = ReceiveType_CharaSingleUnlockKey2 + 1;
      
      public static const ReceiveType_CharacterTableEnd:int = ReceiveType_CharacterTableStart + CharacterTableMax * CharacterTableData;
      
      public static const ReceiveType_UseCustomList:int = ReceiveType_CharacterTableEnd + 1;
      
      public static const ReceiveType_CustomListNum:int = ReceiveType_UseCustomList + 1;
      
      public static const ReceiveType_CustomList_CID_Start:int = ReceiveType_CustomListNum + 1;
      
      public static const ReceiveType_CustomList_CID_End:int = ReceiveType_CustomList_CID_Start + CustomListMax - 1;
      
      public static const ReceiveType_CustomList_MID_Start:int = ReceiveType_CustomList_CID_End + 1;
      
      public static const ReceiveType_CustomList_MID_End:int = ReceiveType_CustomList_MID_Start + CustomListMax - 1;
      
      public static const ReceiveType_CustomList_PID_Start:int = ReceiveType_CustomList_MID_End + 1;
      
      public static const ReceiveType_CustomList_PID_End:int = ReceiveType_CustomList_PID_Start + CustomListMax - 1;
      
      public static const ReceiveType_CustomList_PartnerJudge_Start:int = ReceiveType_CustomList_PID_End + 1;
      
      public static const ReceiveType_CustomList_PartnerJudge_End:int = ReceiveType_CustomList_PartnerJudge_Start + CustomListMax - 1;
      
      public static const ReceiveType_Num:int = ReceiveType_CustomList_PartnerJudge_End + 1;
      
      private static const SendType_SelectCode:int = 0;
      
      private static const SendType_SelectVariation:int = 1;
      
      private static const SendType_SelectCustomFlag:int = 2;
      
      private static const SendType_SelectMid:int = 3;
      
      private static const SendType_SelectModelPreset:int = 4;
      
      private static const SendType_CurrentPlayerIndex:int = 5;
      
      private static const SendType_RequestCharaInfo:int = 6;
      
      private static const SendType_RequestImageStr:int = 7;
      
      private static const SendType_RequestNewInfo:int = 8;
      
      private static const SendType_RequestUnlock:int = 9;
      
      private static const SendType_RequestDecide:int = 10;
      
      private static const SendType_RequestSetFlagSkill:int = 11;
      
      private static const SendType_SelectUnlockIndex:int = 12;
      
      private static const SendType_RequestPlayVoice:int = 13;
      
      private static const SendType_SelectListIndex:int = 14;
      
      private static const SendType_RequestCountDownSe:int = 15;
      
      private static const SendType_EnableOptionBGM:int = 16;
      
      private static const SendType_AvatarSlotID:int = 17;
      
      private static const IndexNumRow:int = 3;
      
      private static const IndexNumColumn:int = 7;
      
      private static const IndexNumCharaNewIcon:int = 26;
      
      private static const IndexNumCharaVariationNewIcon:int = 20;
      
      private static const PlayerIndexOwn:int = 0;
      
      private static const PlayerIndexFriStart:int = PlayerIndexOwn + 1;
      
      private static const PlayerIndexFriEnd:int = PlayerIndexFriStart + PlayerNumFri - 1;
      
      private static const PlayerIndexEnmStart:int = PlayerIndexFriEnd + 1;
      
      private static const PlayerIndexEnmEnd:int = PlayerIndexEnmStart + PlayerNumEnm - 1;
      
      private static const PlayerTeamTypeOwn:int = 0;
      
      private static const PlayerTeamTypeFri:int = 1;
      
      private static const PlayerTeamTypeEnm:int = 2;
      
      private static const PlayerTeamTypeInvalid:int = -1;
      
      private static var SelectInfoTypeListIndex:int = 0;
      
      private static var SelectInfoTypeVarIndex:int = 1;
      
      private static var SelectInfoTypeNum:int = 2;
      
      public static var CharaListShiftCount:Array = new Array();
      
      private var m_callback:Callback = null;
      
      private var m_timeline:MovieClip = null;
      
      private var m_timer:CountDownTimer = null;
      
      private var m_current_player_index:int;
      
      private var m_current_select_index:int;
      
      private var m_select_info:Array;
      
      private var m_select_row:int;
      
      private var m_select_column:int;
      
      private var m_select_column_start:int;
      
      private var m_select_icon_row:int;
      
      private var m_select_icon_column:int;
      
      private var m_select_var:int;
      
      private var m_flag_skill:Boolean;
      
      private var m_chara_face:Array;
      
      private var m_chara_face_npc:Array;
      
      private var m_flag_unlock:Array;
      
      private var m_flag_change_disp:Boolean;
      
      private var m_count_change_disp:int;
      
      private var m_flag_change_player:Boolean;
      
      private var m_flag_change_page:Boolean;
      
      private var m_flag_decide:Boolean;
      
      private var m_flag_exit:Boolean;
      
      private var m_skill_str_width_default:Number;
      
      private var m_skill_str_scalex_default:Number;

      // BattleStyles: the added Battle Style row under skill09, made once.
      private var m_style_row_made:Boolean = false;

      private var m_style_row:MovieClip = null;
      
      private var m_icon_show_wait:Array;
      
      private var m_chara_list:Array;
      
      private var m_chara_list_num:int = 0;
      
      private var m_chara_num_column:int = 0;
      
      private var m_pre_time:int = 99;
      
      private var m_selected_index_list:Array;
      
      private var m_flag_customlist:* = false;
      
      private var isKeyboardControllerActive:Boolean;
      
      public function CharaSele()
      {
         var i:int;
         var j:int;
         super();
         trace("[CHARASELE] CharaSele()");
         this.m_callback = new Callback(ReceiveType_Num);
         this.m_timeline = null;
         this.m_timer = new CountDownTimer();
         this.m_current_player_index = 0;
         this.m_current_select_index = 0;
         this.m_select_info = new Array(PlayerMax);
         this.m_selected_index_list = new Array(PlayerMax);
         i = 0;
         j = 0;
         i = 0;
         while(this.m_select_info.length > i)
         {
            this.m_select_info[i] = new Array(SelectInfoTypeNum);
            this.m_select_info[i][SelectInfoTypeListIndex] = 0;
            this.m_select_info[i][SelectInfoTypeVarIndex] = 0;
            i++;
         }
         i = 0;
         while(this.m_selected_index_list.length > i)
         {
            this.m_selected_index_list[i] = -1;
            i++;
         }
         this.m_select_row = 0;
         this.m_select_column = 0;
         this.m_select_column_start = 0;
         this.m_select_icon_row = -1;
         this.m_select_icon_column = -1;
         this.m_select_var = 0;
         this.m_flag_skill = false;
         this.m_flag_change_disp = true;
         this.m_count_change_disp = 0;
         this.m_flag_change_player = false;
         this.m_flag_change_page = false;
         this.m_flag_decide = false;
         this.m_flag_exit = false;
         this.m_skill_str_width_default = 0;
         this.m_skill_str_scalex_default = 1;
         this.m_icon_show_wait = new Array(IndexNumCharaVariationNewIcon);
         i = 0;
         while(i < this.m_icon_show_wait.length)
         {
            this.m_icon_show_wait[i] = 0;
            i++;
         }
         this.m_chara_list = null;
         this.m_chara_list_num = 0;
         this.m_chara_num_column = 0;
         i = 0;
         while(i < CharacterMax)
         {
            this.m_callback.AddCallbackSetUserDataInt(ReceiveType_CharaSelectedStart + i,0);
            i++;
         }
         i = 0;
         while(i < CharaVarIndexNum)
         {
            this.m_callback.AddCallbackSetUserDataInt(ReceiveType_CharaVariationStart + i,0);
            i++;
         }
         try
         {
            ExternalInterface.addCallback("ForcingCancel",this.pushKeyCancel);
            ExternalInterface.addCallback("RefreshIcons",this.ResetIcons);
            return;
         }
         catch(e:Error)
         {
            trace("[CHARASELE] ForcingCancel EXCEPTION");
            return;
         }
      }
      
      public function Initialize(param1:MovieClip) : void
      {
         this.m_timeline = param1;
         this.m_timeline.visible = false;
         this.m_timeline.cha_frame.visible = false;
         this.m_timeline.cha_skill.visible = false;
         this.m_timeline.cha_select.visible = false;
         this.m_timeline.cha_parameter.visible = false;
         this.m_timeline.cha_arrow.visible = false;
         this.m_timeline.cha_select_cur.visible = false;
         this.m_timeline.press2P.visible = false;
         this.m_timer.Initialize(this.m_timeline.timer.nest._CMN_M_B_mc_timer,null);
         this.m_current_player_index = 0;
         this.m_select_row = 0;
         this.m_select_column = 0;
         this.m_select_column_start = 0;
         this.m_select_var = 0;
         this.m_select_icon_row = -1;
         this.m_select_icon_column = -1;
         this.m_flag_skill = false;
         this.m_flag_change_disp = true;
         this.m_count_change_disp = 0;
         this.m_flag_change_player = false;
         this.m_flag_change_page = false;
         this.m_flag_decide = false;
         this.m_skill_str_width_default = this.m_timeline.cha_skill.inact_skill.skill01.sys_skill.width;
         this.m_skill_str_scalex_default = this.m_timeline.cha_skill.inact_skill.skill01.sys_skill.scaleX;
         this.m_pre_time = 99;
         this.m_chara_list = null;
         this.m_chara_list_num = 0;
         this.m_chara_num_column = 0;
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitCharaInfo);
      }
      
      private function waitCharaInfo(param1:Event) : void
      {
         var _loc2_:* = 0;
         var _loc3_:* = 0;
         var _loc4_:int = 0;
         var _loc5_:* = undefined;
         var _loc6_:int = 0;
         var _loc7_:int = 0;
         var _loc8_:int = 0;
         var _loc9_:int = 0;
         var _loc10_:int = 0;
         var _loc11_:int = 0;
         var _loc12_:Array = null;
         var _loc13_:int = 0;
         var _loc14_:String = null;
         var _loc15_:int = 0;
         var _loc16_:int = 0;
         var _loc17_:int = 0;
         var _loc18_:Array = null;
         var _loc19_:Boolean = false;
         var _loc20_:Boolean = false;
         var _loc21_:int = 0;
         var _loc22_:int = 0;
         var _loc23_:int = 0;
         var _loc24_:int = 0;
         var _loc25_:int = 0;
         var _loc26_:int = 0;
         if(!this.m_callback)
         {
            return;
         }
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_DLCUnlockFlag))
         {
            return;
         }
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_DLCUnlockFlag2))
         {
            return;
         }
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_UseCustomList))
         {
            return;
         }
         var _loc27_:int = this.m_callback.GetUserDataInt(ReceiveType_UseCustomList);
         if(_loc27_)
         {
            this.m_flag_customlist = true;
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_CustomListNum))
            {
               return;
            }
            _loc5_ = this.m_callback.GetUserDataInt(ReceiveType_CustomListNum);
            _loc2_ = 0;
            while(_loc2_ < _loc5_)
            {
               if(!this.m_callback.GetUserDataValidFlag(ReceiveType_CustomList_CID_Start + _loc2_))
               {
                  return;
               }
               if(!this.m_callback.GetUserDataValidFlag(ReceiveType_CustomList_MID_Start + _loc2_))
               {
                  return;
               }
               if(!this.m_callback.GetUserDataValidFlag(ReceiveType_CustomList_PID_Start + _loc2_))
               {
                  return;
               }
               if(!this.m_callback.GetUserDataValidFlag(ReceiveType_CustomList_PartnerJudge_Start + _loc2_))
               {
                  return;
               }
               _loc2_++;
            }
            _loc6_ = this.m_callback.GetUserDataInt(ReceiveType_DLCUnlockFlag);
            _loc7_ = this.m_callback.GetUserDataInt(ReceiveType_DLCUnlockFlag2);
            _loc8_ = this.m_callback.GetUserDataInt(ReceiveType_CharaSingleUnlock);
            _loc9_ = this.m_callback.GetUserDataInt(ReceiveType_CharaSingleUnlock2);
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_CostumeNum))
            {
               return;
            }
            _loc10_ = this.m_callback.GetUserDataInt(ReceiveType_CostumeNum);
            _loc11_ = ReceiveType_CharacterTableStart;
            while(_loc11_ < ReceiveType_CharacterTableStart + _loc10_ * CharacterTableData)
            {
               if(!this.m_callback.GetUserDataValidFlag(_loc11_))
               {
                  return;
               }
               _loc11_++;
            }
            this.m_chara_list = this.recieveList(_loc6_,_loc7_,_loc8_,_loc9_);
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_CharacterTableEnd + 1))
            {
               return;
            }
            _loc12_ = new Array();
            _loc2_ = 0;
            while(_loc2_ < _loc5_)
            {
               _loc14_ = this.m_callback.GetUserDataString(ReceiveType_CustomList_CID_Start + _loc2_);
               _loc15_ = this.m_callback.GetUserDataInt(ReceiveType_CustomList_MID_Start + _loc2_);
               _loc16_ = this.m_callback.GetUserDataInt(ReceiveType_CustomList_PID_Start + _loc2_);
               _loc17_ = this.m_callback.GetUserDataInt(ReceiveType_CustomList_PartnerJudge_Start + _loc2_);
               _loc12_.push([_loc14_,_loc15_,_loc16_,_loc17_]);
               _loc2_++;
            }
            _loc2_ = 0;
            while(_loc2_ < this.m_chara_list.length)
            {
               _loc18_ = this.m_chara_list[_loc2_];
               _loc19_ = true;
               _loc3_ = 0;
               while(_loc3_ < _loc18_.length)
               {
                  _loc20_ = true;
                  _loc4_ = 0;
                  while(_loc4_ < _loc12_.length)
                  {
                     if(_loc18_[_loc3_][0] == _loc12_[_loc4_][0])
                     {
                        if(_loc12_[_loc4_][1] == -1 || _loc18_[_loc3_][1] == _loc12_[_loc4_][1])
                        {
                           if(_loc12_[_loc4_][2] == -1 || _loc18_[_loc3_][2] == _loc12_[_loc4_][2])
                           {
                              if(!(_loc12_[_loc4_][3] != 1 && _loc18_[_loc3_][VarTypeCustomCostume]))
                              {
                                 _loc20_ = false;
                                 _loc19_ = false;
                              }
                           }
                        }
                     }
                     _loc4_++;
                  }
                  if(_loc20_)
                  {
                     _loc18_.splice(_loc3_,1);
                     _loc3_--;
                  }
                  _loc3_++;
               }
               if(_loc19_)
               {
                  this.m_chara_list.splice(_loc2_,1);
                  _loc2_--;
               }
               _loc2_++;
            }
            _loc13_ = 3 - this.m_chara_list.length % 3;
            _loc2_ = 0;
            while(_loc13_ > _loc2_)
            {
               this.m_chara_list.concat([[["",0,0,0]]]);
               _loc2_++;
            }
         }
         else
         {
            _loc21_ = this.m_callback.GetUserDataInt(ReceiveType_DLCUnlockFlag);
            _loc22_ = this.m_callback.GetUserDataInt(ReceiveType_DLCUnlockFlag2);
            _loc23_ = this.m_callback.GetUserDataInt(ReceiveType_CharaSingleUnlock);
            _loc24_ = this.m_callback.GetUserDataInt(ReceiveType_CharaSingleUnlock2);
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_CostumeNum))
            {
               return;
            }
            _loc25_ = this.m_callback.GetUserDataInt(ReceiveType_CostumeNum);
            _loc26_ = ReceiveType_CharacterTableStart;
            while(_loc26_ < ReceiveType_CharacterTableStart + _loc25_ * CharacterTableData)
            {
               if(!this.m_callback.GetUserDataValidFlag(_loc26_))
               {
                  return;
               }
               _loc26_++;
            }
            this.m_chara_list = this.recieveList(_loc21_,_loc22_,_loc23_,_loc24_);
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_CharacterTableEnd + 1))
            {
               return;
            }
         }
         this.m_chara_list_num = this.m_chara_list.length;
         this.m_chara_num_column = (this.m_chara_list_num - 1) / IndexNumRow + 1;
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitCharaInfo);
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.requestUnlock);
      }
      
      private function recieveList(param1:int, param2:int, param3:int, param4:int) : Array
      {
         var _loc39_:Array = null;
         var _loc44_:Array = null;
         var _loc45_:String = null;
         var _loc46_:int = 0;
         var _loc47_:String = null;
         var _loc48_:Array = null;
         var _loc49_:Array = null;
         var _loc50_:Array = null;
         var _loc5_:* = 0;
         var _loc6_:int = 0;
         var _loc7_:Array = null;
         var _loc8_:Array = null;
         var _loc9_:String = null;
         var _loc10_:int = 0;
         var _loc11_:int = 0;
         var _loc12_:int = 0;
         var _loc13_:int = 0;
         var _loc14_:Array = null;
         var _loc15_:int = 0;
         var _loc16_:int = 0;
         var _loc17_:int = 0;
         var _loc18_:int = 0;
         var _loc19_:int = 0;
         var _loc20_:int = 0;
         var _loc21_:int = 0;
         var _loc22_:int = 0;
         var _loc23_:int = 0;
         var _loc24_:int = 0;
         var _loc25_:int = 0;
         var _loc26_:Array = null;
         var _loc27_:Boolean = false;
         var _loc28_:* = 0;
         var _loc29_:int = 0;
         var _loc30_:int = 0;
         var _loc31_:Array = new Array();
         var _loc32_:int = this.m_callback.GetUserDataInt(ReceiveType_CostumeNum);
         var _loc33_:Array = new Array();
         var _loc34_:int = -1;
         var _loc35_:Array = new Array();
         var _loc36_:Array = new Array();
         _loc5_ = 0;
         while(_loc5_ < _loc32_)
         {
            _loc7_ = new Array();
            while(true)
            {
               _loc8_ = new Array();
               _loc9_ = this.m_callback.GetUserDataString(ReceiveType_CID + _loc5_ * CharacterTableData);
               _loc10_ = this.m_callback.GetUserDataInt(ReceiveType_MID + _loc5_ * CharacterTableData);
               _loc11_ = this.m_callback.GetUserDataInt(ReceiveType_PID + _loc5_ * CharacterTableData);
               _loc12_ = this.m_callback.GetUserDataInt(ReceiveType_UnlockNum + _loc5_ * CharacterTableData);
               _loc13_ = this.m_callback.GetUserDataInt(ReceiveType_Gokuaku + _loc5_ * CharacterTableData);
               _loc14_ = new Array();
               _loc14_.push(this.m_callback.GetUserDataInt(ReceiveType_SelectVoice1 + _loc5_ * CharacterTableData));
               _loc14_.push(this.m_callback.GetUserDataInt(ReceiveType_SelectVoice2 + _loc5_ * CharacterTableData));
               _loc15_ = this.m_callback.GetUserDataInt(ReceiveType_DlcKey + _loc5_ * CharacterTableData);
               _loc16_ = this.m_callback.GetUserDataInt(ReceiveType_DlcKey2 + _loc5_ * CharacterTableData);
               _loc17_ = this.m_callback.GetUserDataInt(ReceiveType_CustomCostume + _loc5_ * CharacterTableData);
               _loc18_ = this.m_callback.GetUserDataInt(ReceiveType_AvatarSlotID + _loc5_ * CharacterTableData);
               _loc19_ = this.m_callback.GetUserDataInt(ReceiveType_AfterTU9Order + _loc5_ * CharacterTableData);
               _loc20_ = this.m_callback.GetUserDataInt(ReceiveType_CustomCostumeEx + _loc5_ * CharacterTableData);
               _loc21_ = this.m_callback.GetUserDataInt(ReceiveType_ChouGokuaku + _loc5_ * CharacterTableData);
               _loc22_ = this.m_callback.GetUserDataInt(ReceiveType_CharaSingleUnlockKey + _loc5_ * CharacterTableData);
               _loc23_ = this.m_callback.GetUserDataInt(ReceiveType_CharaSingleUnlockKey2 + _loc5_ * CharacterTableData);
               _loc24_ = this.m_callback.GetUserDataInt(ReceiveType_Kokuhuku + _loc5_ * CharacterTableData);
               _loc8_.push(_loc9_);
               _loc8_.push(_loc10_);
               _loc8_.push(_loc11_);
               _loc8_.push(_loc12_);
               _loc8_.push(_loc13_);
               _loc8_.push(_loc14_);
               _loc8_.push(_loc15_);
               _loc8_.push(_loc16_);
               _loc8_.push(_loc17_);
               _loc8_.push(_loc18_);
               _loc8_.push(_loc19_);
               _loc8_.push(_loc20_);
               _loc8_.push(_loc21_);
               _loc8_.push(_loc22_);
               _loc8_.push(_loc23_);
               _loc8_.push(_loc24_);
               _loc7_.push(_loc8_);
               if(_loc34_ < 0)
               {
                  _loc34_ = _loc18_;
               }
               if(_loc17_)
               {
                  _loc44_ = _loc7_[0];
                  _loc45_ = _loc44_[0] + _loc44_[1].toString();
                  _loc33_[_loc45_] = _loc8_;
               }
               else if(_loc15_ < 0 && _loc9_ == "GOK")
               {
                  _loc35_.push(_loc8_);
               }
               else if(_loc18_ != _loc34_ && _loc9_ == "HUM")
               {
                  _loc36_.push(_loc8_);
               }
               _loc25_ = this.m_callback.GetUserDataInt(ReceiveType_CostumeID + (_loc5_ + 1) * CharacterTableData);
               if(_loc25_ == 0)
               {
                  break;
               }
               _loc5_++;
            }
            _loc31_.push(_loc7_);
            _loc5_++;
         }
         var _loc37_:int = 0;
         var _loc38_:String = XV2Patcher.GetSlotsData();
         _loc31_ = new Array();
         while(_loc37_ < _loc38_.length)
         {
            _loc7_ = new Array();
            if(_loc38_.substr(_loc37_,1) != "{")
            {
               trace("Format error 1\n");
               return null;
            }
            _loc37_++;
            while(_loc38_.substr(_loc37_,1) == "[")
            {
               _loc37_++;
               _loc8_ = new Array();
               _loc46_ = _loc38_.indexOf("]",_loc37_);
               if(_loc46_ == -1)
               {
                  trace("Format error 2\n");
                  return null;
               }
               _loc47_ = _loc38_.substring(_loc37_,_loc46_);
               _loc48_ = _loc47_.split(",");
               if(_loc48_.length != 11)
               {
                  trace("Invalid number of elements: " + _loc48_.length);
                  return null;
               }
               _loc8_.push(_loc48_[0]);
               _loc8_.push(int(_loc48_[1]));
               _loc8_.push(int(_loc48_[2]));
               _loc8_.push(int(_loc48_[3]));
               _loc8_.push(int(_loc48_[4]));
               _loc8_.push(new Array(int(_loc48_[5]),int(_loc48_[6])));
               _loc8_.push(int(_loc48_[7]));
               _loc8_.push(int(_loc48_[8]));
               _loc8_.push(0);
               _loc8_.push(_loc34_);
               _loc8_.push(-1);
               _loc8_.push(0);
               _loc8_.push(int(_loc48_[9]));
               _loc8_.push(0);
               _loc8_.push(0);
               _loc8_.push(int(_loc48_[10]));
               _loc37_ = _loc46_ + 1;
               _loc7_.push(_loc8_);
            }
            if(_loc7_.length > 0)
            {
               _loc44_ = _loc7_[0];
               _loc45_ = _loc44_[0] + _loc44_[1].toString();
               _loc49_ = _loc33_[_loc45_];
               if(_loc49_ != undefined)
               {
                  _loc7_.push(_loc49_);
               }
            }
            if(_loc38_.substr(_loc37_,1) != "}")
            {
               trace("Format error 3\n");
               return null;
            }
            _loc37_++;
            _loc31_.push(_loc7_);
         }
         _loc37_ = 0;
         while(_loc37_ < _loc35_.length)
         {
            _loc50_ = new Array();
            _loc50_.push(_loc35_[_loc37_]);
            _loc31_.push(_loc50_);
            _loc37_++;
         }
         var _loc40_:int = -1;
         _loc37_ = 0;
         while(_loc37_ < _loc36_.length)
         {
            if(_loc36_[_loc37_][9] != _loc40_)
            {
               _loc40_ = int(_loc36_[_loc37_][9]);
               if(_loc37_ != 0)
               {
                  _loc31_.push(_loc39_);
               }
               _loc39_ = new Array();
            }
            _loc39_.push(_loc36_[_loc37_]);
            if(_loc37_ == _loc36_.length - 1)
            {
               _loc31_.push(_loc39_);
            }
            _loc37_++;
         }
         var _loc41_:int = 0;
         var _loc42_:* = 0;
         _loc5_ = 0;
         while(_loc5_ < _loc31_.length)
         {
            _loc26_ = _loc31_[_loc5_];
            _loc27_ = true;
            _loc28_ = 0;
            while(_loc28_ < _loc26_.length)
            {
               if((param1 & _loc26_[_loc28_][VarTypeDlcKeyFlag]) == 0 && (param2 & _loc26_[_loc28_][VarTypeDlcKeyFlag2]) == 0 && (param3 & _loc26_[_loc28_][VarTypeCharaSingleUnlockFlag]) == 0 && (param4 & _loc26_[_loc28_][VarTypeCharaSingleUnlockFlag2]) == 0)
               {
                  trace("delete chara:" + _loc5_ + "," + _loc28_ + "," + _loc26_[_loc28_][VarTypeCode]);
                  _loc26_.splice(_loc28_,1);
                  _loc28_--;
               }
               else
               {
                  _loc27_ = false;
               }
               _loc28_++;
            }
            if(_loc27_)
            {
               _loc31_.splice(_loc5_,1);
               _loc5_--;
            }
            else
            {
               _loc29_ = _loc42_;
               _loc30_ = int(_loc26_[0][VarTypeAfterTU9Order]);
               if(_loc30_ == -1)
               {
                  _loc29_ = _loc42_;
               }
               else
               {
                  _loc29_ = _loc30_ + 122;
                  _loc42_--;
               }
               CharaListShiftCount.push(_loc29_);
            }
            _loc5_++;
            _loc42_++;
         }
         var _loc43_:int = 3 - _loc31_.length % 3;
         _loc5_ = 0;
         while(_loc43_ > _loc5_)
         {
            _loc31_.concat([[[InvalidCode,0,0,0]]]);
            _loc5_++;
         }
         return _loc31_;
      }
      
      private function requestUnlock(param1:Event) : void
      {
         var _loc2_:Boolean = false;
         var _loc3_:Array = null;
         var _loc4_:Boolean = false;
         var _loc5_:int = 0;
         var _loc6_:Array = null;
         var _loc7_:String = null;
         var _loc8_:int = 0;
         var _loc9_:int = 0;
         var _loc10_:int = 0;
         var _loc11_:int = 0;
         var _loc12_:int = 0;
         var _loc13_:int = 0;
         var _loc14_:* = 0;
         _loc13_ = 0;
         while(CharacterMax > _loc13_)
         {
            _loc2_ = false;
            _loc3_ = this.getCharaInfo(_loc13_);
            if(!_loc3_)
            {
               _loc14_ = 0;
               while(CharaVarIndexNum > _loc14_)
               {
                  this.SetUserDataInt(ReceiveType_UnlockVarStart + _loc13_ * CharaVarIndexNum + _loc14_,0);
                  _loc14_++;
               }
               this.SetUserDataString(ReceiveType_ImageStrStart + _loc13_,"");
            }
            else if(_loc3_.length <= _loc14_)
            {
               _loc14_ = 0;
               while(CharaVarIndexNum > _loc14_)
               {
                  this.SetUserDataInt(ReceiveType_UnlockVarStart + _loc13_ * CharaVarIndexNum + _loc14_,0);
                  _loc14_++;
               }
               this.SetUserDataString(ReceiveType_ImageStrStart + _loc13_,"");
            }
            else
            {
               _loc4_ = false;
               _loc14_ = int(CharaVarIndexNum - 1);
               while(0 <= _loc14_)
               {
                  _loc5_ = _loc13_ * CharaVarIndexNum + _loc14_;
                  if(_loc3_.length <= _loc14_)
                  {
                     this.SetUserDataInt(ReceiveType_UnlockVarStart + _loc5_,0);
                  }
                  else
                  {
                     _loc6_ = _loc3_[_loc14_];
                     _loc7_ = _loc6_[VarTypeCode];
                     if(_loc7_ == InvalidCode)
                     {
                        this.SetUserDataInt(ReceiveType_UnlockVarStart + _loc5_,0);
                     }
                     else
                     {
                        _loc8_ = int(_loc6_[VarTypeMid]);
                        _loc2_ = true;
                        if(!this.m_callback.GetUserDataValidFlag(ReceiveType_UnlockVarStart + _loc5_))
                        {
                           _loc9_ = int(_loc6_[VarTypeUnlockIndex]);
                           _loc10_ = int(_loc6_[VarTypeAvatarSlotID]);
                           _loc11_ = int(_loc6_[VarTypeCustomCostume]);
                           _loc12_ = _loc14_;
                           this.m_callback.CallbackUserData("user",SendType_AvatarSlotID,_loc10_);
                           this.m_callback.CallbackUserDataString("user",SendType_SelectCode,_loc7_);
                           this.m_callback.CallbackUserData("user",SendType_SelectUnlockIndex,_loc9_);
                           this.m_callback.CallbackUserData("user",SendType_SelectVariation,_loc12_);
                           this.m_callback.CallbackUserData("user",SendType_SelectCustomFlag,_loc11_);
                           this.m_callback.CallbackUserData("user",SendType_SelectMid,_loc8_);
                           this.m_callback.CallbackUserData("user",SendType_RequestUnlock,_loc5_);
                           this.m_callback.CallbackUserData("user",SendType_RequestImageStr,_loc13_);
                           this.m_callback.CallbackUserData("user",SendType_RequestNewInfo,CharaListShiftCount[_loc13_]);
                        }
                     }
                  }
                  _loc14_--;
               }
               if(!_loc2_)
               {
                  this.SetUserDataString(ReceiveType_ImageStrStart + _loc13_,"");
               }
            }
            _loc13_++;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.requestUnlock);
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitUnlock);
      }
      
      private function waitUnlock(param1:Event) : void
      {
         var _loc2_:int = 0;
         var _loc3_:int = 0;
         var _loc4_:int = 0;
         var _loc5_:int = 0;
         var _loc6_:int = 0;
         while(_loc6_ < this.m_select_info.length)
         {
            _loc2_ = -1;
            _loc5_ = 0;
            while(_loc5_ < CharacterMax)
            {
               _loc3_ = PlayerIndexOwn + _loc5_;
               if(!this.m_callback.GetUserDataValidFlag(ReceiveType_FlagSelectAvatar))
               {
                  return;
               }
               _loc4_ = this.m_callback.GetUserDataInt(ReceiveType_FlagSelectAvatar);
               if(this.m_flag_customlist)
               {
                  _loc4_ = 1;
                  _loc2_ = 0;
               }
               if(Boolean(_loc6_ == PlayerIndexOwn) || Boolean(_loc4_) || !this.checkAvatar(_loc3_))
               {
                  if(this.checkUnlockChara(_loc3_))
                  {
                     _loc2_ = _loc3_;
                     break;
                  }
               }
               _loc5_++;
            }
            if(_loc2_ < 0)
            {
               return;
            }
            this.m_select_row = this.calcIconIndexRow(_loc2_);
            this.m_select_column = this.calcIconIndexColumn(_loc2_);
            this.m_current_player_index = _loc6_;
            this.m_select_var = 0;
            this.setSelectChara();
            _loc6_++;
         }
         this.m_current_player_index = PlayerIndexOwn;
         var _loc7_:* = this.m_select_info[this.m_current_player_index][SelectInfoTypeListIndex];
         this.m_select_row = this.calcIconIndexRow(_loc7_);
         this.m_select_column = this.calcIconIndexColumn(_loc7_);
         this.m_select_var = this.m_select_info[this.m_current_player_index][SelectInfoTypeVarIndex];
         this.sendCharaInfo(this.m_current_player_index);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitUnlock);
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitStart);
      }
      
      private function waitStart(param1:Event) : void
      {
         var _loc2_:int = 0;
         _loc2_ = 0;
         while(ReceiveType_WaitLoadNum > _loc2_)
         {
            if(!this.m_callback.GetUserDataValidFlag(_loc2_))
            {
               return;
            }
            _loc2_++;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStart);
         this.startMain();
      }
      
      private function ResetIcons() : void
      {
         var _loc1_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrL2);
         var _loc2_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrR2);
         var _loc3_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrRU);
         var _loc4_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrRD);
         var _loc5_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrRL);
         var _loc6_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrRR);
         this.isKeyboardControllerActive = _loc1_.indexOf("src=\"kb") > 0;
         this.m_timeline.cha_skill.inact_skill.skill01.sys_com01.htmlText = _loc2_;
         this.m_timeline.cha_skill.inact_skill.skill01.sys_com02.htmlText = _loc3_;
         this.m_timeline.cha_skill.inact_skill.skill01.sys_com03.htmlText = "";
         this.m_timeline.cha_skill.inact_skill.skill01.sys_01.visible = false;
         this.m_timeline.cha_skill.inact_skill.skill02.sys_com01.htmlText = _loc2_;
         this.m_timeline.cha_skill.inact_skill.skill02.sys_com02.htmlText = _loc5_;
         this.m_timeline.cha_skill.inact_skill.skill02.sys_com03.htmlText = "";
         this.m_timeline.cha_skill.inact_skill.skill02.sys_01.visible = false;
         this.m_timeline.cha_skill.inact_skill.skill03.sys_com01.htmlText = _loc2_;
         this.m_timeline.cha_skill.inact_skill.skill03.sys_com02.htmlText = _loc6_;
         this.m_timeline.cha_skill.inact_skill.skill03.sys_com03.htmlText = "";
         this.m_timeline.cha_skill.inact_skill.skill03.sys_01.visible = false;
         this.m_timeline.cha_skill.inact_skill.skill04.sys_com01.htmlText = _loc2_;
         this.m_timeline.cha_skill.inact_skill.skill04.sys_com02.htmlText = _loc4_;
         this.m_timeline.cha_skill.inact_skill.skill04.sys_com03.htmlText = "";
         this.m_timeline.cha_skill.inact_skill.skill04.sys_01.visible = false;
         if(this.isKeyboardControllerActive)
         {
            this.m_timeline.cha_skill.inact_skill.skill05.sys_com01.htmlText = _loc1_;
            this.m_timeline.cha_skill.inact_skill.skill05.sys_com02.htmlText = _loc3_;
            this.m_timeline.cha_skill.inact_skill.skill05.sys_com03.htmlText = "";
            this.m_timeline.cha_skill.inact_skill.skill05.sys_01.visible = false;
            this.m_timeline.cha_skill.inact_skill.skill06.sys_com01.htmlText = _loc1_;
            this.m_timeline.cha_skill.inact_skill.skill06.sys_com02.htmlText = _loc5_;
            this.m_timeline.cha_skill.inact_skill.skill06.sys_com03.htmlText = "";
            this.m_timeline.cha_skill.inact_skill.skill06.sys_01.visible = false;
            this.m_timeline.cha_skill.inact_skill.skill07.sys_com01.htmlText = _loc1_;
            this.m_timeline.cha_skill.inact_skill.skill07.sys_com02.htmlText = _loc4_;
            this.m_timeline.cha_skill.inact_skill.skill07.sys_com03.htmlText = "";
            this.m_timeline.cha_skill.inact_skill.skill07.sys_01.visible = false;
            this.m_timeline.cha_skill.inact_skill.skill08.sys_com01.htmlText = _loc1_;
            this.m_timeline.cha_skill.inact_skill.skill08.sys_com02.htmlText = _loc6_;
            this.m_timeline.cha_skill.inact_skill.skill08.sys_com03.htmlText = "";
            this.m_timeline.cha_skill.inact_skill.skill08.sys_01.visible = false;
         }
         else
         {
            this.m_timeline.cha_skill.inact_skill.skill05.sys_com01.htmlText = _loc1_;
            this.m_timeline.cha_skill.inact_skill.skill05.sys_com02.htmlText = _loc2_;
            this.m_timeline.cha_skill.inact_skill.skill05.sys_com03.htmlText = _loc3_;
            this.m_timeline.cha_skill.inact_skill.skill05.sys_01.visible = true;
            this.m_timeline.cha_skill.inact_skill.skill06.sys_com01.htmlText = _loc1_;
            this.m_timeline.cha_skill.inact_skill.skill06.sys_com02.htmlText = _loc2_;
            this.m_timeline.cha_skill.inact_skill.skill06.sys_com03.htmlText = _loc5_;
            this.m_timeline.cha_skill.inact_skill.skill06.sys_01.visible = true;
            this.m_timeline.cha_skill.inact_skill.skill07.sys_com01.htmlText = _loc1_;
            this.m_timeline.cha_skill.inact_skill.skill07.sys_com02.htmlText = _loc2_;
            this.m_timeline.cha_skill.inact_skill.skill07.sys_com03.htmlText = _loc4_;
            this.m_timeline.cha_skill.inact_skill.skill07.sys_01.visible = true;
            this.m_timeline.cha_skill.inact_skill.skill08.sys_com01.htmlText = _loc1_;
            this.m_timeline.cha_skill.inact_skill.skill08.sys_com02.htmlText = _loc2_;
            this.m_timeline.cha_skill.inact_skill.skill08.sys_com03.htmlText = _loc6_;
            this.m_timeline.cha_skill.inact_skill.skill08.sys_01.visible = true;
         }
         var _loc7_:String = this.m_callback.GetUserDataString(ReceiveType_TarismanHeaderStr);
         this.m_timeline.cha_skill.inact_skill.skill09.sys_com01.htmlText = _loc7_;
         this.m_timeline.cha_parameter.nest_clothes.sys_clothes.htmlText = "";
         var _loc8_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrL1);
         this.m_timeline.cha_parameter.nest_clothes.sys_l.htmlText = _loc8_;
         var _loc9_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrR1);
         this.m_timeline.cha_parameter.nest_clothes.sys_r.htmlText = _loc9_;
      }
      
      private function startMain() : void
      {
         var _loc1_:int = 0;
         var _loc2_:MovieClip = null;
         var _loc3_:int = 0;
         var _loc4_:MovieClip = null;
         var _loc5_:MovieClip = null;
         var _loc6_:Array = null;
         var _loc7_:Bitmap = null;
         var _loc8_:String = null;
         var _loc9_:String = null;
         var _loc10_:String = null;
         var _loc11_:Bitmap = null;
         var _loc12_:MovieClip = null;
         var _loc13_:int = 0;
         var _loc14_:MovieClip = null;
         var _loc15_:int = 0;
         this.m_timeline.visible = true;
         this.m_timeline.cha_frame.cmn_CMN_M_frame.visible = true;
         this.m_timeline.cha_skill.visible = false;
         this.m_timeline.cha_select.visible = true;
         this.m_timeline.cha_parameter.visible = true;
         this.m_timeline.cha_arrow.visible = true;
         var _loc16_:* = this.m_timeline.cha_select;
         _loc16_.chara_icn_set00.visible = false;
         var _loc17_:int = (this.m_chara_list_num - 1) / IndexNumRow + 1;
         var _loc18_:int = 1;
         while(_loc18_ < 9)
         {
            if(_loc18_ <= _loc17_)
            {
               _loc16_["chara_icn_set0" + _loc18_].visible = true;
            }
            else
            {
               _loc16_["chara_icn_set0" + _loc18_].visible = false;
            }
            _loc18_++;
         }
         var _loc19_:int = (this.m_chara_list_num - 1) % IndexNumRow + 1;
         _loc17_ = Math.min(_loc17_,8);
         var _loc20_:* = _loc16_["chara_icn_set0" + _loc17_];
         var _loc21_:int = 1;
         while(_loc21_ < 4)
         {
            if(_loc21_ <= _loc19_)
            {
               _loc20_["nest_charaselect0" + _loc21_].visible = true;
            }
            else
            {
               _loc20_["nest_charaselect0" + _loc21_].visible = false;
            }
            _loc21_++;
         }
         var _loc22_:int = this.m_callback.GetUserDataInt(ReceiveType_PlayerFriNum);
         var _loc23_:int = this.m_callback.GetUserDataInt(ReceiveType_PlayerEnmNum);
         if(_loc22_ == 0 && _loc23_ == 0)
         {
            this.m_timeline.cha_parameter.nest_ready.visible = false;
            this.m_timeline.cha_parameter.ready_base.visible = false;
            this.m_timeline.cha_parameter.ready_base.vs.visible = false;
         }
         else
         {
            this.m_timeline.cha_parameter.nest_ready.visible = true;
            this.m_timeline.cha_parameter.ready_base.visible = true;
            if(_loc23_ > 0)
            {
               this.m_timeline.cha_parameter.ready_base.vs.visible = true;
            }
            else
            {
               this.m_timeline.cha_parameter.ready_base.vs.visible = false;
            }
         }
         var _loc24_:int = this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle);
         if(_loc24_)
         {
            this.m_timeline.cha_parameter.icon_1P.play();
            this.m_timeline.cha_parameter.icon_1P.visible = true;
            this.m_timeline.cha_parameter.icon_2P.play();
            this.m_timeline.cha_parameter.icon_2P.visible = false;
         }
         else
         {
            this.m_timeline.cha_parameter.icon_1P.stop();
            this.m_timeline.cha_parameter.icon_1P.visible = false;
            this.m_timeline.cha_parameter.icon_2P.stop();
            this.m_timeline.cha_parameter.icon_2P.visible = false;
            this.m_timeline.press2P.stop();
            this.m_timeline.press2P.visible = false;
         }
         var _loc25_:MovieClip = this.getReadyIconMc(0);
         _loc25_.visible = true;
         _loc15_ = 0;
         while(PlayerNumFri > _loc15_)
         {
            _loc1_ = PlayerIndexFriStart + _loc15_;
            _loc2_ = this.getReadyIconMc(_loc1_);
            if(_loc15_ < _loc22_)
            {
               _loc2_.visible = true;
            }
            else
            {
               _loc2_.visible = false;
            }
            _loc15_++;
         }
         _loc15_ = 0;
         while(PlayerNumEnm > _loc15_)
         {
            _loc3_ = PlayerIndexEnmStart + _loc15_;
            _loc4_ = this.getReadyIconMc(_loc3_);
            if(_loc15_ < _loc23_)
            {
               _loc4_.visible = true;
            }
            else
            {
               _loc4_.visible = false;
            }
            _loc15_++;
         }
         this.m_timeline.cha_parameter.sys_charaName.sys_charaName.htmlText = "";
         if(this.m_timeline.cha_parameter.sys_skill_header)
         {
            this.m_timeline.cha_parameter.sys_skill_header.sys_skill_header.htmlText = "";
         }
         this.setSelectChara();
         var _loc26_:MovieClip = this.m_timeline.cha_new;
         _loc15_ = 0;
         while(IndexNumCharaNewIcon > _loc15_)
         {
            _loc5_ = _loc26_["chara_new_" + _loc15_];
            if(!this.checkCharaNewIconVisible(_loc15_))
            {
               _loc5_.visible = false;
            }
            _loc15_++;
         }
         this.m_chara_face = new Array(this.m_chara_list_num);
         _loc15_ = 0;
         while(this.m_chara_list_num > _loc15_)
         {
            _loc6_ = this.getCharaInfo(_loc15_);
            _loc7_ = null;
            if(Boolean(_loc6_) && _loc6_.length > 0)
            {
               _loc8_ = _loc6_[0][VarTypeCode];
               _loc9_ = this.m_callback.GetUserDataString(ReceiveType_ImageStrStart + _loc15_);
               if(_loc8_ != InvalidCode)
               {
                  _loc7_ = new Bitmap(null);
                  IggyFunctions.setTextureForBitmap(_loc7_,_loc9_);
                  _loc7_.scaleX = 256 / _loc7_.width;
                  _loc7_.scaleY = 128 / _loc7_.height;
               }
            }
            this.m_chara_face[_loc15_] = _loc7_;
            _loc15_++;
         }
         this.m_chara_face_npc = new Array(PlayerNumFri);
         _loc15_ = 0;
         while(PlayerNumFri > _loc15_)
         {
            _loc10_ = this.m_callback.GetUserDataString(ReceiveType_ImageStrNpcStart + _loc15_);
            _loc11_ = new Bitmap(null);
            IggyFunctions.setTextureForBitmap(_loc11_,_loc10_);
            _loc11_.scaleX = 256 / _loc11_.width;
            _loc11_.scaleY = 128 / _loc11_.height;
            this.m_chara_face_npc[_loc15_] = _loc11_;
            _loc15_++;
         }
         var _loc27_:MovieClip = this.m_timeline.cha_select_cur;
         _loc27_.icn_lock.visible = false;
         this.ResetIcons();
         var _loc28_:int = this.m_callback.GetUserDataInt(ReceiveType_JoyConSingleFlag);
         var _loc29_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrL2);
         var _loc30_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrR2);
         var _loc31_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrRU);
         var _loc32_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrRD);
         var _loc33_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrRL);
         var _loc34_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrRR);
         if(_loc28_ == 1)
         {
            _loc29_ = this.m_callback.GetUserDataString(ReceiveType_KeyStrSingleLS);
            _loc30_ = this.m_callback.GetUserDataString(ReceiveType_KeyStrSingleRS);
            _loc31_ = this.m_callback.GetUserDataString(ReceiveType_KeyStrSingleU);
            _loc32_ = this.m_callback.GetUserDataString(ReceiveType_KeyStrSingleD);
            _loc33_ = this.m_callback.GetUserDataString(ReceiveType_KeyStrSingleL);
            _loc34_ = this.m_callback.GetUserDataString(ReceiveType_KeyStrSingleR);
         }
         this.m_timeline.cha_skill.inact_skill.skill01.sys_com01.htmlText = _loc30_;
         this.m_timeline.cha_skill.inact_skill.skill01.sys_com02.htmlText = _loc31_;
         this.m_timeline.cha_skill.inact_skill.skill01.sys_com03.htmlText = "";
         this.m_timeline.cha_skill.inact_skill.skill01.sys_01.visible = false;
         this.m_timeline.cha_skill.inact_skill.skill02.sys_com01.htmlText = _loc30_;
         this.m_timeline.cha_skill.inact_skill.skill02.sys_com02.htmlText = _loc33_;
         this.m_timeline.cha_skill.inact_skill.skill02.sys_com03.htmlText = "";
         this.m_timeline.cha_skill.inact_skill.skill02.sys_01.visible = false;
         this.m_timeline.cha_skill.inact_skill.skill03.sys_com01.htmlText = _loc30_;
         this.m_timeline.cha_skill.inact_skill.skill03.sys_com02.htmlText = _loc34_;
         this.m_timeline.cha_skill.inact_skill.skill03.sys_com03.htmlText = "";
         this.m_timeline.cha_skill.inact_skill.skill03.sys_01.visible = false;
         this.m_timeline.cha_skill.inact_skill.skill04.sys_com01.htmlText = _loc30_;
         this.m_timeline.cha_skill.inact_skill.skill04.sys_com02.htmlText = _loc32_;
         this.m_timeline.cha_skill.inact_skill.skill04.sys_com03.htmlText = "";
         this.m_timeline.cha_skill.inact_skill.skill04.sys_01.visible = false;
         this.m_timeline.cha_skill.inact_skill.skill05.sys_com01.htmlText = _loc29_;
         this.m_timeline.cha_skill.inact_skill.skill05.sys_com02.htmlText = _loc30_;
         this.m_timeline.cha_skill.inact_skill.skill05.sys_com03.htmlText = _loc31_;
         this.m_timeline.cha_skill.inact_skill.skill06.sys_com01.htmlText = _loc29_;
         this.m_timeline.cha_skill.inact_skill.skill06.sys_com02.htmlText = _loc30_;
         this.m_timeline.cha_skill.inact_skill.skill06.sys_com03.htmlText = _loc33_;
         this.m_timeline.cha_skill.inact_skill.skill07.sys_com01.htmlText = _loc29_;
         this.m_timeline.cha_skill.inact_skill.skill07.sys_com02.htmlText = _loc30_;
         this.m_timeline.cha_skill.inact_skill.skill07.sys_com03.htmlText = _loc32_;
         this.m_timeline.cha_skill.inact_skill.skill08.sys_com01.htmlText = _loc29_;
         this.m_timeline.cha_skill.inact_skill.skill08.sys_com02.htmlText = _loc30_;
         this.m_timeline.cha_skill.inact_skill.skill08.sys_com03.htmlText = _loc34_;
         var _loc35_:String = this.m_callback.GetUserDataString(ReceiveType_TarismanHeaderStr);
         this.m_timeline.cha_skill.inact_skill.skill09.sys_com01.htmlText = _loc35_;
         this.m_timeline.cha_parameter.nest_clothes.sys_clothes.htmlText = "";
         var _loc36_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrL1);
         var _loc37_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrR1);
         if(_loc28_)
         {
            _loc36_ = _loc29_;
            _loc37_ = _loc30_;
         }
         this.m_timeline.cha_parameter.nest_clothes.sys_l.htmlText = _loc36_;
         this.m_timeline.cha_parameter.nest_clothes.sys_r.htmlText = _loc37_;
         _loc15_ = 0;
         while(PlayerMax > _loc15_)
         {
            if(_loc15_ == this.m_current_player_index)
            {
               this.setReadyIcon(_loc15_,true,false);
            }
            else
            {
               _loc13_ = this.m_callback.GetUserDataInt(ReceiveType_PartyNpcNum);
               if(0 <= _loc15_ - 1 && _loc15_ - 1 < _loc13_)
               {
                  this.setReadyIcon(_loc15_,false,true);
               }
               else
               {
                  this.setReadyIcon(_loc15_,false,false);
               }
            }
            _loc12_ = this.getReadyIconMc(_loc15_);
            if(PlayerIndexOwn == _loc15_)
            {
               _loc12_.cmn_icn_you.visible = true;
            }
            else
            {
               _loc12_.cmn_icn_you.visible = false;
            }
            _loc15_++;
         }
         _loc15_ = 0;
         while(IndexNumCharaNewIcon > _loc15_)
         {
            _loc14_ = this.getMcChamysetNewIcon(_loc15_);
            _loc14_.visible = false;
            _loc15_++;
         }
         this.updateVariation();
         var _loc38_:int = this.m_callback.GetUserDataInt(ReceiveType_Time);
         if(_loc38_ <= 0)
         {
            this.m_timeline.timer.visible = false;
         }
         else
         {
            this.m_timeline.timer.visible = true;
            this.m_timer.Start(_loc38_,this.cbFuncEndTimer);
         }
         if(this.m_chara_num_column <= IndexNumColumn)
         {
            this.m_timeline.cha_arrow.visible = false;
         }
         trace(" ***** BGM OPTION ENABLE");
         this.m_callback.CallbackUserData("user",SendType_EnableOptionBGM,1);
         this.m_timeline.cha_select.gotoAndPlay("start");
         this.m_timeline.cha_parameter.gotoAndPlay("start");
         this.m_timeline.cha_frame.cmn_CMN_M_frame.gotoAndPlay("start");
         this.m_timeline.cha_arrow.gotoAndPlay("start");
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitMain);
      }
      
      private function waitMain(param1:Event) : void
      {
         if(this.m_timeline.cha_parameter.currentFrame != Utility.GetLabelEndFrame(this.m_timeline.cha_parameter,"start"))
         {
            return;
         }
         this.setSelectChara();
         this.updateVariation();
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitMain);
         this.m_timeline.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.main);
      }
      
      private function main(param1:Event) : void
      {
         var _loc16_:Boolean = false;
         var _loc17_:int = 0;
         var _loc2_:int = 0;
         var _loc3_:* = undefined;
         var _loc4_:Array = null;
         var _loc5_:int = 0;
         var _loc6_:int = 0;
         var _loc7_:int = 0;
         var _loc8_:int = 0;
         var _loc9_:int = int(this.m_select_info[this.m_current_player_index][SelectInfoTypeListIndex]);
         var _loc10_:Array = this.getCharaInfo(_loc9_);
         var _loc11_:int = this.m_callback.GetUserDataInt(ReceiveType_Time);
         if(_loc11_ > 0)
         {
            if(this.m_timer)
            {
               _loc2_ = this.m_timer.GetTime();
               if(this.m_pre_time != _loc2_)
               {
                  if(_loc2_ < 10)
                  {
                     _loc3_ = 8;
                     this.m_callback.CallbackUserData("user",SendType_RequestCountDownSe,_loc3_);
                  }
                  this.m_pre_time = _loc2_;
               }
               if(_loc2_ <= _loc11_)
               {
                  this.m_timeline.timer.nest._CMN_M_B_mc_timer.visible = true;
               }
               else
               {
                  this.m_timeline.timer.nest._CMN_M_B_mc_timer.visible = false;
               }
            }
         }
         this.updateLocalBattle();
         if(this.m_flag_change_player || this.m_flag_change_page)
         {
            if(this.m_count_change_disp == 0)
            {
               this.m_flag_change_disp = true;
            }
         }
         if(this.m_flag_change_disp)
         {
            if(!this.m_flag_change_player && !this.m_flag_change_page)
            {
               this.m_count_change_disp = 6;
               this.m_flag_change_disp = false;
               _loc8_ = 0;
               while(_loc8_ < this.m_icon_show_wait.length)
               {
                  this.m_icon_show_wait[_loc8_] = 0;
                  _loc8_++;
               }
            }
            else if(this.m_count_change_disp == 0)
            {
               this.updateDisply();
               --this.m_count_change_disp;
            }
            else
            {
               this.updateCharaIconUnit(this.m_select_icon_column,this.m_select_icon_row);
            }
         }
         if(this.m_count_change_disp == 5 || this.m_count_change_disp == 3 || this.m_count_change_disp == 1)
         {
            this.updateDisply();
         }
         if(this.m_count_change_disp > 0)
         {
            --this.m_count_change_disp;
         }
         if(this.m_flag_change_page)
         {
            if(this.m_timeline.cha_select.chara_icn_set01.currentLabel == "push_l_end" || this.m_timeline.cha_select.chara_icn_set01.currentLabel == "push_r_end")
            {
               this.m_flag_change_page = false;
               this.m_timeline.cha_select.chara_icn_set01.gotoAndPlay(0);
            }
         }
         var _loc12_:MovieClip = this.m_timeline.cha_select;
         var _loc13_:MovieClip = _loc12_["chara_icn_set0" + (this.m_select_icon_column + 1)];
         var _loc14_:MovieClip = _loc13_["nest_charaselect0" + (this.m_select_icon_row + 1)];
         if(!this.m_flag_change_player)
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         var _loc15_:int = 0;
         if(this.m_flag_decide)
         {
            _loc15_ = this.m_current_select_index - 1;
            if(0 <= _loc15_ && _loc15_ < this.m_selected_index_list.length)
            {
               this.m_selected_index_list[_loc15_] = _loc9_;
            }
         }
         else
         {
            _loc15_ = this.m_current_select_index;
            if(0 <= _loc15_ && _loc15_ < this.m_selected_index_list.length)
            {
               this.m_selected_index_list[_loc15_] = -1;
            }
         }
         if(this.m_flag_decide)
         {
            if(this.m_timeline.cha_parameter.currentLabel == "start" || this.m_timeline.cha_parameter.currentLabel == "wait")
            {
               this.m_timeline.cha_parameter.gotoAndPlay("push");
               if(_loc10_)
               {
                  _loc4_ = _loc10_[this.m_select_var][VarTypeVoiceIdList];
                  _loc5_ = Math.floor(Math.random() * _loc4_.length);
                  _loc6_ = int(_loc4_[_loc5_]);
                  this.m_callback.CallbackUserData("user",SendType_RequestPlayVoice,_loc6_);
               }
               trace(" ----- BGM OPTION DISABLE");
               this.m_callback.CallbackUserData("user",SendType_EnableOptionBGM,0);
            }
            if(this.m_timeline.cha_parameter.currentFrame < Utility.GetLabelEndFrame(this.m_timeline.cha_parameter,"push"))
            {
               return;
            }
         }
         if(this.m_timeline.cha_parameter.currentLabel == "end_comp")
         {
            this.m_timeline.cha_parameter.nest_clothes.visible = false;
            if(this.m_flag_exit)
            {
               this.end();
               return;
            }
            this.changeCurrentPlayer(this.m_flag_decide);
            this.updatePage();
            this.setSelectChara();
            this.sendCharaInfo(this.m_current_player_index);
            this.m_flag_decide = false;
            this.m_flag_change_player = false;
            _loc16_ = false;
            if(this.m_current_player_index < 0 || PlayerMax <= this.m_current_player_index)
            {
               _loc16_ = true;
            }
            if(_loc16_)
            {
               this.end();
               return;
            }
            _loc17_ = this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle);
            if(!_loc17_)
            {
               if(_loc11_ > 0)
               {
                  this.m_timer.Start(_loc11_ + 1,this.cbFuncEndTimer);
                  this.m_timeline.timer.nest._CMN_M_B_mc_timer.visible = false;
               }
            }
            this.m_timeline.cha_parameter.gotoAndPlay("start");
            this.m_timeline.cha_parameter.nest_clothes.visible = true;
            this.m_timeline.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
            trace(" ***** BGM OPTION ENABLE");
            this.m_callback.CallbackUserData("user",SendType_EnableOptionBGM,1);
            return;
         }
         if(this.m_timeline.cha_parameter.currentLabel == "end")
         {
            return;
         }
         trace(" ----- BGM OPTION DISABLE");
         this.m_callback.CallbackUserData("user",SendType_EnableOptionBGM,0);
         this.m_timeline.cha_parameter.gotoAndPlay("end");
         if(!this.m_flag_exit)
         {
            _loc7_ = -1;
            if(this.m_timer)
            {
               _loc7_ = this.m_timer.GetTime();
            }
            this.m_callback.CallbackUserData("user",SendType_RequestDecide,_loc7_);
         }
      }
      
      private function end() : void
      {
         this.m_timeline.cha_select.gotoAndPlay("end");
         this.m_timeline.cha_arrow.gotoAndPlay("end");
         if(this.m_timeline.cha_skill.currentFrame <= Utility.GetLabelEndFrame(this.m_timeline.cha_skill,"wait"))
         {
            this.m_timeline.cha_skill.gotoAndPlay("end");
         }
         this.m_timeline.cha_frame.cmn_CMN_M_frame.gotoAndPlay("end");
         this.m_timeline.cha_select_cur.visible = false;
         this.m_timer.End();
         this.m_timeline.timer.gotoAndPlay("end");
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_Time);
         if(_loc1_ > 0)
         {
            this.m_timer.Stop();
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.main);
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         if(this.m_current_player_index < 0)
         {
            this.m_callback.CallbackCancel();
         }
         else
         {
            this.m_callback.CallbackDecide(1);
         }
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitEnd);
      }
      
      private function waitEnd(param1:Event) : void
      {
         if(this.m_timeline.cha_select.currentFrame != Utility.GetLabelEndFrame(this.m_timeline.cha_select,"end"))
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEnd);
         this.m_callback.CallbackExit();
      }
      
      private function cbFuncEndTimer() : void
      {
         this.pushKeyDecide();
      }
      
      private function updateLocalBattle() : void
      {
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle);
         if(!_loc1_)
         {
            return;
         }
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveType_Time);
         var _loc3_:int = this.m_callback.GetUserDataInt(ReceiveType_Flag2pController);
         if(_loc3_)
         {
            if(this.m_timeline.press2P.currentLabel != "end" && this.m_timeline.press2P.currentLabel != "end_comp")
            {
               this.m_timeline.press2P.gotoAndPlay("end");
               if(_loc2_ >= 0)
               {
                  if(this.m_current_player_index != 0)
                  {
                     if(this.m_timer)
                     {
                        this.m_timer.Start(_loc2_ + 1,this.cbFuncEndTimer);
                        this.m_timeline.timer.nest._CMN_M_B_mc_timer.visible = false;
                     }
                  }
               }
            }
         }
         else if(this.m_timeline.press2P.currentLabel != "start" && this.m_timeline.press2P.currentLabel != "wait")
         {
            this.m_timeline.press2P.gotoAndPlay("start");
            this.m_timeline.press2P.nest.sys.sys.htmlText = this.m_callback.GetUserDataString(ReceiveType_Str2pController);
         }
         switch(this.m_current_player_index)
         {
            case 0:
               if(!this.m_timeline.cha_parameter.icon_1P.visible)
               {
                  this.m_timeline.cha_parameter.icon_1P.gotoAndPlay("start");
               }
               this.m_timeline.cha_parameter.icon_1P.visible = true;
               this.m_timeline.cha_parameter.icon_2P.visible = false;
               break;
            case 3:
               this.m_timeline.cha_parameter.icon_1P.visible = false;
               if(!this.m_timeline.cha_parameter.icon_2P.visible)
               {
                  this.m_timeline.cha_parameter.icon_2P.gotoAndPlay("start");
                  if(_loc2_ >= 0)
                  {
                     if(this.m_timer)
                     {
                        if(_loc3_)
                        {
                           this.m_timer.Start(_loc2_ + 1,this.cbFuncEndTimer);
                           this.m_timeline.timer.nest._CMN_M_B_mc_timer.visible = false;
                        }
                        else
                        {
                           this.m_timer.Stop();
                        }
                     }
                  }
               }
               this.m_timeline.cha_parameter.icon_2P.visible = true;
               break;
            default:
               this.m_timeline.cha_parameter.icon_1P.visible = false;
               this.m_timeline.cha_parameter.icon_2P.visible = false;
         }
      }
      
      private function getCharaInfo(param1:int) : Array
      {
         if(param1 < 0)
         {
            return null;
         }
         if(this.m_chara_list_num <= param1)
         {
            return null;
         }
         if(this.m_chara_list[param1][0] < 0)
         {
            return null;
         }
         return this.m_chara_list[param1];
      }
      
      public function GetSelectVarInfo(param1:int) : Array
      {
         if(param1 < 0 || PlayerMax <= param1)
         {
            return null;
         }
         var _loc2_:int = int(this.m_select_info[param1][SelectInfoTypeListIndex]);
         var _loc3_:Array = this.getCharaInfo(_loc2_);
         if(!_loc3_)
         {
            return null;
         }
         var _loc4_:int = int(this.m_select_info[param1][SelectInfoTypeVarIndex]);
         _loc4_ = _loc4_ + _loc3_.length;
         _loc4_ = _loc4_ % _loc3_.length;
         return _loc3_[_loc4_];
      }
      
      private function getReadyIconMc(param1:int) : MovieClip
      {
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveType_PlayerFriNum);
         var _loc3_:int = 0;
         if(_loc2_ < param1)
         {
            _loc3_ = param1;
         }
         else
         {
            _loc3_ = _loc2_ - param1;
         }
         return this.m_timeline.cha_parameter.nest_ready["btnact_rb0" + (_loc3_ + 1)];
      }
      
      private function calcCharaListIndex(param1:int, param2:int) : int
      {
         return param1 + param2 * IndexNumRow;
      }
      
      private function calcIconIndexRow(param1:int) : int
      {
         if(param1 < 0 || this.m_chara_list_num <= param1)
         {
            return -1;
         }
         return param1 % IndexNumRow;
      }
      
      private function calcIconIndexColumn(param1:int) : int
      {
         if(param1 < 0 || this.m_chara_list_num <= param1)
         {
            return -1;
         }
         return param1 / IndexNumRow;
      }
      
      private function getVarIndexNum(param1:int) : int
      {
         var _loc2_:int = 0;
         var _loc3_:int = 0;
         _loc2_ = 0;
         while(CharaVarIndexNum > _loc2_)
         {
            if(this.checkUnlockVar(param1,_loc2_))
            {
               _loc3_++;
            }
            _loc2_++;
         }
         return _loc3_;
      }
      
      private function updatePage() : void
      {
         var _loc1_:int = 0;
         var _loc2_:int = 0;
         var _loc3_:int = 0;
         var _loc4_:MovieClip = null;
         var _loc5_:Boolean = false;
         var _loc6_:Boolean = false;
         if(this.m_select_column < this.m_select_column_start)
         {
            this.m_select_column_start = this.m_select_column;
            _loc5_ = true;
            _loc6_ = true;
         }
         else if(this.m_select_column_start + IndexNumColumn <= this.m_select_column)
         {
            this.m_select_column_start = this.m_select_column - IndexNumColumn + 1;
            _loc5_ = true;
         }
         if(!_loc5_)
         {
            return;
         }
         this.m_flag_change_page = true;
         var _loc7_:MovieClip = this.m_timeline.cha_select;
         var _loc8_:MovieClip = this.m_timeline.cha_new;
         var _loc9_:MovieClip = null;
         _loc2_ = -1;
         while(IndexNumColumn + 1 > _loc2_)
         {
            _loc4_ = _loc7_["chara_icn_set0" + (_loc2_ + 1)];
            if(_loc6_)
            {
               _loc4_.gotoAndPlay("push_l");
               this.m_timeline.cha_arrow.spinbtn_l.cmn_CMN_M_B_mc_spinbtn_l.gotoAndPlay("push");
               _loc3_ = 0;
               while(IndexNumCharaNewIcon > _loc3_)
               {
                  _loc9_ = _loc8_["chara_new_" + _loc3_];
                  _loc9_.gotoAndPlay("push_l");
                  _loc3_++;
               }
            }
            else
            {
               _loc4_.gotoAndPlay("push_r");
               this.m_timeline.cha_arrow.spinbtn_r.cmn_CMN_M_B_mc_spinbtn_r.gotoAndPlay("push");
               _loc3_ = 0;
               while(IndexNumCharaNewIcon > _loc3_)
               {
                  _loc9_ = _loc8_["chara_new_" + _loc3_];
                  _loc9_.gotoAndPlay("push_r");
                  _loc3_++;
               }
            }
            _loc2_++;
         }
      }
      
      private function changeLR(param1:Boolean, param2:Boolean) : void
      {
         var _loc3_:int = 0;
         var _loc4_:int = 0;
         var _loc5_:Boolean = false;
         while(true)
         {
            if(param1)
            {
               if(this.m_select_column == 0 && this.m_select_row == 0)
               {
                  break;
               }
               if(param2)
               {
                  this.m_select_column -= 7;
               }
               else
               {
                  --this.m_select_column;
               }
               if(this.m_select_column < 0)
               {
                  this.m_select_column = this.m_chara_num_column - 1;
                  --this.m_select_row;
                  this.m_select_row += IndexNumRow;
                  this.m_select_row %= IndexNumRow;
               }
            }
            else
            {
               if(param2)
               {
                  this.m_select_column += 7;
               }
               else
               {
                  ++this.m_select_column;
               }
               if(this.m_chara_num_column <= this.m_select_column)
               {
                  this.m_select_column = 0;
                  ++this.m_select_row;
                  this.m_select_row += IndexNumRow;
                  this.m_select_row %= IndexNumRow;
               }
            }
            _loc3_ = this.calcCharaListIndex(this.m_select_row,this.m_select_column);
            _loc4_ = this.m_callback.GetUserDataInt(ReceiveType_FlagSelectAvatar);
            if(!_loc4_)
            {
               if(this.checkAvatar(_loc3_))
               {
                  if(this.checkSelectedAvatar(_loc3_))
                  {
                     continue;
                  }
               }
            }
            _loc5_ = this.checkUnlockChara(_loc3_);
            if(_loc5_)
            {
               this.m_select_var = 0;
               return;
            }
         }
         this.changeUD(true);
      }
      
      private function changeUD(param1:Boolean) : void
      {
         var _loc2_:int = 0;
         var _loc3_:int = 0;
         var _loc4_:Boolean = false;
         while(true)
         {
            if(param1)
            {
               --this.m_select_row;
               if(this.m_select_row < 0)
               {
                  this.m_select_row = IndexNumRow - 1;
                  --this.m_select_column;
                  this.m_select_column += this.m_chara_num_column;
                  this.m_select_column %= this.m_chara_num_column;
               }
            }
            else
            {
               ++this.m_select_row;
               if(IndexNumRow <= this.m_select_row)
               {
                  this.m_select_row = 0;
                  ++this.m_select_column;
                  this.m_select_column += this.m_chara_num_column;
                  this.m_select_column %= this.m_chara_num_column;
               }
            }
            _loc2_ = this.calcCharaListIndex(this.m_select_row,this.m_select_column);
            _loc3_ = this.m_callback.GetUserDataInt(ReceiveType_FlagSelectAvatar);
            if(!_loc3_)
            {
               if(this.checkAvatar(_loc2_))
               {
                  if(this.checkSelectedAvatar(_loc2_))
                  {
                     continue;
                  }
               }
            }
            _loc4_ = this.checkUnlockChara(_loc2_);
            if(_loc4_)
            {
               break;
            }
         }
         this.m_select_var = 0;
      }
      
      private function changeVar(param1:Boolean) : void
      {
         var _loc2_:Boolean = false;
         var _loc3_:int = this.calcCharaListIndex(this.m_select_row,this.m_select_column);
         var _loc4_:Array = this.getCharaInfo(_loc3_);
         if(!_loc4_)
         {
            return;
         }
         do
         {
            if(param1)
            {
               --this.m_select_var;
            }
            else
            {
               ++this.m_select_var;
            }
            this.m_select_var += _loc4_.length;
            this.m_select_var %= _loc4_.length;
            _loc2_ = this.checkUnlockVar(_loc3_,this.m_select_var);
         }
         while(!_loc2_);
      }
      
      private function checkPlayerTeamType(param1:int) : int
      {
         var _loc2_:int = PlayerTeamTypeInvalid;
         if(param1 == PlayerIndexOwn)
         {
            _loc2_ = PlayerTeamTypeOwn;
         }
         else if(PlayerIndexFriStart <= param1 && param1 <= PlayerIndexFriEnd)
         {
            _loc2_ = PlayerTeamTypeFri;
         }
         else if(PlayerIndexEnmStart <= param1 && param1 <= PlayerIndexEnmEnd)
         {
            _loc2_ = PlayerTeamTypeEnm;
         }
         return _loc2_;
      }
      
      private function setReadyIcon(param1:int, param2:Boolean, param3:Boolean) : void
      {
         var _loc4_:int = 0;
         var _loc5_:Boolean = false;
         var _loc6_:int = 0;
         var _loc7_:int = 0;
         var _loc8_:int = 0;
         var _loc9_:int = 0;
         if(param1 < 0 || PlayerMax <= param1)
         {
            return;
         }
         var _loc10_:MovieClip = this.getReadyIconMc(param1);
         if(param3)
         {
            _loc4_ = this.checkPlayerTeamType(param1);
            _loc5_ = false;
            _loc6_ = param1 - 1;
            switch(_loc4_)
            {
               case PlayerTeamTypeOwn:
                  _loc10_.gotoAndStop("blue_team");
                  break;
               case PlayerTeamTypeFri:
                  _loc10_.gotoAndStop("blue_team");
                  _loc7_ = this.m_callback.GetUserDataInt(ReceiveType_PlayerFriNum);
                  _loc8_ = this.m_callback.GetUserDataInt(ReceiveType_PartyNpcNum);
                  if(0 <= _loc6_ && _loc6_ < _loc8_)
                  {
                     _loc5_ = true;
                  }
                  break;
               case PlayerTeamTypeEnm:
                  _loc10_.gotoAndStop("red_team");
            }
            _loc10_.sys_ready.text = "OK";
            if(_loc5_)
            {
               this.setImageFriendNpc(_loc10_.icn_chara_lit.chara_img,_loc6_);
            }
            else
            {
               _loc9_ = this.calcCharaListIndex(this.m_select_row,this.m_select_column);
               this.setImage(_loc10_.icn_chara_lit.chara_img,_loc9_,true);
            }
         }
         else
         {
            _loc10_.gotoAndStop("off");
            _loc10_.sys_ready.text = "---";
         }
         if(param2)
         {
            _loc10_.btnact_ready.gotoAndPlay("on");
         }
         else
         {
            _loc10_.btnact_ready.gotoAndPlay("off");
         }
      }
      
      private function changeCurrentPlayer(param1:Boolean) : void
      {
         var _loc2_:Boolean = false;
         var _loc3_:int = 0;
         var _loc4_:int = 0;
         var _loc5_:int = 0;
         var _loc6_:int = 0;
         var _loc7_:int = 0;
         var _loc8_:int = 0;
         this.setReadyIcon(this.m_current_player_index,false,param1);
         var _loc9_:int = this.m_callback.GetUserDataInt(ReceiveType_PlayerFriNum);
         var _loc10_:int = this.m_callback.GetUserDataInt(ReceiveType_PlayerEnmNum);
         do
         {
            if(param1)
            {
               ++this.m_current_player_index;
            }
            else
            {
               --this.m_current_player_index;
            }
            _loc2_ = false;
            _loc3_ = this.checkPlayerTeamType(this.m_current_player_index);
            switch(_loc3_)
            {
               case PlayerTeamTypeFri:
                  _loc4_ = this.m_current_player_index - PlayerIndexFriStart;
                  _loc5_ = this.m_callback.GetUserDataInt(ReceiveType_PartyNpcNum);
                  if(_loc5_ > _loc4_)
                  {
                     _loc2_ = false;
                  }
                  else if(_loc9_ > _loc4_)
                  {
                     _loc2_ = true;
                  }
                  break;
               case PlayerTeamTypeEnm:
                  _loc6_ = this.m_current_player_index - PlayerIndexEnmStart;
                  if(_loc10_ > _loc6_)
                  {
                     _loc2_ = true;
                  }
                  break;
               case PlayerTeamTypeOwn:
               default:
                  _loc2_ = true;
            }
         }
         while(!_loc2_);
         this.setReadyIcon(this.m_current_player_index,true,false);
         if(0 <= this.m_current_player_index && this.m_current_player_index < PlayerMax)
         {
            _loc7_ = int(this.m_select_info[this.m_current_player_index][SelectInfoTypeListIndex]);
            this.m_select_row = this.calcIconIndexRow(_loc7_);
            this.m_select_column = this.calcIconIndexColumn(_loc7_);
            this.m_select_var = this.m_select_info[this.m_current_player_index][SelectInfoTypeVarIndex];
            if(this.checkAvatar(_loc7_) && this.checkSelectedAvatar(_loc7_))
            {
               this.changeUD(false);
            }
         }
      }
      
      private function setSelectChara() : void
      {
         var _loc1_:int = 0;
         var _loc2_:int = 0;
         var _loc3_:int = 0;
         var _loc4_:MovieClip = null;
         var _loc5_:MovieClip = null;
         var _loc6_:MovieClip = null;
         var _loc7_:Boolean = false;
         var _loc8_:Boolean = false;
         var _loc9_:Boolean = false;
         var _loc10_:int = 0;
         var _loc11_:int = 0;
         var _loc12_:int = 0;
         if(this.m_current_player_index < 0 || PlayerMax <= this.m_current_player_index)
         {
            return;
         }
         var _loc13_:int = this.calcCharaListIndex(this.m_select_row,this.m_select_column);
         var _loc14_:Array = this.getCharaInfo(_loc13_);
         var _loc15_:int = -1;
         if(_loc14_)
         {
            _loc15_ = int(_loc14_.length);
            _loc1_ = 0;
            while(_loc15_ > _loc1_)
            {
               _loc3_ = this.m_select_var + _loc1_;
               _loc3_ %= _loc15_;
               if(this.checkUnlockVar(_loc13_,_loc3_))
               {
                  this.m_select_var = _loc3_;
                  break;
               }
               _loc1_++;
            }
         }
         var _loc16_:int = this.m_select_row;
         var _loc17_:int = this.m_select_column - this.m_select_column_start;
         var _loc18_:MovieClip = this.m_timeline.cha_select;
         var _loc19_:MovieClip = _loc18_["chara_icn_set0" + (_loc17_ + 1)];
         var _loc20_:MovieClip = _loc19_["nest_charaselect0" + (_loc16_ + 1)];
         this.m_select_icon_row = _loc16_;
         this.m_select_icon_column = _loc17_;
         _loc1_ = 0;
         while(IndexNumCharaNewIcon > _loc1_)
         {
            _loc4_ = this.getMcChamysetNewIcon(_loc1_);
            if(_loc4_)
            {
               _loc4_.visible = false;
            }
            _loc1_++;
         }
         _loc1_ = 0;
         while(CharaVarIndexNum > _loc1_)
         {
            _loc5_ = this.getMcChamyset(_loc1_);
            _loc6_ = this.getMcChamysetNewIcon(_loc1_);
            if(_loc5_)
            {
               _loc7_ = this.checkUnlockVar(_loc13_,_loc1_);
               _loc5_.btnact_off.visible = true;
               if(_loc1_ < _loc15_)
               {
                  if(_loc7_)
                  {
                     if(_loc1_ == this.m_select_var)
                     {
                        if(_loc5_.currentLabel != "on")
                        {
                           _loc5_.gotoAndPlay("on");
                           _loc5_.btnact_ef.visible = true;
                           _loc5_.btnact_on.visible = true;
                           _loc5_.btnact_off.visible = false;
                        }
                     }
                     else
                     {
                        _loc5_.gotoAndPlay("off");
                        _loc5_.btnact_ef.visible = false;
                        _loc5_.btnact_on.visible = false;
                        _loc5_.btnact_off.visible = true;
                     }
                     if(_loc6_)
                     {
                     }
                     _loc8_ = false;
                     _loc9_ = false;
                     if(_loc14_[_loc1_][VarTypeCustomCostume])
                     {
                        _loc8_ = true;
                     }
                     if(_loc14_[_loc1_][VarTypeCustomCostumeEx])
                     {
                        _loc9_ = true;
                     }
                     _loc5_.btnact_ef.blue.visible = false;
                     _loc5_.btnact_ef.red.visible = false;
                     _loc5_.btnact_ef.yellow.visible = false;
                     _loc5_.btnact_on.blue.visible = false;
                     _loc5_.btnact_on.red.visible = false;
                     _loc5_.btnact_on.yellow.visible = false;
                     _loc5_.btnact_off.blue.visible = false;
                     _loc5_.btnact_off.red.visible = false;
                     _loc5_.btnact_off.yellow.visible = false;
                     _loc10_ = 1;
                     _loc11_ = 2;
                     _loc12_ = 3;
                     if(_loc8_)
                     {
                        if(_loc9_)
                        {
                           _loc5_.btnact_ef.blue.visible = true;
                           _loc5_.btnact_on.blue.visible = true;
                           _loc5_.btnact_off.blue.visible = true;
                        }
                        else
                        {
                           _loc5_.btnact_ef.red.visible = true;
                           _loc5_.btnact_on.red.visible = true;
                           _loc5_.btnact_off.red.visible = true;
                        }
                     }
                     else
                     {
                        _loc5_.btnact_ef.yellow.visible = true;
                        _loc5_.btnact_on.yellow.visible = true;
                        _loc5_.btnact_off.yellow.visible = true;
                     }
                  }
                  else
                  {
                     _loc5_.gotoAndPlay("lock");
                     _loc5_.btnact_ef.visible = false;
                     _loc5_.btnact_on.visible = false;
                     _loc5_.btnact_off.visible = true;
                     if(_loc6_)
                     {
                        _loc6_.visible = false;
                     }
                  }
               }
               else
               {
                  _loc5_.gotoAndPlay("off");
                  _loc5_.btnact_ef.visible = false;
                  _loc5_.btnact_on.visible = false;
                  _loc5_.btnact_off.visible = false;
                  if(_loc6_)
                  {
                     _loc6_.visible = false;
                  }
               }
            }
            else if(_loc6_)
            {
               _loc6_.visible = false;
            }
            _loc1_++;
         }
         var _loc21_:int = this.getVarIndexNum(_loc13_);
         var _loc22_:* = false;
         if(_loc21_ > 1)
         {
            _loc22_ = true;
         }
         this.m_timeline.cha_parameter.nest_clothes.sys_r.visible = _loc22_;
         this.m_timeline.cha_parameter.nest_clothes.sys_l.visible = _loc22_;
         this.m_select_info[this.m_current_player_index][SelectInfoTypeListIndex] = _loc13_;
         this.m_select_info[this.m_current_player_index][SelectInfoTypeVarIndex] = this.m_select_var;
      }
      
      public function sendCharaInfo(param1:int) : void
      {
         if(param1 < 0)
         {
            return;
         }
         if(PlayerMax <= param1)
         {
            return;
         }
         var _loc2_:Array = this.GetSelectVarInfo(param1);
         if(!_loc2_)
         {
            this.SetUserDataString(CharaSele.ReceiveType_CharaNameStr,"???");
            return;
         }
         var _loc3_:String = _loc2_[VarTypeCode];
         var _loc4_:int = int(this.m_select_info[param1][SelectInfoTypeVarIndex]);
         var _loc5_:int = int(_loc2_[VarTypeMid]);
         var _loc6_:int = int(_loc2_[VarTypeModelPreset]);
         var _loc7_:int = this.calcCharaListIndex(this.m_select_row,this.m_select_column);
         var _loc8_:int = int(_loc2_[VarTypeCustomCostume]);
         var _loc9_:int = int(_loc2_[VarTypeAvatarSlotID]);
         this.m_callback.CallbackUserData("user",SendType_CurrentPlayerIndex,param1);
         this.m_callback.CallbackUserData("user",SendType_AvatarSlotID,_loc9_);
         this.m_callback.CallbackUserDataString("user",SendType_SelectCode,_loc3_);
         this.m_callback.CallbackUserData("user",SendType_SelectVariation,_loc4_);
         this.m_callback.CallbackUserData("user",SendType_SelectCustomFlag,_loc8_);
         this.m_callback.CallbackUserData("user",SendType_SelectMid,_loc5_);
         this.m_callback.CallbackUserData("user",SendType_SelectModelPreset,_loc6_);
         this.m_callback.CallbackUserData("user",SendType_RequestCharaInfo,0);
         this.m_callback.CallbackUserData("user",SendType_SelectListIndex,CharaListShiftCount[_loc7_]);
         this.m_callback.SetUserDataValidFlag(ReceiveType_CharaNameStr,false);
      }
      
      private function setImage(param1:MovieClip, param2:int, param3:Boolean) : void
      {
         var _loc4_:Bitmap = null;
         var _loc5_:String = null;
         if(!param1)
         {
            return;
         }
         while(param1.numChildren > 0)
         {
            param1.removeChildAt(0);
         }
         if(!this.checkUnlockChara(param2))
         {
            return;
         }
         if(this.m_chara_face[param2])
         {
            _loc4_ = this.m_chara_face[param2];
            if(param3)
            {
               _loc5_ = this.m_callback.GetUserDataString(ReceiveType_ImageStrStart + param2);
               _loc4_ = new Bitmap(null);
               IggyFunctions.setTextureForBitmap(_loc4_,_loc5_);
               _loc4_.scaleX = 256 / _loc4_.width;
               _loc4_.scaleY = 128 / _loc4_.height;
            }
            param1.addChild(_loc4_);
         }
      }
      
      private function setImageFriendNpc(param1:MovieClip, param2:int) : void
      {
         var _loc3_:Bitmap = null;
         if(!param1)
         {
            return;
         }
         if(param2 < 0 || PlayerNumFri <= param2)
         {
            return;
         }
         while(param1.numChildren > 0)
         {
            param1.removeChildAt(0);
         }
         if(this.m_chara_face_npc[param2])
         {
            _loc3_ = this.m_chara_face_npc[param2];
            param1.addChild(_loc3_);
         }
      }
      
      private function updateCharaIconUnit(param1:int, param2:int) : void
      {
         var _loc3_:MovieClip = null;
         var _loc4_:Array = null;
         var _loc5_:int = 0;
         var _loc6_:int = 0;
         var _loc7_:MovieClip = this.m_timeline.cha_select;
         var _loc8_:MovieClip = this.m_timeline.cha_new;
         var _loc9_:MovieClip = _loc7_["chara_icn_set0" + (param1 + 1)];
         var _loc10_:MovieClip = _loc9_["nest_charaselect0" + (param2 + 1)];
         if(!_loc10_)
         {
            return;
         }
         var _loc11_:int = this.calcCharaListIndex(param2,param1 + this.m_select_column_start);
         var _loc12_:MovieClip = _loc8_["chara_new_" + ((param1 + 1) * 3 + param2)];
         this.setImage(_loc10_.chara_img,_loc11_,false);
         if(this.checkNoChara(_loc11_))
         {
            _loc10_.visible = false;
            _loc12_.visible = false;
         }
         else if(this.checkUnlockChara(_loc11_))
         {
            _loc10_.visible = true;
            _loc12_.visible = this.checkCharaNewIconVisible(_loc11_);
            _loc3_ = this.m_timeline.cha_select_cur;
            _loc4_ = this.getCharaInfo(_loc11_);
            _loc5_ = int(_loc4_[VarTypeAvatarSlotID]);
            _loc6_ = this.m_callback.GetUserDataInt(ReceiveType_FlagSelectAvatar);
            if(!_loc6_ && this.m_current_player_index != PlayerIndexOwn && this.checkAvatar(_loc11_) && this.checkSelectedAvatar(_loc11_))
            {
               if(_loc10_.currentLabel != "grayout")
               {
                  _loc10_.gotoAndStop("grayout");
               }
               if(this.checkSelectedAvatar(_loc11_))
               {
                  if(this.m_select_column - this.m_select_column_start == param1 && this.m_select_row == param2)
                  {
                     this.setImage(_loc3_.chara_sel.chara_img,_loc11_,false);
                  }
               }
            }
            else
            {
               if(_loc10_.currentLabel != "off")
               {
                  _loc10_.gotoAndStop("off");
               }
               if(this.m_select_icon_column == param1 && this.m_select_icon_row == param2)
               {
                  _loc3_.visible = true;
                  _loc3_.x = _loc7_.x + _loc9_.x + _loc10_.x;
                  _loc3_.y = _loc7_.y + _loc9_.y + _loc10_.y;
                  this.setImage(_loc3_.chara_sel.chara_img,_loc11_,false);
                  if(this.m_flag_decide)
                  {
                     if(_loc3_.currentLabel != "push")
                     {
                        _loc3_.gotoAndStop("push");
                     }
                  }
                  else if(_loc3_.currentLabel != "on")
                  {
                     _loc3_.gotoAndStop("on");
                  }
               }
            }
         }
         else
         {
            _loc10_.visible = true;
            if(_loc10_.currentLabel != "lock")
            {
               _loc10_.gotoAndStop("lock");
            }
            _loc12_.visible = false;
         }
      }
      
      private function updateCharaIcon() : void
      {
         var _loc1_:int = 0;
         var _loc2_:int = 0;
         var _loc3_:MovieClip = this.m_timeline.cha_select;
         _loc2_ = -1;
         while(IndexNumColumn + 1 > _loc2_)
         {
            _loc1_ = 0;
            while(IndexNumRow > _loc1_)
            {
               this.updateCharaIconUnit(_loc2_,_loc1_);
               _loc1_++;
            }
            _loc2_++;
         }
         var _loc4_:MovieClip = _loc3_["chara_icn_set00"];
         if(this.m_select_column_start <= 0)
         {
            _loc4_.visible = false;
         }
         else
         {
            _loc4_.visible = true;
         }
         var _loc5_:MovieClip = _loc3_["chara_icn_set0" + (IndexNumColumn + 1)];
         if(this.m_chara_num_column <= this.m_select_column_start + IndexNumColumn)
         {
            _loc5_.visible = false;
         }
         else
         {
            _loc5_.visible = true;
         }
      }
      
      private function updateSkill() : void
      {
         var _loc1_:String = null;
         var _loc2_:Number = NaN;
         var _loc3_:Number = NaN;
         var _loc4_:String = null;
         var _loc5_:Number = NaN;
         var _loc6_:Number = NaN;
         var _loc7_:int = 0;
         this.ResetIcons();
         var _loc8_:MovieClip = this.m_timeline.cha_skill;
         if(this.m_flag_skill)
         {
            if(_loc8_.currentFrame > Utility.GetLabelEndFrame(_loc8_,"wait"))
            {
               _loc8_.gotoAndPlay("start");
               _loc8_.visible = true;
            }
            _loc7_ = 0;
            while(SkillMax > _loc7_)
            {
               _loc4_ = this.m_callback.GetUserDataString(ReceiveType_SkillNameStrStart + _loc7_);
               _loc8_.inact_skill["skill0" + (_loc7_ + 1)].sys_skill.scaleX = this.m_skill_str_scalex_default;
               _loc8_.inact_skill["skill0" + (_loc7_ + 1)].sys_skill.autoSize = "left";
               _loc8_.inact_skill["skill0" + (_loc7_ + 1)].sys_skill.htmlText = _loc4_;
               _loc5_ = Number(_loc8_.inact_skill["skill0" + (_loc7_ + 1)].sys_skill.width);
               _loc6_ = this.m_skill_str_scalex_default;
               if(this.m_skill_str_width_default < _loc5_)
               {
                  _loc6_ = this.m_skill_str_width_default / _loc5_;
               }
               _loc8_.inact_skill["skill0" + (_loc7_ + 1)].sys_skill.scaleX = _loc6_;
               _loc7_++;
            }
            _loc1_ = this.m_callback.GetUserDataString(ReceiveType_TarismanNameStr);
            this.showStyle(_loc8_,styleAfterText(_loc1_));
            _loc1_ = textBeforeStyle(_loc1_);
            _loc8_.inact_skill.skill09.sys_skill.scaleX = this.m_skill_str_scalex_default;
            _loc8_.inact_skill.skill09.sys_skill.autoSize = "left";
            _loc8_.inact_skill.skill09.sys_skill.htmlText = _loc1_;
            _loc2_ = Number(_loc8_.inact_skill.skill09.sys_skill.width);
            _loc3_ = this.m_skill_str_scalex_default;
            if(this.m_skill_str_width_default < _loc2_)
            {
               _loc3_ = this.m_skill_str_width_default / _loc2_;
            }
            _loc8_.inact_skill.skill09.sys_skill.scaleX = _loc3_ - 0.15;
            _loc8_.inact_skill.skill09.sys_skill.scaleY = 1;
         }
         else if(_loc8_.currentFrame <= Utility.GetLabelEndFrame(_loc8_,"wait"))
         {
            _loc8_.gotoAndPlay("end");
         }
      }
      
      // ---- BattleStyles: the Battle Style row ----

      // Shows the row with `style`, or hides it (null: not a CaC, or no mod).
      private function showStyle(popup:MovieClip, style:String) : void
      {
         this.makeStyleRow(popup);
         if(this.m_style_row == null)
         {
            return;
         }
         this.m_style_row.visible = style != null;
         if(style == null)
         {
            return;
         }
         var header:TextField = this.m_style_row.sys_com01;
         var name:TextField = this.m_style_row.sys_skill;
         header.text = StyleRowHeader;
         name.scaleX = this.m_skill_str_scalex_default;
         name.autoSize = "left";
         name.htmlText = style;
         var scale:Number = this.m_skill_str_scalex_default;
         if(this.m_skill_str_width_default < name.width)
         {
            scale = this.m_skill_str_width_default / name.width;
         }
         name.scaleX = scale - TarismanNameShrink;
         name.scaleY = 1;
      }

      // Adds the row: the header and name fields of skill09 (Super Soul),
      // one row step lower, with the popup's backing grown to fit. Any
      // failure leaves the vanilla popup. Sends the layout to the log.
      private function makeStyleRow(popup:MovieClip) : void
      {
         if(this.m_style_row_made)
         {
            return;
         }
         this.m_style_row_made = true;
         var notes:Array = [];
         try
         {
            var rows:MovieClip = popup.inact_skill;
            var last:MovieClip = rows.skill09;
            var row:MovieClip = new MovieClip();
            row.transform.matrix = last.transform.matrix;
            row.y = last.y + (last.y - rows.skill08.y);
            var header:TextField = copyTextField(last.sys_com01);
            var name:TextField = copyTextField(last.sys_skill);
            header.name = "sys_com01";
            name.name = "sys_skill";
            row.addChild(header);
            row.addChild(name);
            row.sys_com01 = header;
            row.sys_skill = name;
            rows.addChild(row);
            this.m_style_row = row;
            growBackgrounds(popup,rows.skill01,last,row,notes);
         }
         catch(e:Error)
         {
            notes.push("no row: " + e.toString());
            removeFromParent(this.m_style_row);
            this.m_style_row = null;
         }
         this.sendLayout("CHARASELE",popup,notes);
      }

      // The layout under `root`, a line per object, to the mod's log.
      private function sendLayout(file:String, root:DisplayObject, notes:Array) : void
      {
         try
         {
            var lines:Array = [file + " layout: " + notes.join(", ")];
            describeTree(root,5,"",lines);
            this.m_callback.CallbackUserDataString("user",SendType_StyleLayoutDump,lines.join("\n"));
         }
         catch(e:Error)
         {
            trace("[" + file + "] layout dump: " + e.toString());
         }
      }

      // `value` without the appended style (the game's own text).
      private static function textBeforeStyle(value:String) : String
      {
         if(value == null)
         {
            return value;
         }
         var at:int = value.indexOf(StyleSeparator);
         return at < 0 ? value : value.substr(0,at);
      }

      // The style appended to `value`, or null when there is none (no mod
      // DLL, or the game's own string).
      private static function styleAfterText(value:String) : String
      {
         if(value == null)
         {
            return null;
         }
         var at:int = value.indexOf(StyleSeparator);
         return at < 0 ? null : value.substr(at + 1);
      }

      private static function removeFromParent(object:DisplayObject) : void
      {
         if(object != null && object.parent != null)
         {
            object.parent.removeChild(object);
         }
      }

      // A text-only copy of a row clip (a MovieClip holding the TextField
      // `sys`), `step` lower. Iggy draws only fonts the movie already uses,
      // so the field copies the original's format. Utility.SetAutoSized*
      // keeps the field's first width on the row clip
      // (user_add_property_default_*) and squeezes wider text into it: the
      // copy takes the original's, not its own (a copied empty value field
      // is a few pixels wide, which squeezed the style name to nothing).
      private static function copyRow(row:MovieClip, step:Number) : MovieClip
      {
         var copy:MovieClip = new MovieClip();
         copy.transform.matrix = row.transform.matrix;
         copy.y = row.y + step;
         var field:TextField = copyTextField(row.sys);
         field.name = "sys";
         copy.addChild(field);
         copy.sys = field;
         if(row.user_add_property_default_width != undefined)
         {
            copy.user_add_property_default_width = row.user_add_property_default_width;
            copy.user_add_property_default_align = row.user_add_property_default_align;
            copy.user_add_property_default_x = row.user_add_property_default_x;
            field.width = row.user_add_property_default_width;
            field.x = row.user_add_property_default_x;
         }
         return copy;
      }

      private static function copyTextField(source:TextField) : TextField
      {
         var field:TextField = new TextField();
         field.defaultTextFormat = source.getTextFormat();
         field.embedFonts = source.embedFonts;
         field.selectable = false;
         field.mouseEnabled = false;
         field.multiline = false;
         field.wordWrap = false;
         field.width = source.scaleX != 0 ? source.width / Math.abs(source.scaleX) : source.width;
         field.height = source.scaleY != 0 ? source.height / Math.abs(source.scaleY) : source.height;
         field.transform.matrix = source.transform.matrix;
         field.filters = source.filters;
         field.alpha = source.alpha;
         return field;
      }

      // Whether `row` covers any of the panel's own rows or headers.
      private static function overlapsRows(panel:MovieClip, row:DisplayObject) : Boolean
      {
         var bounds:Rectangle = row.getBounds(panel);
         var index:int = 0;
         while(index < panel.numChildren)
         {
            var child:DisplayObject = panel.getChildAt(index);
            index++;
            if(child == row || child.name == null || !child.visible)
            {
               continue;
            }
            if(child.name.indexOf("sys_") != 0 && child.name.indexOf("po_") != 0)
            {
               continue;
            }
            if(child.getBounds(panel).intersects(bounds))
            {
               return true;
            }
         }
         return false;
      }

      // Stretches every plain object under `root` (3 levels down) that backs
      // the rows first..last - covers them, but is less than 3 times their
      // height, so not a full-screen layer - down by the added row, keeping
      // its top and its bottom margin. Backings that already reach past the
      // row grow too (the Presets window has a margin the row fills).
      private static function growBackgrounds(root:DisplayObjectContainer, first:DisplayObject, last:DisplayObject, added:DisplayObject, notes:Array) : void
      {
         var top:Number = first.getBounds(root).top;
         var bottom:Number = last.getBounds(root).bottom;
         var newBottom:Number = added.getBounds(root).bottom;
         if(newBottom <= bottom)
         {
            return;
         }
         growIn(root,root,first,added,top,bottom,newBottom,3,notes);
      }

      private static function growIn(root:DisplayObjectContainer, container:DisplayObjectContainer, first:DisplayObject, added:DisplayObject, top:Number, bottom:Number, newBottom:Number, depth:int, notes:Array) : void
      {
         var index:int = 0;
         while(index < container.numChildren)
         {
            var child:DisplayObject = container.getChildAt(index);
            index++;
            if(child == added || !child.visible || child is TextField)
            {
               continue;
            }
            var holder:DisplayObjectContainer = child as DisplayObjectContainer;
            if(holder != null && (holder.contains(first) || holder.contains(added)))
            {
               if(depth > 0)
               {
                  growIn(root,holder,first,added,top,bottom,newBottom,depth - 1,notes);
               }
               continue;
            }
            var bounds:Rectangle = child.getBounds(root);
            if(bounds.top > top + 2 || bounds.bottom < bottom - 2 || bounds.height > (bottom - top) * 3)
            {
               continue;
            }
            var scale:Number = (newBottom + (bounds.bottom - bottom) - bounds.top) / bounds.height;
            var oldTop:Number = bounds.top;
            child.scaleY *= scale;
            child.y += oldTop - child.getBounds(root).top;
            notes.push("grew " + child.name + " x" + Math.round(scale * 100) / 100);
         }
      }

      // `object` and its children as text: name, type, position, scale, box
      // in its parent, visibility, text.
      private static function describeTree(object:DisplayObject, depth:int, indent:String, lines:Array) : void
      {
         var bounds:Rectangle = object.getBounds(object.parent != null ? object.parent : object);
         var line:String = indent + object.name + " " + String(object) + " at " + Math.round(object.x) + "," + Math.round(object.y) + " scale " + Math.round(object.scaleX * 100) / 100 + "," + Math.round(object.scaleY * 100) / 100 + " box " + Math.round(bounds.x) + "," + Math.round(bounds.y) + " " + Math.round(bounds.width) + "x" + Math.round(bounds.height) + (object.visible ? "" : " hidden");
         if(object is TextField)
         {
            line += " \"" + TextField(object).text.substr(0,24) + "\"";
         }
         lines.push(line);
         var container:DisplayObjectContainer = object as DisplayObjectContainer;
         if(container == null || depth <= 0)
         {
            return;
         }
         var index:int = 0;
         while(index < container.numChildren && index < 48)
         {
            describeTree(container.getChildAt(index),depth - 1,indent + "  ",lines);
            index++;
         }
      }

      private function updateVariation() : *
      {
         var _loc1_:int = 0;
         var _loc2_:MovieClip = null;
         var _loc3_:Boolean = false;
         var _loc4_:Boolean = false;
         var _loc5_:int = this.calcCharaListIndex(this.m_select_row,this.m_select_column);
         if(this.checkNoChara(_loc5_))
         {
            _loc5_ = 0;
         }
         _loc1_ = 0;
         while(IndexNumCharaVariationNewIcon > _loc1_)
         {
            _loc2_ = this.getMcVariationNewIcon(_loc1_);
            _loc3_ = this.checkUnlockVar(_loc5_,_loc1_);
            if(_loc3_)
            {
               _loc4_ = this.checkVariationNewIconVisible(_loc5_,_loc1_);
               if(_loc2_.visible != _loc4_)
               {
                  if(this.m_icon_show_wait[_loc1_] < 1)
                  {
                     this.m_icon_show_wait[_loc1_] += 1;
                  }
                  else
                  {
                     this.m_icon_show_wait[_loc1_] = 0;
                     _loc2_.visible = _loc4_;
                  }
               }
               else
               {
                  this.m_icon_show_wait[_loc1_] = 0;
               }
            }
            else
            {
               _loc2_.visible = false;
            }
            trace("m_icon_show_wait[" + _loc1_ + "]:" + this.m_icon_show_wait[_loc1_] + " visible:" + _loc2_.visible);
            _loc1_++;
         }
      }
      
      private function updateDisply() : *
      {
         var _loc1_:* = undefined;
         trace("updateDisplay");
         var _loc2_:int = int(this.m_select_info[this.m_current_player_index][SelectInfoTypeListIndex]);
         var _loc3_:Array = this.getCharaInfo(_loc2_);
         this.updateCharaIcon();
         var _loc4_:* = this.m_callback.GetUserDataString(ReceiveType_CharaNameStr);
         var _loc5_:String = "";
         if(_loc3_[this.m_select_var][VarTypeFlag_GK2] == true)
         {
            _loc5_ = this.m_callback.GetUserDataString(ReceiveType_NameOption_GK2);
         }
         else if(_loc3_[this.m_select_var][VarTypeFlag_CGK] == true)
         {
            _loc5_ = this.m_callback.GetUserDataString(ReceiveType_NameOption_CGK);
         }
         else if(_loc3_[this.m_select_var][VarTypeFlag_KFK] == true)
         {
            _loc5_ = this.m_callback.GetUserDataString(ReceiveType_NameOption_KFK);
         }
         _loc4_ += _loc5_;
         var _loc6_:int = this.m_callback.GetUserDataInt(ReceiveType_NameOption_LvNum);
         if(_loc6_ != -1)
         {
            _loc1_ = this.m_callback.GetUserDataString(ReceiveType_NameOption_LvText);
            _loc4_ += " (";
            _loc4_ = _loc4_ + _loc1_;
            _loc4_ = _loc4_ + _loc6_.toString();
            _loc4_ = _loc4_ + ")";
         }
         Utility.SetAutoSizedhtmlText(this.m_timeline.cha_parameter.sys_charaName.sys_charaName,_loc4_);
         var _loc7_:String = this.m_callback.GetUserDataString(ReceiveType_VariationNameStr);
         this.m_timeline.cha_parameter.nest_clothes.sys_charavari_name.sys_charavari_name.htmlText = _loc7_;
         this.updateSkill();
         this.updateVariation();
      }
      
      private function getMcChamyset(param1:int) : MovieClip
      {
         var _loc2_:MovieClip = null;
         if(param1 < 9)
         {
            _loc2_ = this.m_timeline.cha_parameter.nest_clothes["btnact_chamyset_0" + (param1 + 1)];
         }
         else
         {
            _loc2_ = this.m_timeline.cha_parameter.nest_clothes["btnact_chamyset_" + (param1 + 1)];
         }
         return _loc2_;
      }
      
      private function getMcChamysetNewIcon(param1:int) : MovieClip
      {
         var _loc2_:MovieClip = null;
         return this.m_timeline.cha_new["chara_new_" + param1];
      }
      
      private function getMcVariationNewIcon(param1:int) : MovieClip
      {
         var _loc2_:MovieClip = null;
         return this.m_timeline.cha_parameter.nest_clothes["charavari_new_" + param1];
      }
      
      private function checkUnlockVar(param1:int, param2:int) : Boolean
      {
         if(param1 < 0 || this.m_chara_list_num <= param1)
         {
            return false;
         }
         if(param2 < 0 || CharaVarIndexNum <= param2)
         {
            return false;
         }
         var _loc3_:int = param1 * CharaVarIndexNum + param2;
         return Boolean(this.m_callback.GetUserDataInt(ReceiveType_UnlockVarStart + _loc3_));
      }
      
      private function checkUnlockChara(param1:*) : Boolean
      {
         if(param1 < 0 || this.m_chara_list_num <= param1)
         {
            return false;
         }
         var _loc2_:int = 0;
         _loc2_ = 0;
         while(CharaVarIndexNum > _loc2_)
         {
            if(this.checkUnlockVar(param1,_loc2_))
            {
               return true;
            }
            _loc2_++;
         }
         return false;
      }
      
      private function checkAvatar(param1:int) : Boolean
      {
         var _loc2_:Array = this.getCharaInfo(param1);
         if(!_loc2_)
         {
            return false;
         }
         if(_loc2_.length <= 0)
         {
            return false;
         }
         var _loc3_:String = _loc2_[0][VarTypeCode];
         if(_loc3_ != AvatarCode)
         {
            return false;
         }
         return true;
      }
      
      private function checkNoChara(param1:int) : Boolean
      {
         var _loc2_:Array = this.getCharaInfo(param1);
         if(!_loc2_)
         {
            return true;
         }
         if(_loc2_.length <= 0)
         {
            return true;
         }
         var _loc3_:String = _loc2_[0][VarTypeCode];
         if(_loc3_ == InvalidCode)
         {
            return true;
         }
         return false;
      }
      
      private function checkCharaNewIconVisible(param1:int) : Boolean
      {
         if(this.checkNoChara(param1))
         {
            return false;
         }
         if(this.checkAvatar(param1))
         {
            return false;
         }
         if(CharaListShiftCount.length <= param1)
         {
            return false;
         }
         var _loc2_:int = int(CharaListShiftCount[param1]);
         return this.m_callback.GetUserDataInt(ReceiveType_CharaSelectedStart + _loc2_) == 0;
      }
      
      private function checkVariationNewIconVisible(param1:int, param2:int) : Boolean
      {
         if(this.checkNoChara(param1))
         {
            return false;
         }
         if(this.checkAvatar(param1))
         {
            return false;
         }
         var _loc3_:Array = this.getCharaInfo(param1);
         if(param2 >= _loc3_.length)
         {
            return false;
         }
         return this.m_callback.GetUserDataInt(ReceiveType_CharaVariationStart + param2) == 0;
      }
      
      private function checkSelectedAvatar(param1:int) : Boolean
      {
         if(param1 == -1)
         {
            return false;
         }
         var _loc2_:int = 0;
         while(_loc2_ < this.m_selected_index_list.length)
         {
            if(this.m_selected_index_list[_loc2_] == param1)
            {
               return true;
            }
            _loc2_++;
         }
         return false;
      }
      
      private function pushLeft() : void
      {
         if(Boolean(this.m_current_player_index != PlayerIndexOwn) && Boolean(this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle)) && !this.m_callback.GetUserDataInt(ReceiveType_Flag2pController))
         {
            return;
         }
         this.changeLR(true,false);
         this.updatePage();
         this.setSelectChara();
         this.sendCharaInfo(this.m_current_player_index);
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
      }
      
      private function pushRight() : void
      {
         if(Boolean(this.m_current_player_index != PlayerIndexOwn) && Boolean(this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle)) && !this.m_callback.GetUserDataInt(ReceiveType_Flag2pController))
         {
            return;
         }
         this.changeLR(false,false);
         this.updatePage();
         this.setSelectChara();
         this.sendCharaInfo(this.m_current_player_index);
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
      }
      
      private function pushUp() : void
      {
         if(Boolean(this.m_current_player_index != PlayerIndexOwn) && Boolean(this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle)) && !this.m_callback.GetUserDataInt(ReceiveType_Flag2pController))
         {
            return;
         }
         this.changeUD(true);
         this.updatePage();
         this.setSelectChara();
         this.sendCharaInfo(this.m_current_player_index);
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
      }
      
      private function pushDown() : void
      {
         if(Boolean(this.m_current_player_index != PlayerIndexOwn) && Boolean(this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle)) && !this.m_callback.GetUserDataInt(ReceiveType_Flag2pController))
         {
            return;
         }
         this.changeUD(false);
         this.updatePage();
         this.setSelectChara();
         this.sendCharaInfo(this.m_current_player_index);
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
      }
      
      private function pushKeyPrevPage() : void
      {
         if(Boolean(this.m_current_player_index != PlayerIndexOwn) && Boolean(this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle)) && !this.m_callback.GetUserDataInt(ReceiveType_Flag2pController))
         {
            return;
         }
         this.changeLR(true,true);
         this.updatePage();
         this.setSelectChara();
         this.sendCharaInfo(this.m_current_player_index);
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
      }
      
      private function pushKeyNextPage() : void
      {
         if(Boolean(this.m_current_player_index != PlayerIndexOwn) && Boolean(this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle)) && !this.m_callback.GetUserDataInt(ReceiveType_Flag2pController))
         {
            return;
         }
         this.changeLR(false,true);
         this.updatePage();
         this.setSelectChara();
         this.sendCharaInfo(this.m_current_player_index);
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
      }
      
      private function pushKeyL() : void
      {
         if(Boolean(this.m_current_player_index != PlayerIndexOwn) && Boolean(this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle)) && !this.m_callback.GetUserDataInt(ReceiveType_Flag2pController))
         {
            return;
         }
         this.changeVar(true);
         this.setSelectChara();
         this.sendCharaInfo(this.m_current_player_index);
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
      }
      
      private function pushKeyR() : void
      {
         if(Boolean(this.m_current_player_index != PlayerIndexOwn) && Boolean(this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle)) && !this.m_callback.GetUserDataInt(ReceiveType_Flag2pController))
         {
            return;
         }
         this.changeVar(false);
         this.setSelectChara();
         this.sendCharaInfo(this.m_current_player_index);
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
      }
      
      private function pushKeySkill() : void
      {
         if(Boolean(this.m_current_player_index != PlayerIndexOwn) && Boolean(this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle)) && !this.m_callback.GetUserDataInt(ReceiveType_Flag2pController))
         {
            return;
         }
         if(this.m_flag_skill)
         {
            this.m_flag_skill = false;
            this.m_callback.CallbackUserData("user",SendType_RequestSetFlagSkill,0);
         }
         else
         {
            this.m_flag_skill = true;
            this.m_callback.CallbackUserData("user",SendType_RequestSetFlagSkill,1);
         }
      }
      
      private function pushKeyRandom() : void
      {
         var _loc1_:int = 0;
         var _loc2_:* = 0;
         var _loc3_:int = 0;
         var _loc5_:int = 0;
         var _loc6_:int = 0;
         if(Boolean(this.m_current_player_index != PlayerIndexOwn) && Boolean(this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle)) && !this.m_callback.GetUserDataInt(ReceiveType_Flag2pController))
         {
            return;
         }
         _loc1_ = 0;
         _loc2_ = 0;
         _loc1_ = 0;
         while(this.m_chara_list_num > _loc1_)
         {
            if(this.checkUnlockChara(_loc1_))
            {
               _loc2_++;
            }
            _loc1_++;
         }
         if(this.m_current_player_index != PlayerIndexOwn)
         {
            _loc2_--;
         }
         _loc3_ = 0;
         if(_loc2_ > 1)
         {
            _loc3_ = Math.floor(Math.random() * _loc2_);
         }
         _loc1_ = 0;
         while(_loc3_ > _loc1_)
         {
            this.changeUD(false);
            this.updatePage();
            _loc1_++;
         }
         var _loc4_:int = this.calcCharaListIndex(this.m_select_row,this.m_select_column);
         _loc5_ = this.getVarIndexNum(_loc4_);
         _loc6_ = 0;
         if(_loc5_ > 1)
         {
            _loc6_ = Math.floor(Math.random() * _loc5_);
         }
         _loc1_ = 0;
         while(_loc6_ > _loc1_)
         {
            this.changeVar(false);
            _loc1_++;
         }
         this.setSelectChara();
         this.sendCharaInfo(this.m_current_player_index);
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
      }
      
      private function pushKeyDecide() : void
      {
         if(this.m_flag_change_player)
         {
            return;
         }
         if(Boolean(this.m_current_player_index != PlayerIndexOwn) && Boolean(this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle)) && !this.m_callback.GetUserDataInt(ReceiveType_Flag2pController))
         {
            return;
         }
         ++this.m_current_select_index;
         this.m_flag_change_player = true;
         this.m_flag_decide = true;
         this.m_callback.CallbackSe(this.m_callback.SeTypeDecide);
      }
      
      private function pushKeyCancel() : void
      {
         var _loc1_:int = 0;
         var _loc2_:int = 0;
         _loc1_ = 0;
         if(this.m_flag_change_player)
         {
            return;
         }
         _loc2_ = this.m_callback.GetUserDataInt(ReceiveType_FlagUseCancel);
         if(!_loc2_)
         {
            _loc1_ = this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle);
            if(_loc1_)
            {
               return;
            }
            if(this.m_current_player_index <= PlayerIndexOwn)
            {
               return;
            }
         }
         --this.m_current_select_index;
         this.m_flag_change_player = true;
         this.m_flag_decide = false;
         this.m_callback.CallbackSe(this.m_callback.SeTypeCancel);
      }
      
      private function pushStart() : void
      {
         var _loc1_:int = 0;
         _loc1_ = this.m_callback.GetUserDataInt(ReceiveType_PlayerEnmNum);
         if(_loc1_ > 0)
         {
            return;
         }
         if(this.m_current_player_index == PlayerIndexOwn)
         {
            return;
         }
         this.m_flag_change_player = true;
         this.m_flag_decide = false;
         this.m_flag_exit = true;
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
      }
      
      private function checkKey(param1:KeyboardEvent) : void
      {
         trace("[CHARASELE] checkKey " + param1.keyCode);
         trace("hello");
         if(this.m_flag_decide)
         {
            return;
         }
         if(this.m_flag_change_disp || this.m_count_change_disp > 0)
         {
            return;
         }
         switch(param1.keyCode)
         {
            case Keyboard.ENTER:
               this.pushKeyDecide();
               this.m_flag_change_disp = true;
               break;
            case Keyboard.ESCAPE:
               this.pushKeyCancel();
               this.m_flag_change_disp = true;
               break;
            case Keyboard.LEFT:
               this.pushLeft();
               this.m_flag_change_disp = true;
               break;
            case Keyboard.RIGHT:
               this.pushRight();
               this.m_flag_change_disp = true;
               break;
            case Keyboard.UP:
               this.pushUp();
               this.m_flag_change_disp = true;
               break;
            case Keyboard.DOWN:
               this.pushDown();
               this.m_flag_change_disp = true;
               break;
            case 33:
               this.pushKeyNextPage();
               this.m_flag_change_disp = true;
               break;
            case 45:
               this.pushKeyPrevPage();
               this.m_flag_change_disp = true;
               break;
            case Keyboard.DELETE:
               this.pushKeyL();
               this.m_flag_change_disp = true;
               break;
            case Keyboard.PAGE_DOWN:
               this.pushKeyR();
               this.m_flag_change_disp = true;
               break;
            case 88:
               this.pushKeySkill();
               this.m_flag_change_disp = true;
               break;
            case 89:
               this.pushKeyRandom();
               this.m_flag_change_disp = true;
               break;
            case Keyboard.END:
               if(this.isKeyboardControllerActive)
               {
                  this.pushKeyRandom();
                  this.m_flag_change_disp = true;
               }
               break;
            case Keyboard.SPACE:
            case 83:
               this.pushStart();
               this.m_flag_change_disp = true;
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
      
      public function TestDestroy() : void
      {
         this.m_callback = null;
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.requestUnlock);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitUnlock);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStart);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitMain);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.main);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEnd);
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_timeline.visible = false;
         this.m_timeline = null;
         this.m_timer.Destroy();
         this.m_timer = null;
      }
      
      public function TestCheckChangeSelect() : Boolean
      {
         return !this.m_callback.GetUserDataValidFlag(ReceiveType_CharaNameStr);
      }
      
      public function TestGetCharaList() : Array
      {
         return this.m_chara_list;
      }
      
      public function TestGetCharaVarInfo() : Array
      {
         return this.GetSelectVarInfo(this.m_current_player_index);
      }
      
      public function TestGetCurrentPlayerIndex() : int
      {
         return this.m_current_player_index;
      }
   }
}

