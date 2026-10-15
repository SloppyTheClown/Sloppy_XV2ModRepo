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

      // CharaSearch: the grid's entries to the mod
      // once (string "token\nkey\tcodes" per entry), the cursor's entry
      // every frame (int, -1 = none); the mod answers in user data
      // ReceiveType_CharaSearchState. Both ids are handled by the mod and
      // never reach the game.
      internal static const SendType_CharaSearchList:int = 9902;

      internal static const SendType_CharaSearchPoll:int = 9904;

      // waitStart holds the grid this many frames (0.5 s) at most for the
      // mod's first answer, so it opens already in order.
      internal static const CharaSearchWaitFrames:int = 30;

      internal static const FavoriteStarName:String = "bs_fav_star";

      // The same gold star as the item and style lists.
      internal static const FavoriteStarHtml:String = "<font color=\"#FFC800\">\u2605</font>";

      // The star's font size, in icon units: about a quarter of the tile's
      // frame (128x64, measured with a layout dump of the icon).
      internal static const FavoriteStarFontSize:int = 16;

      // How far in from the frame's left edge the star sits, as a part of the
      // frame's width (the tile is slanted: its left edge at mid height is in
      // from the frame's bounds).
      internal static const FavoriteStarInset:Number = 0.18;
      
      public static const PlayerNumFri:int = 2;
      
      public static const PlayerNumEnm:int = 3;
      
      public static const PlayerMax:int = 1 + PlayerNumFri + PlayerNumEnm;
      
      public static const SkillMax:int = 8;
      
      public static const CharaVarIndexNum:int = 32;
      
      public static const CharacterMax:int = 256;
      
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

      // CharaSearch: the character search's answer
      // "token\tshown masters or *\tfavorite masters" (chara_search.cpp).
      public static const ReceiveType_CharaSearchState:int = ReceiveType_CustomList_PartnerJudge_End;
      
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
      
      private var m_icon_show_wait:Array;
      
      private var m_chara_list:Array;
      
      private var m_chara_list_num:int = 0;
      
      private var m_chara_num_column:int = 0;
      
      private var m_pre_time:int = 99;
      
      private var m_selected_index_list:Array;
      
      private var m_flag_customlist:* = false;

      // CharaSearch (chara_search.as): the list as the
      // game sent it, its shift counts and faces, and the grid's view of it.
      private var m_master_list:Array = null;

      private var m_master_shift:Array = null;

      private var m_master_faces:Array = null;

      private var m_view:Array = null;

      private var m_own_master:int = 0;

      private var m_search_token:String = null;

      private var m_search_state:String = null;

      private var m_search_wait:int = 0;

      private var m_fav:Object = new Object();

      private var m_stars_settled:Boolean = false;
      
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
         }
         catch(e:Error)
         {
            trace("[CHARASELE] ForcingCancel EXCEPTION");
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
         var _loc2_:int = 0;
         var _loc3_:int = 0;
         var _loc4_:int = 0;
         var _loc6_:* = undefined;
         var _loc7_:int = 0;
         var _loc8_:int = 0;
         var _loc9_:int = 0;
         var _loc10_:int = 0;
         var _loc11_:int = 0;
         var _loc12_:int = 0;
         var _loc13_:Array = null;
         var _loc14_:int = 0;
         var _loc15_:String = null;
         var _loc16_:int = 0;
         var _loc17_:int = 0;
         var _loc18_:int = 0;
         var _loc19_:Array = null;
         var _loc20_:Boolean = false;
         var _loc21_:Boolean = false;
         var _loc22_:int = 0;
         var _loc23_:int = 0;
         var _loc24_:int = 0;
         var _loc25_:int = 0;
         var _loc26_:int = 0;
         var _loc27_:int = 0;
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
         var _loc5_:int = this.m_callback.GetUserDataInt(ReceiveType_UseCustomList);
         if(_loc5_)
         {
            this.m_flag_customlist = true;
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_CustomListNum))
            {
               return;
            }
            _loc6_ = this.m_callback.GetUserDataInt(ReceiveType_CustomListNum);
            _loc2_ = 0;
            while(_loc2_ < _loc6_)
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
            _loc7_ = this.m_callback.GetUserDataInt(ReceiveType_DLCUnlockFlag);
            _loc8_ = this.m_callback.GetUserDataInt(ReceiveType_DLCUnlockFlag2);
            _loc9_ = this.m_callback.GetUserDataInt(ReceiveType_CharaSingleUnlock);
            _loc10_ = this.m_callback.GetUserDataInt(ReceiveType_CharaSingleUnlock2);
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_CostumeNum))
            {
               return;
            }
            _loc11_ = this.m_callback.GetUserDataInt(ReceiveType_CostumeNum);
            _loc12_ = ReceiveType_CharacterTableStart;
            while(_loc12_ < ReceiveType_CharacterTableStart + _loc11_ * CharacterTableData)
            {
               if(!this.m_callback.GetUserDataValidFlag(_loc12_))
               {
                  return;
               }
               _loc12_++;
            }
            this.m_chara_list = this.recieveList(_loc7_,_loc8_,_loc9_,_loc10_);
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_CharacterTableEnd + 1))
            {
               return;
            }
            _loc13_ = new Array();
            _loc2_ = 0;
            while(_loc2_ < _loc6_)
            {
               _loc15_ = this.m_callback.GetUserDataString(ReceiveType_CustomList_CID_Start + _loc2_);
               _loc16_ = this.m_callback.GetUserDataInt(ReceiveType_CustomList_MID_Start + _loc2_);
               _loc17_ = this.m_callback.GetUserDataInt(ReceiveType_CustomList_PID_Start + _loc2_);
               _loc18_ = this.m_callback.GetUserDataInt(ReceiveType_CustomList_PartnerJudge_Start + _loc2_);
               _loc13_.push([_loc15_,_loc16_,_loc17_,_loc18_]);
               _loc2_++;
            }
            _loc2_ = 0;
            while(_loc2_ < this.m_chara_list.length)
            {
               _loc19_ = this.m_chara_list[_loc2_];
               _loc20_ = true;
               _loc3_ = 0;
               while(_loc3_ < _loc19_.length)
               {
                  _loc21_ = true;
                  _loc4_ = 0;
                  while(_loc4_ < _loc13_.length)
                  {
                     if(_loc19_[_loc3_][0] == _loc13_[_loc4_][0])
                     {
                        if(_loc13_[_loc4_][1] == -1 || _loc19_[_loc3_][1] == _loc13_[_loc4_][1])
                        {
                           if(_loc13_[_loc4_][2] == -1 || _loc19_[_loc3_][2] == _loc13_[_loc4_][2])
                           {
                              if(!(_loc13_[_loc4_][3] != 1 && Boolean(_loc19_[_loc3_][VarTypeCustomCostume])))
                              {
                                 _loc21_ = false;
                                 _loc20_ = false;
                              }
                           }
                        }
                     }
                     _loc4_++;
                  }
                  if(_loc21_)
                  {
                     _loc19_.splice(_loc3_,1);
                     _loc3_--;
                  }
                  _loc3_++;
               }
               if(_loc20_)
               {
                  this.m_chara_list.splice(_loc2_,1);
                  _loc2_--;
               }
               _loc2_++;
            }
            _loc14_ = 3 - this.m_chara_list.length % 3;
            _loc2_ = 0;
            while(_loc14_ > _loc2_)
            {
               this.m_chara_list.concat([[["",0,0,0]]]);
               _loc2_++;
            }
         }
         else
         {
            _loc22_ = this.m_callback.GetUserDataInt(ReceiveType_DLCUnlockFlag);
            _loc23_ = this.m_callback.GetUserDataInt(ReceiveType_DLCUnlockFlag2);
            _loc24_ = this.m_callback.GetUserDataInt(ReceiveType_CharaSingleUnlock);
            _loc25_ = this.m_callback.GetUserDataInt(ReceiveType_CharaSingleUnlock2);
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_CostumeNum))
            {
               return;
            }
            _loc26_ = this.m_callback.GetUserDataInt(ReceiveType_CostumeNum);
            _loc27_ = ReceiveType_CharacterTableStart;
            while(_loc27_ < ReceiveType_CharacterTableStart + _loc26_ * CharacterTableData)
            {
               if(!this.m_callback.GetUserDataValidFlag(_loc27_))
               {
                  return;
               }
               _loc27_++;
            }
            this.m_chara_list = this.recieveList(_loc22_,_loc23_,_loc24_,_loc25_);
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
         var _loc7_:int = 0;
         var _loc11_:int = 0;
         var _loc12_:Array = null;
         var _loc13_:Array = null;
         var _loc14_:String = null;
         var _loc15_:int = 0;
         var _loc16_:int = 0;
         var _loc17_:int = 0;
         var _loc18_:int = 0;
         var _loc19_:Array = null;
         var _loc20_:int = 0;
         var _loc21_:int = 0;
         var _loc22_:int = 0;
         var _loc23_:int = 0;
         var _loc24_:int = 0;
         var _loc25_:int = 0;
         var _loc26_:int = 0;
         var _loc27_:int = 0;
         var _loc28_:int = 0;
         var _loc29_:int = 0;
         var _loc30_:int = 0;
         var _loc31_:Array = null;
         var _loc32_:Boolean = false;
         var _loc33_:int = 0;
         var _loc34_:int = 0;
         var _loc35_:int = 0;
         var _loc5_:Array = new Array();
         var _loc6_:int = this.m_callback.GetUserDataInt(ReceiveType_CostumeNum);
         _loc7_ = 0;
         while(_loc7_ < _loc6_)
         {
            _loc12_ = new Array();
            while(true)
            {
               _loc13_ = new Array();
               _loc14_ = this.m_callback.GetUserDataString(ReceiveType_CID + _loc7_ * CharacterTableData);
               _loc15_ = this.m_callback.GetUserDataInt(ReceiveType_MID + _loc7_ * CharacterTableData);
               _loc16_ = this.m_callback.GetUserDataInt(ReceiveType_PID + _loc7_ * CharacterTableData);
               _loc17_ = this.m_callback.GetUserDataInt(ReceiveType_UnlockNum + _loc7_ * CharacterTableData);
               _loc18_ = this.m_callback.GetUserDataInt(ReceiveType_Gokuaku + _loc7_ * CharacterTableData);
               _loc19_ = new Array();
               _loc19_.push(this.m_callback.GetUserDataInt(ReceiveType_SelectVoice1 + _loc7_ * CharacterTableData));
               _loc19_.push(this.m_callback.GetUserDataInt(ReceiveType_SelectVoice2 + _loc7_ * CharacterTableData));
               _loc20_ = this.m_callback.GetUserDataInt(ReceiveType_DlcKey + _loc7_ * CharacterTableData);
               _loc21_ = this.m_callback.GetUserDataInt(ReceiveType_DlcKey2 + _loc7_ * CharacterTableData);
               _loc22_ = this.m_callback.GetUserDataInt(ReceiveType_CustomCostume + _loc7_ * CharacterTableData);
               _loc23_ = this.m_callback.GetUserDataInt(ReceiveType_AvatarSlotID + _loc7_ * CharacterTableData);
               _loc24_ = this.m_callback.GetUserDataInt(ReceiveType_AfterTU9Order + _loc7_ * CharacterTableData);
               _loc25_ = this.m_callback.GetUserDataInt(ReceiveType_CustomCostumeEx + _loc7_ * CharacterTableData);
               _loc26_ = this.m_callback.GetUserDataInt(ReceiveType_ChouGokuaku + _loc7_ * CharacterTableData);
               _loc27_ = this.m_callback.GetUserDataInt(ReceiveType_CharaSingleUnlockKey + _loc7_ * CharacterTableData);
               _loc28_ = this.m_callback.GetUserDataInt(ReceiveType_CharaSingleUnlockKey2 + _loc7_ * CharacterTableData);
               _loc29_ = this.m_callback.GetUserDataInt(ReceiveType_Kokuhuku + _loc7_ * CharacterTableData);
               _loc13_.push(_loc14_);
               _loc13_.push(_loc15_);
               _loc13_.push(_loc16_);
               _loc13_.push(_loc17_);
               _loc13_.push(_loc18_);
               _loc13_.push(_loc19_);
               _loc13_.push(_loc20_);
               _loc13_.push(_loc21_);
               _loc13_.push(_loc22_);
               _loc13_.push(_loc23_);
               _loc13_.push(_loc24_);
               _loc13_.push(_loc25_);
               _loc13_.push(_loc26_);
               _loc13_.push(_loc27_);
               _loc13_.push(_loc28_);
               _loc13_.push(_loc29_);
               _loc12_.push(_loc13_);
               _loc30_ = this.m_callback.GetUserDataInt(ReceiveType_CostumeID + (_loc7_ + 1) * CharacterTableData);
               if(_loc30_ == 0)
               {
                  break;
               }
               _loc7_++;
            }
            _loc5_.push(_loc12_);
            _loc7_++;
         }
         var _loc8_:int = 0;
         var _loc9_:int = 0;
         _loc7_ = 0;
         while(_loc7_ < _loc5_.length)
         {
            _loc31_ = _loc5_[_loc7_];
            _loc32_ = true;
            _loc33_ = 0;
            while(_loc33_ < _loc31_.length)
            {
               if((param1 & _loc31_[_loc33_][VarTypeDlcKeyFlag]) == 0 && (param2 & _loc31_[_loc33_][VarTypeDlcKeyFlag2]) == 0 && (param3 & _loc31_[_loc33_][VarTypeCharaSingleUnlockFlag]) == 0 && (param4 & _loc31_[_loc33_][VarTypeCharaSingleUnlockFlag2]) == 0)
               {
                  trace("delete chara:" + _loc7_ + "," + _loc33_ + "," + _loc31_[_loc33_][VarTypeCode]);
                  _loc31_.splice(_loc33_,1);
                  _loc33_--;
               }
               else
               {
                  _loc32_ = false;
               }
               _loc33_++;
            }
            if(_loc32_)
            {
               _loc5_.splice(_loc7_,1);
               _loc7_--;
            }
            else
            {
               _loc34_ = _loc9_;
               _loc35_ = int(_loc31_[0][VarTypeAfterTU9Order]);
               if(_loc35_ == -1)
               {
                  _loc34_ = _loc9_;
               }
               else
               {
                  _loc34_ = _loc35_ + 122;
                  _loc9_--;
               }
               CharaListShiftCount.push(_loc34_);
            }
            _loc7_++;
            _loc9_++;
         }
         var _loc10_:int = 3 - _loc5_.length % 3;
         _loc7_ = 0;
         while(_loc10_ > _loc7_)
         {
            _loc5_.concat([[[InvalidCode,0,0,0]]]);
            _loc7_++;
         }
         return _loc5_;
      }
      
      private function requestUnlock(param1:Event) : void
      {
         var _loc4_:Boolean = false;
         var _loc5_:Array = null;
         var _loc6_:Boolean = false;
         var _loc7_:int = 0;
         var _loc8_:Array = null;
         var _loc9_:String = null;
         var _loc10_:int = 0;
         var _loc11_:int = 0;
         var _loc12_:int = 0;
         var _loc13_:int = 0;
         var _loc14_:int = 0;
         var _loc2_:int = 0;
         var _loc3_:* = 0;
         _loc2_ = 0;
         while(CharacterMax > _loc2_)
         {
            _loc4_ = false;
            _loc5_ = this.getCharaInfo(_loc2_);
            if(!_loc5_)
            {
               _loc3_ = 0;
               while(CharaVarIndexNum > _loc3_)
               {
                  this.SetUserDataInt(ReceiveType_UnlockVarStart + _loc2_ * CharaVarIndexNum + _loc3_,0);
                  _loc3_++;
               }
               this.SetUserDataString(ReceiveType_ImageStrStart + _loc2_,"");
            }
            else if(_loc5_.length <= _loc3_)
            {
               _loc3_ = 0;
               while(CharaVarIndexNum > _loc3_)
               {
                  this.SetUserDataInt(ReceiveType_UnlockVarStart + _loc2_ * CharaVarIndexNum + _loc3_,0);
                  _loc3_++;
               }
               this.SetUserDataString(ReceiveType_ImageStrStart + _loc2_,"");
            }
            else
            {
               _loc6_ = false;
               _loc3_ = int(CharaVarIndexNum - 1);
               while(0 <= _loc3_)
               {
                  _loc7_ = _loc2_ * CharaVarIndexNum + _loc3_;
                  if(_loc5_.length <= _loc3_)
                  {
                     this.SetUserDataInt(ReceiveType_UnlockVarStart + _loc7_,0);
                  }
                  else
                  {
                     _loc8_ = _loc5_[_loc3_];
                     _loc9_ = _loc8_[VarTypeCode];
                     if(_loc9_ == InvalidCode)
                     {
                        this.SetUserDataInt(ReceiveType_UnlockVarStart + _loc7_,0);
                     }
                     else
                     {
                        _loc10_ = int(_loc8_[VarTypeMid]);
                        _loc4_ = true;
                        if(!this.m_callback.GetUserDataValidFlag(ReceiveType_UnlockVarStart + _loc7_))
                        {
                           _loc11_ = int(_loc8_[VarTypeUnlockIndex]);
                           _loc12_ = int(_loc8_[VarTypeAvatarSlotID]);
                           _loc13_ = int(_loc8_[VarTypeCustomCostume]);
                           _loc14_ = _loc3_;
                           this.m_callback.CallbackUserData("user",SendType_AvatarSlotID,_loc12_);
                           this.m_callback.CallbackUserDataString("user",SendType_SelectCode,_loc9_);
                           this.m_callback.CallbackUserData("user",SendType_SelectUnlockIndex,_loc11_);
                           this.m_callback.CallbackUserData("user",SendType_SelectVariation,_loc14_);
                           this.m_callback.CallbackUserData("user",SendType_SelectCustomFlag,_loc13_);
                           this.m_callback.CallbackUserData("user",SendType_SelectMid,_loc10_);
                           this.m_callback.CallbackUserData("user",SendType_RequestUnlock,_loc7_);
                           this.m_callback.CallbackUserData("user",SendType_RequestImageStr,_loc2_);
                           this.m_callback.CallbackUserData("user",SendType_RequestNewInfo,CharaListShiftCount[_loc2_]);
                        }
                     }
                  }
                  _loc3_--;
               }
               if(!_loc4_)
               {
                  this.SetUserDataString(ReceiveType_ImageStrStart + _loc2_,"");
               }
            }
            _loc2_++;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.requestUnlock);
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitUnlock);
      }
      
      private function waitUnlock(param1:Event) : void
      {
         var _loc5_:int = 0;
         var _loc6_:int = 0;
         var _loc7_:int = 0;
         var _loc2_:int = 0;
         var _loc3_:int = 0;
         _loc3_ = 0;
         while(this.m_select_info.length > _loc3_)
         {
            _loc5_ = -1;
            _loc2_ = 0;
            for(; CharacterMax > _loc2_; _loc2_++)
            {
               _loc6_ = PlayerIndexOwn + _loc2_;
               _loc6_ = _loc6_ % CharacterMax;
               if(!this.m_callback.GetUserDataValidFlag(ReceiveType_FlagSelectAvatar))
               {
                  return;
               }
               _loc7_ = this.m_callback.GetUserDataInt(ReceiveType_FlagSelectAvatar);
               if(this.m_flag_customlist)
               {
                  _loc7_ = 1;
                  _loc5_ = 0;
               }
               if(_loc3_ != PlayerIndexOwn)
               {
                  if(!_loc7_)
                  {
                     if(this.checkAvatar(_loc6_))
                     {
                        continue;
                     }
                  }
               }
               if(this.checkUnlockChara(_loc6_))
               {
                  _loc5_ = _loc6_;
                  break;
               }
            }
            if(_loc5_ < 0)
            {
               return;
            }
            this.m_select_row = this.calcIconIndexRow(_loc5_);
            this.m_select_column = this.calcIconIndexColumn(_loc5_);
            this.m_current_player_index = _loc3_;
            this.m_select_var = 0;
            this.setSelectChara();
            _loc3_++;
         }
         this.m_current_player_index = PlayerIndexOwn;
         var _loc4_:* = this.m_select_info[this.m_current_player_index][SelectInfoTypeListIndex];
         this.m_select_row = this.calcIconIndexRow(_loc4_);
         this.m_select_column = this.calcIconIndexColumn(_loc4_);
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
         if(!this.startCharaSearch())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStart);
         this.startMain();
         this.keepMasterFaces();
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
         var _loc25_:int = 0;
         var _loc26_:MovieClip = null;
         var _loc27_:int = 0;
         var _loc28_:MovieClip = null;
         var _loc29_:MovieClip = null;
         var _loc30_:Array = null;
         var _loc31_:Bitmap = null;
         var _loc32_:String = null;
         var _loc33_:String = null;
         var _loc34_:String = null;
         var _loc35_:Bitmap = null;
         var _loc36_:MovieClip = null;
         var _loc37_:int = 0;
         var _loc38_:MovieClip = null;
         var _loc1_:int = 0;
         this.m_timeline.visible = true;
         this.m_timeline.cha_frame.cmn_CMN_M_frame.visible = true;
         this.m_timeline.cha_skill.visible = false;
         this.m_timeline.cha_select.visible = true;
         this.m_timeline.cha_parameter.visible = true;
         this.m_timeline.cha_arrow.visible = true;
         var _loc2_:* = this.m_timeline.cha_select;
         _loc2_.chara_icn_set00.visible = false;
         var _loc3_:int = (this.m_chara_list_num - 1) / IndexNumRow + 1;
         var _loc4_:int = 1;
         while(_loc4_ < 9)
         {
            if(_loc4_ <= _loc3_)
            {
               _loc2_["chara_icn_set0" + _loc4_].visible = true;
            }
            else
            {
               _loc2_["chara_icn_set0" + _loc4_].visible = false;
            }
            _loc4_++;
         }
         var _loc5_:int = (this.m_chara_list_num - 1) % IndexNumRow + 1;
         _loc3_ = Math.min(_loc3_,8);
         var _loc6_:* = _loc2_["chara_icn_set0" + _loc3_];
         var _loc7_:int = 1;
         while(_loc7_ < 4)
         {
            if(_loc7_ <= _loc5_)
            {
               _loc6_["nest_charaselect0" + _loc7_].visible = true;
            }
            else
            {
               _loc6_["nest_charaselect0" + _loc7_].visible = false;
            }
            _loc7_++;
         }
         var _loc8_:int = this.m_callback.GetUserDataInt(ReceiveType_PlayerFriNum);
         var _loc9_:int = this.m_callback.GetUserDataInt(ReceiveType_PlayerEnmNum);
         if(_loc8_ == 0 && _loc9_ == 0)
         {
            this.m_timeline.cha_parameter.nest_ready.visible = false;
            this.m_timeline.cha_parameter.ready_base.visible = false;
            this.m_timeline.cha_parameter.ready_base.vs.visible = false;
         }
         else
         {
            this.m_timeline.cha_parameter.nest_ready.visible = true;
            this.m_timeline.cha_parameter.ready_base.visible = true;
            if(_loc9_ > 0)
            {
               this.m_timeline.cha_parameter.ready_base.vs.visible = true;
            }
            else
            {
               this.m_timeline.cha_parameter.ready_base.vs.visible = false;
            }
         }
         var _loc10_:int = this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle);
         if(_loc10_)
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
         var _loc11_:MovieClip = this.getReadyIconMc(0);
         _loc11_.visible = true;
         _loc1_ = 0;
         while(PlayerNumFri > _loc1_)
         {
            _loc25_ = PlayerIndexFriStart + _loc1_;
            _loc26_ = this.getReadyIconMc(_loc25_);
            if(_loc1_ < _loc8_)
            {
               _loc26_.visible = true;
            }
            else
            {
               _loc26_.visible = false;
            }
            _loc1_++;
         }
         _loc1_ = 0;
         while(PlayerNumEnm > _loc1_)
         {
            _loc27_ = PlayerIndexEnmStart + _loc1_;
            _loc28_ = this.getReadyIconMc(_loc27_);
            if(_loc1_ < _loc9_)
            {
               _loc28_.visible = true;
            }
            else
            {
               _loc28_.visible = false;
            }
            _loc1_++;
         }
         this.m_timeline.cha_parameter.sys_charaName.sys_charaName.htmlText = "";
         if(this.m_timeline.cha_parameter.sys_skill_header)
         {
            this.m_timeline.cha_parameter.sys_skill_header.sys_skill_header.htmlText = "";
         }
         this.setSelectChara();
         var _loc12_:MovieClip = this.m_timeline.cha_new;
         _loc1_ = 0;
         while(IndexNumCharaNewIcon > _loc1_)
         {
            _loc29_ = _loc12_["chara_new_" + _loc1_];
            if(!this.checkCharaNewIconVisible(_loc1_))
            {
               _loc29_.visible = false;
            }
            _loc1_++;
         }
         this.m_chara_face = new Array(this.m_chara_list_num);
         _loc1_ = 0;
         while(this.m_chara_list_num > _loc1_)
         {
            _loc30_ = this.getCharaInfo(_loc1_);
            _loc31_ = null;
            if(Boolean(_loc30_) && _loc30_.length > 0)
            {
               _loc32_ = _loc30_[0][VarTypeCode];
               _loc33_ = this.m_callback.GetUserDataString(ReceiveType_ImageStrStart + this.masterIndex(_loc1_));
               if(_loc32_ != InvalidCode)
               {
                  _loc31_ = new Bitmap(null);
                  IggyFunctions.setTextureForBitmap(_loc31_,_loc33_);
                  _loc31_.scaleX = 256 / _loc31_.width;
                  _loc31_.scaleY = 128 / _loc31_.height;
               }
            }
            this.m_chara_face[_loc1_] = _loc31_;
            _loc1_++;
         }
         this.m_chara_face_npc = new Array(PlayerNumFri);
         _loc1_ = 0;
         while(PlayerNumFri > _loc1_)
         {
            _loc34_ = this.m_callback.GetUserDataString(ReceiveType_ImageStrNpcStart + _loc1_);
            _loc35_ = new Bitmap(null);
            IggyFunctions.setTextureForBitmap(_loc35_,_loc34_);
            _loc35_.scaleX = 256 / _loc35_.width;
            _loc35_.scaleY = 128 / _loc35_.height;
            this.m_chara_face_npc[_loc1_] = _loc35_;
            _loc1_++;
         }
         var _loc13_:MovieClip = this.m_timeline.cha_select_cur;
         _loc13_.icn_lock.visible = false;
         this.ResetIcons();
         var _loc14_:int = this.m_callback.GetUserDataInt(ReceiveType_JoyConSingleFlag);
         var _loc15_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrL2);
         var _loc16_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrR2);
         var _loc17_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrRU);
         var _loc18_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrRD);
         var _loc19_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrRL);
         var _loc20_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrRR);
         if(_loc14_ == 1)
         {
            _loc15_ = this.m_callback.GetUserDataString(ReceiveType_KeyStrSingleLS);
            _loc16_ = this.m_callback.GetUserDataString(ReceiveType_KeyStrSingleRS);
            _loc17_ = this.m_callback.GetUserDataString(ReceiveType_KeyStrSingleU);
            _loc18_ = this.m_callback.GetUserDataString(ReceiveType_KeyStrSingleD);
            _loc19_ = this.m_callback.GetUserDataString(ReceiveType_KeyStrSingleL);
            _loc20_ = this.m_callback.GetUserDataString(ReceiveType_KeyStrSingleR);
         }
         this.m_timeline.cha_skill.inact_skill.skill01.sys_com01.htmlText = _loc16_;
         this.m_timeline.cha_skill.inact_skill.skill01.sys_com02.htmlText = _loc17_;
         this.m_timeline.cha_skill.inact_skill.skill01.sys_com03.htmlText = "";
         this.m_timeline.cha_skill.inact_skill.skill01.sys_01.visible = false;
         this.m_timeline.cha_skill.inact_skill.skill02.sys_com01.htmlText = _loc16_;
         this.m_timeline.cha_skill.inact_skill.skill02.sys_com02.htmlText = _loc19_;
         this.m_timeline.cha_skill.inact_skill.skill02.sys_com03.htmlText = "";
         this.m_timeline.cha_skill.inact_skill.skill02.sys_01.visible = false;
         this.m_timeline.cha_skill.inact_skill.skill03.sys_com01.htmlText = _loc16_;
         this.m_timeline.cha_skill.inact_skill.skill03.sys_com02.htmlText = _loc20_;
         this.m_timeline.cha_skill.inact_skill.skill03.sys_com03.htmlText = "";
         this.m_timeline.cha_skill.inact_skill.skill03.sys_01.visible = false;
         this.m_timeline.cha_skill.inact_skill.skill04.sys_com01.htmlText = _loc16_;
         this.m_timeline.cha_skill.inact_skill.skill04.sys_com02.htmlText = _loc18_;
         this.m_timeline.cha_skill.inact_skill.skill04.sys_com03.htmlText = "";
         this.m_timeline.cha_skill.inact_skill.skill04.sys_01.visible = false;
         this.m_timeline.cha_skill.inact_skill.skill05.sys_com01.htmlText = _loc15_;
         this.m_timeline.cha_skill.inact_skill.skill05.sys_com02.htmlText = _loc16_;
         this.m_timeline.cha_skill.inact_skill.skill05.sys_com03.htmlText = _loc17_;
         this.m_timeline.cha_skill.inact_skill.skill06.sys_com01.htmlText = _loc15_;
         this.m_timeline.cha_skill.inact_skill.skill06.sys_com02.htmlText = _loc16_;
         this.m_timeline.cha_skill.inact_skill.skill06.sys_com03.htmlText = _loc19_;
         this.m_timeline.cha_skill.inact_skill.skill07.sys_com01.htmlText = _loc15_;
         this.m_timeline.cha_skill.inact_skill.skill07.sys_com02.htmlText = _loc16_;
         this.m_timeline.cha_skill.inact_skill.skill07.sys_com03.htmlText = _loc18_;
         this.m_timeline.cha_skill.inact_skill.skill08.sys_com01.htmlText = _loc15_;
         this.m_timeline.cha_skill.inact_skill.skill08.sys_com02.htmlText = _loc16_;
         this.m_timeline.cha_skill.inact_skill.skill08.sys_com03.htmlText = _loc20_;
         var _loc21_:String = this.m_callback.GetUserDataString(ReceiveType_TarismanHeaderStr);
         this.m_timeline.cha_skill.inact_skill.skill09.sys_com01.htmlText = _loc21_;
         this.m_timeline.cha_parameter.nest_clothes.sys_clothes.htmlText = "";
         var _loc22_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrL1);
         var _loc23_:String = this.m_callback.GetUserDataString(ReceiveType_KeyStrR1);
         if(_loc14_)
         {
            _loc22_ = _loc15_;
            _loc23_ = _loc16_;
         }
         this.m_timeline.cha_parameter.nest_clothes.sys_l.htmlText = _loc22_;
         this.m_timeline.cha_parameter.nest_clothes.sys_r.htmlText = _loc23_;
         _loc1_ = 0;
         while(PlayerMax > _loc1_)
         {
            if(_loc1_ == this.m_current_player_index)
            {
               this.setReadyIcon(_loc1_,true,false);
            }
            else
            {
               _loc37_ = this.m_callback.GetUserDataInt(ReceiveType_PartyNpcNum);
               if(0 <= _loc1_ - 1 && _loc1_ - 1 < _loc37_)
               {
                  this.setReadyIcon(_loc1_,false,true);
               }
               else
               {
                  this.setReadyIcon(_loc1_,false,false);
               }
            }
            _loc36_ = this.getReadyIconMc(_loc1_);
            if(PlayerIndexOwn == _loc1_)
            {
               _loc36_.cmn_icn_you.visible = true;
            }
            else
            {
               _loc36_.cmn_icn_you.visible = false;
            }
            _loc1_++;
         }
         _loc1_ = 0;
         while(IndexNumCharaNewIcon > _loc1_)
         {
            _loc38_ = this.getMcChamysetNewIcon(_loc1_);
            _loc38_.visible = false;
            _loc1_++;
         }
         this.updateVariation();
         var _loc24_:int = this.m_callback.GetUserDataInt(ReceiveType_Time);
         if(_loc24_ <= 0)
         {
            this.m_timeline.timer.visible = false;
         }
         else
         {
            this.m_timeline.timer.visible = true;
            this.m_timer.Start(_loc24_,this.cbFuncEndTimer);
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
         this.pollCharaSearch();
         var _loc12_:int = 0;
         var _loc13_:* = undefined;
         var _loc14_:Array = null;
         var _loc15_:int = 0;
         var _loc16_:int = 0;
         var _loc17_:int = 0;
         var _loc2_:int = 0;
         var _loc3_:int = int(this.m_select_info[this.m_current_player_index][SelectInfoTypeListIndex]);
         var _loc4_:Array = this.getCharaInfo(_loc3_);
         var _loc5_:int = this.m_callback.GetUserDataInt(ReceiveType_Time);
         if(_loc5_ > 0)
         {
            if(this.m_timer)
            {
               _loc12_ = this.m_timer.GetTime();
               if(this.m_pre_time != _loc12_)
               {
                  if(_loc12_ < 10)
                  {
                     _loc13_ = 8;
                     this.m_callback.CallbackUserData("user",SendType_RequestCountDownSe,_loc13_);
                  }
                  this.m_pre_time = _loc12_;
               }
               if(_loc12_ <= _loc5_)
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
               _loc2_ = 0;
               while(_loc2_ < this.m_icon_show_wait.length)
               {
                  this.m_icon_show_wait[_loc2_] = 0;
                  _loc2_++;
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
         var _loc6_:MovieClip = this.m_timeline.cha_select;
         var _loc7_:MovieClip = _loc6_["chara_icn_set0" + (this.m_select_icon_column + 1)];
         var _loc8_:MovieClip = _loc7_["nest_charaselect0" + (this.m_select_icon_row + 1)];
         if(!this.m_flag_change_player)
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         var _loc9_:int = 0;
         if(this.m_flag_decide)
         {
            _loc9_ = this.m_current_select_index - 1;
            if(0 <= _loc9_ && _loc9_ < this.m_selected_index_list.length)
            {
               this.m_selected_index_list[_loc9_] = _loc3_;
            }
         }
         else
         {
            _loc9_ = this.m_current_select_index;
            if(0 <= _loc9_ && _loc9_ < this.m_selected_index_list.length)
            {
               this.m_selected_index_list[_loc9_] = -1;
            }
         }
         if(this.m_flag_decide)
         {
            if(this.m_timeline.cha_parameter.currentLabel == "start" || this.m_timeline.cha_parameter.currentLabel == "wait")
            {
               this.m_timeline.cha_parameter.gotoAndPlay("push");
               if(_loc4_)
               {
                  _loc14_ = _loc4_[this.m_select_var][VarTypeVoiceIdList];
                  _loc15_ = Math.floor(Math.random() * _loc14_.length);
                  _loc16_ = int(_loc14_[_loc15_]);
                  this.m_callback.CallbackUserData("user",SendType_RequestPlayVoice,_loc16_);
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
            var _loc10_:Boolean = false;
            if(this.m_current_player_index < 0 || PlayerMax <= this.m_current_player_index)
            {
               _loc10_ = true;
            }
            if(_loc10_)
            {
               this.end();
               return;
            }
            var _loc11_:int = this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle);
            if(!_loc11_)
            {
               if(_loc5_ > 0)
               {
                  this.m_timer.Start(_loc5_ + 1,this.cbFuncEndTimer);
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
            _loc17_ = -1;
            if(this.m_timer)
            {
               _loc17_ = this.m_timer.GetTime();
            }
            this.m_callback.CallbackUserData("user",SendType_RequestDecide,_loc17_);
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
         var _loc3_:int = 0;
         var _loc4_:int = 0;
         var _loc5_:int = 0;
         var _loc9_:MovieClip = null;
         var _loc1_:Boolean = false;
         var _loc2_:Boolean = false;
         if(this.m_select_column < this.m_select_column_start)
         {
            this.m_select_column_start = this.m_select_column;
            _loc1_ = true;
            _loc2_ = true;
         }
         else if(this.m_select_column_start + IndexNumColumn <= this.m_select_column)
         {
            this.m_select_column_start = this.m_select_column - IndexNumColumn + 1;
            _loc1_ = true;
         }
         if(!_loc1_)
         {
            return;
         }
         this.m_flag_change_page = true;
         var _loc6_:MovieClip = this.m_timeline.cha_select;
         var _loc7_:MovieClip = this.m_timeline.cha_new;
         var _loc8_:MovieClip = null;
         _loc4_ = -1;
         while(IndexNumColumn + 1 > _loc4_)
         {
            _loc9_ = _loc6_["chara_icn_set0" + (_loc4_ + 1)];
            if(_loc2_)
            {
               _loc9_.gotoAndPlay("push_l");
               this.m_timeline.cha_arrow.spinbtn_l.cmn_CMN_M_B_mc_spinbtn_l.gotoAndPlay("push");
               _loc5_ = 0;
               while(IndexNumCharaNewIcon > _loc5_)
               {
                  _loc8_ = _loc7_["chara_new_" + _loc5_];
                  _loc8_.gotoAndPlay("push_l");
                  _loc5_++;
               }
            }
            else
            {
               _loc9_.gotoAndPlay("push_r");
               this.m_timeline.cha_arrow.spinbtn_r.cmn_CMN_M_B_mc_spinbtn_r.gotoAndPlay("push");
               _loc5_ = 0;
               while(IndexNumCharaNewIcon > _loc5_)
               {
                  _loc8_ = _loc7_["chara_new_" + _loc5_];
                  _loc8_.gotoAndPlay("push_r");
                  _loc5_++;
               }
            }
            _loc4_++;
         }
      }
      
      private function changeLR(param1:Boolean) : void
      {
         var _loc2_:int = 0;
         var _loc3_:int = 0;
         var _loc4_:Boolean = false;
         while(true)
         {
            if(param1)
            {
               if(this.m_select_column == 0 && this.m_select_row == 0)
               {
                  break;
               }
               --this.m_select_column;
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
               ++this.m_select_column;
               if(this.m_chara_num_column <= this.m_select_column)
               {
                  this.m_select_column = 0;
                  ++this.m_select_row;
                  this.m_select_row += IndexNumRow;
                  this.m_select_row %= IndexNumRow;
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
         var _loc4_:Boolean = false;
         var _loc2_:int = this.calcCharaListIndex(this.m_select_row,this.m_select_column);
         var _loc3_:Array = this.getCharaInfo(_loc2_);
         if(!_loc3_)
         {
            return;
         }
         while(true)
         {
            if(param1)
            {
               --this.m_select_var;
            }
            else
            {
               ++this.m_select_var;
            }
            this.m_select_var += _loc3_.length;
            this.m_select_var %= _loc3_.length;
            _loc4_ = this.checkUnlockVar(_loc2_,this.m_select_var);
            if(_loc4_)
            {
               break;
            }
         }
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
         var _loc5_:int = 0;
         var _loc6_:Boolean = false;
         var _loc7_:int = 0;
         var _loc8_:int = 0;
         var _loc9_:int = 0;
         var _loc10_:int = 0;
         if(param1 < 0 || PlayerMax <= param1)
         {
            return;
         }
         var _loc4_:MovieClip = this.getReadyIconMc(param1);
         if(param3)
         {
            _loc5_ = this.checkPlayerTeamType(param1);
            _loc6_ = false;
            _loc7_ = param1 - 1;
            switch(_loc5_)
            {
               case PlayerTeamTypeOwn:
                  _loc4_.gotoAndStop("blue_team");
                  break;
               case PlayerTeamTypeFri:
                  _loc4_.gotoAndStop("blue_team");
                  _loc8_ = this.m_callback.GetUserDataInt(ReceiveType_PlayerFriNum);
                  _loc9_ = this.m_callback.GetUserDataInt(ReceiveType_PartyNpcNum);
                  if(0 <= _loc7_ && _loc7_ < _loc9_)
                  {
                     _loc6_ = true;
                  }
                  break;
               case PlayerTeamTypeEnm:
                  _loc4_.gotoAndStop("red_team");
            }
            _loc4_.sys_ready.text = "OK";
            if(_loc6_)
            {
               this.setImageFriendNpc(_loc4_.icn_chara_lit.chara_img,_loc7_);
            }
            else
            {
               _loc10_ = this.calcCharaListIndex(this.m_select_row,this.m_select_column);
               this.setImage(_loc4_.icn_chara_lit.chara_img,_loc10_,true);
            }
         }
         else
         {
            _loc4_.gotoAndStop("off");
            _loc4_.sys_ready.text = "---";
         }
         if(param2)
         {
            _loc4_.btnact_ready.gotoAndPlay("on");
         }
         else
         {
            _loc4_.btnact_ready.gotoAndPlay("off");
         }
      }
      
      private function changeCurrentPlayer(param1:Boolean) : void
      {
         var _loc5_:Boolean = false;
         var _loc6_:int = 0;
         var _loc7_:int = 0;
         var _loc8_:int = 0;
         var _loc9_:int = 0;
         var _loc10_:int = 0;
         var _loc2_:int = 0;
         var decidedAt:int = this.decidedPick(param1);
         this.setReadyIcon(this.m_current_player_index,false,param1);
         var _loc3_:int = this.m_callback.GetUserDataInt(ReceiveType_PlayerFriNum);
         var _loc4_:int = this.m_callback.GetUserDataInt(ReceiveType_PlayerEnmNum);
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
            _loc5_ = false;
            _loc6_ = this.checkPlayerTeamType(this.m_current_player_index);
            switch(_loc6_)
            {
               case PlayerTeamTypeFri:
                  _loc7_ = this.m_current_player_index - PlayerIndexFriStart;
                  _loc8_ = this.m_callback.GetUserDataInt(ReceiveType_PartyNpcNum);
                  if(_loc8_ > _loc7_)
                  {
                     _loc5_ = false;
                  }
                  else if(_loc3_ > _loc7_)
                  {
                     _loc5_ = true;
                  }
                  break;
               case PlayerTeamTypeEnm:
                  _loc9_ = this.m_current_player_index - PlayerIndexEnmStart;
                  if(_loc4_ > _loc9_)
                  {
                     _loc5_ = true;
                  }
                  break;
               case PlayerTeamTypeOwn:
               default:
                  _loc5_ = true;
            }
         }
         while(!_loc5_);
         this.startAfterDecided(decidedAt);
         this.setReadyIcon(this.m_current_player_index,true,false);
         if(0 <= this.m_current_player_index && this.m_current_player_index < PlayerMax)
         {
            _loc10_ = int(this.m_select_info[this.m_current_player_index][SelectInfoTypeListIndex]);
            this.m_select_row = this.calcIconIndexRow(_loc10_);
            this.m_select_column = this.calcIconIndexColumn(_loc10_);
            this.m_select_var = this.m_select_info[this.m_current_player_index][SelectInfoTypeVarIndex];
            if(this.checkAvatar(_loc10_) && this.checkSelectedAvatar(_loc10_))
            {
               this.changeUD(false);
            }
         }
      }
      
      private function setSelectChara() : void
      {
         var _loc1_:int = 0;
         var _loc2_:int = 0;
         var _loc13_:int = 0;
         var _loc14_:MovieClip = null;
         var _loc15_:MovieClip = null;
         var _loc16_:MovieClip = null;
         var _loc17_:Boolean = false;
         var _loc18_:Boolean = false;
         var _loc19_:Boolean = false;
         var _loc20_:int = 0;
         var _loc21_:int = 0;
         var _loc22_:int = 0;
         if(this.m_current_player_index < 0 || PlayerMax <= this.m_current_player_index)
         {
            return;
         }
         var _loc3_:int = this.calcCharaListIndex(this.m_select_row,this.m_select_column);
         var _loc4_:Array = this.getCharaInfo(_loc3_);
         var _loc5_:int = -1;
         if(_loc4_)
         {
            _loc5_ = int(_loc4_.length);
            _loc1_ = 0;
            while(_loc5_ > _loc1_)
            {
               _loc13_ = this.m_select_var + _loc1_;
               _loc13_ = _loc13_ % _loc5_;
               if(this.checkUnlockVar(_loc3_,_loc13_))
               {
                  this.m_select_var = _loc13_;
                  break;
               }
               _loc1_++;
            }
         }
         var _loc6_:int = this.m_select_row;
         var _loc7_:int = this.m_select_column - this.m_select_column_start;
         var _loc8_:MovieClip = this.m_timeline.cha_select;
         var _loc9_:MovieClip = _loc8_["chara_icn_set0" + (_loc7_ + 1)];
         var _loc10_:MovieClip = _loc9_["nest_charaselect0" + (_loc6_ + 1)];
         this.m_select_icon_row = _loc6_;
         this.m_select_icon_column = _loc7_;
         _loc1_ = 0;
         while(IndexNumCharaNewIcon > _loc1_)
         {
            _loc14_ = this.getMcChamysetNewIcon(_loc1_);
            if(_loc14_)
            {
               _loc14_.visible = false;
            }
            _loc1_++;
         }
         _loc1_ = 0;
         while(CharaVarIndexNum > _loc1_)
         {
            _loc15_ = this.getMcChamyset(_loc1_);
            _loc16_ = this.getMcChamysetNewIcon(_loc1_);
            if(_loc15_)
            {
               _loc17_ = this.checkUnlockVar(_loc3_,_loc1_);
               _loc15_.btnact_off.visible = true;
               if(_loc1_ < _loc5_)
               {
                  if(_loc17_)
                  {
                     if(_loc1_ == this.m_select_var)
                     {
                        if(_loc15_.currentLabel != "on")
                        {
                           _loc15_.gotoAndPlay("on");
                           _loc15_.btnact_ef.visible = true;
                           _loc15_.btnact_on.visible = true;
                           _loc15_.btnact_off.visible = false;
                        }
                     }
                     else
                     {
                        _loc15_.gotoAndPlay("off");
                        _loc15_.btnact_ef.visible = false;
                        _loc15_.btnact_on.visible = false;
                        _loc15_.btnact_off.visible = true;
                     }
                     if(!_loc16_)
                     {
                     }
                     _loc18_ = false;
                     _loc19_ = false;
                     if(_loc4_[_loc1_][VarTypeCustomCostume])
                     {
                        _loc18_ = true;
                     }
                     if(_loc4_[_loc1_][VarTypeCustomCostumeEx])
                     {
                        _loc19_ = true;
                     }
                     _loc15_.btnact_ef.blue.visible = false;
                     _loc15_.btnact_ef.red.visible = false;
                     _loc15_.btnact_ef.yellow.visible = false;
                     _loc15_.btnact_on.blue.visible = false;
                     _loc15_.btnact_on.red.visible = false;
                     _loc15_.btnact_on.yellow.visible = false;
                     _loc15_.btnact_off.blue.visible = false;
                     _loc15_.btnact_off.red.visible = false;
                     _loc15_.btnact_off.yellow.visible = false;
                     _loc20_ = 1;
                     _loc21_ = 2;
                     _loc22_ = 3;
                     if(_loc18_)
                     {
                        if(_loc19_)
                        {
                           _loc15_.btnact_ef.blue.visible = true;
                           _loc15_.btnact_on.blue.visible = true;
                           _loc15_.btnact_off.blue.visible = true;
                        }
                        else
                        {
                           _loc15_.btnact_ef.red.visible = true;
                           _loc15_.btnact_on.red.visible = true;
                           _loc15_.btnact_off.red.visible = true;
                        }
                     }
                     else
                     {
                        _loc15_.btnact_ef.yellow.visible = true;
                        _loc15_.btnact_on.yellow.visible = true;
                        _loc15_.btnact_off.yellow.visible = true;
                     }
                  }
                  else
                  {
                     _loc15_.gotoAndPlay("lock");
                     _loc15_.btnact_ef.visible = false;
                     _loc15_.btnact_on.visible = false;
                     _loc15_.btnact_off.visible = true;
                     if(_loc16_)
                     {
                        _loc16_.visible = false;
                     }
                  }
               }
               else
               {
                  _loc15_.gotoAndPlay("off");
                  _loc15_.btnact_ef.visible = false;
                  _loc15_.btnact_on.visible = false;
                  _loc15_.btnact_off.visible = false;
                  if(_loc16_)
                  {
                     _loc16_.visible = false;
                  }
               }
            }
            else if(_loc16_)
            {
               _loc16_.visible = false;
            }
            _loc1_++;
         }
         var _loc11_:int = this.getVarIndexNum(_loc3_);
         var _loc12_:* = false;
         if(_loc11_ > 1)
         {
            _loc12_ = true;
         }
         this.m_timeline.cha_parameter.nest_clothes.sys_r.visible = _loc12_;
         this.m_timeline.cha_parameter.nest_clothes.sys_l.visible = _loc12_;
         this.m_select_info[this.m_current_player_index][SelectInfoTypeListIndex] = _loc3_;
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
               _loc5_ = this.m_callback.GetUserDataString(ReceiveType_ImageStrStart + this.masterIndex(param2));
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
         var _loc9_:MovieClip = null;
         var _loc10_:Array = null;
         var _loc11_:int = 0;
         var _loc12_:int = 0;
         var _loc3_:MovieClip = this.m_timeline.cha_select;
         var _loc4_:MovieClip = this.m_timeline.cha_new;
         var _loc5_:MovieClip = _loc3_["chara_icn_set0" + (param1 + 1)];
         var _loc6_:MovieClip = _loc5_["nest_charaselect0" + (param2 + 1)];
         if(!_loc6_)
         {
            return;
         }
         var _loc7_:int = this.calcCharaListIndex(param2,param1 + this.m_select_column_start);
         var _loc8_:MovieClip = _loc4_["chara_new_" + ((param1 + 1) * 3 + param2)];
         this.setImage(_loc6_.chara_img,_loc7_,false);
         this.showFavoriteStar(_loc6_,_loc7_);
         if(this.checkNoChara(_loc7_))
         {
            _loc6_.visible = false;
            _loc8_.visible = false;
         }
         else if(this.checkUnlockChara(_loc7_))
         {
            _loc6_.visible = true;
            _loc8_.visible = this.checkCharaNewIconVisible(_loc7_);
            _loc9_ = this.m_timeline.cha_select_cur;
            _loc10_ = this.getCharaInfo(_loc7_);
            _loc11_ = int(_loc10_[VarTypeAvatarSlotID]);
            _loc12_ = this.m_callback.GetUserDataInt(ReceiveType_FlagSelectAvatar);
            if(!_loc12_ && this.m_current_player_index != PlayerIndexOwn && this.checkAvatar(_loc7_) && this.checkSelectedAvatar(_loc7_))
            {
               if(_loc6_.currentLabel != "grayout")
               {
                  _loc6_.gotoAndStop("grayout");
               }
               if(this.checkSelectedAvatar(_loc7_))
               {
                  if(this.m_select_column - this.m_select_column_start == param1 && this.m_select_row == param2)
                  {
                     this.setImage(_loc9_.chara_sel.chara_img,_loc7_,false);
                     this.showFavoriteStar(_loc9_.chara_sel,_loc7_);
                  }
               }
            }
            else
            {
               if(_loc6_.currentLabel != "off")
               {
                  _loc6_.gotoAndStop("off");
               }
               if(this.m_select_icon_column == param1 && this.m_select_icon_row == param2)
               {
                  _loc9_.visible = true;
                  _loc9_.x = _loc3_.x + _loc5_.x + _loc6_.x;
                  _loc9_.y = _loc3_.y + _loc5_.y + _loc6_.y;
                  this.setImage(_loc9_.chara_sel.chara_img,_loc7_,false);
                  this.showFavoriteStar(_loc9_.chara_sel,_loc7_);
                  if(this.m_flag_decide)
                  {
                     if(_loc9_.currentLabel != "push")
                     {
                        _loc9_.gotoAndStop("push");
                     }
                  }
                  else if(_loc9_.currentLabel != "on")
                  {
                     _loc9_.gotoAndStop("on");
                  }
               }
            }
         }
         else
         {
            _loc6_.visible = true;
            if(_loc6_.currentLabel != "lock")
            {
               _loc6_.gotoAndStop("lock");
            }
            _loc8_.visible = false;
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
         var _loc3_:String = null;
         var _loc4_:Number = NaN;
         var _loc5_:Number = NaN;
         var _loc6_:String = null;
         var _loc7_:Number = NaN;
         var _loc8_:Number = NaN;
         var _loc1_:int = 0;
         this.ResetIcons();
         var _loc2_:MovieClip = this.m_timeline.cha_skill;
         if(this.m_flag_skill)
         {
            if(_loc2_.currentFrame > Utility.GetLabelEndFrame(_loc2_,"wait"))
            {
               _loc2_.gotoAndPlay("start");
               _loc2_.visible = true;
            }
            _loc1_ = 0;
            while(SkillMax > _loc1_)
            {
               _loc6_ = this.m_callback.GetUserDataString(ReceiveType_SkillNameStrStart + _loc1_);
               _loc2_.inact_skill["skill0" + (_loc1_ + 1)].sys_skill.scaleX = this.m_skill_str_scalex_default;
               _loc2_.inact_skill["skill0" + (_loc1_ + 1)].sys_skill.autoSize = "left";
               _loc2_.inact_skill["skill0" + (_loc1_ + 1)].sys_skill.htmlText = _loc6_;
               _loc7_ = Number(_loc2_.inact_skill["skill0" + (_loc1_ + 1)].sys_skill.width);
               _loc8_ = this.m_skill_str_scalex_default;
               if(this.m_skill_str_width_default < _loc7_)
               {
                  _loc8_ = this.m_skill_str_width_default / _loc7_;
               }
               _loc2_.inact_skill["skill0" + (_loc1_ + 1)].sys_skill.scaleX = _loc8_;
               _loc1_++;
            }
            _loc3_ = this.m_callback.GetUserDataString(ReceiveType_TarismanNameStr);
            _loc2_.inact_skill.skill09.sys_skill.scaleX = this.m_skill_str_scalex_default;
            _loc2_.inact_skill.skill09.sys_skill.autoSize = "left";
            _loc2_.inact_skill.skill09.sys_skill.htmlText = _loc3_;
            _loc4_ = Number(_loc2_.inact_skill.skill09.sys_skill.width);
            _loc5_ = this.m_skill_str_scalex_default;
            if(this.m_skill_str_width_default < _loc4_)
            {
               _loc5_ = this.m_skill_str_width_default / _loc4_;
            }
            _loc2_.inact_skill.skill09.sys_skill.scaleX = _loc5_ - 0.15;
            _loc2_.inact_skill.skill09.sys_skill.scaleY = 1;
         }
         else if(_loc2_.currentFrame <= Utility.GetLabelEndFrame(_loc2_,"wait"))
         {
            _loc2_.gotoAndPlay("end");
         }
      }
      
      // ---- CharaSearch: character search and favorites ----
      // The grid shows a view of the game's list: the loaded CaC first, then
      // the favorites, then the rest, filtered by the XV2 Mod Loader's search
      // box (chara_search.cpp does the matching and keeps the favorites). The
      // list as the game sent it is the "master" list: everything the game
      // indexes by list position (image strings, unlock flags) is read by
      // master index (masterIndex); CharaListShiftCount, the only list
      // number the game is sent back (SelectListIndex, RequestNewInfo), is
      // permuted with the view so the game always gets the right character.

      // The master index of grid entry `display`.
      private function masterIndex(display:int) : int
      {
         if(this.m_view == null || display < 0 || display >= this.m_view.length)
         {
            return display;
         }
         return int(this.m_view[display]);
      }

      // The master index under the current player's cursor, -1 when none.
      private function currentMaster() : int
      {
         if(this.m_current_player_index < 0 || PlayerMax <= this.m_current_player_index)
         {
            return -1;
         }
         var display:int = int(this.m_select_info[this.m_current_player_index][SelectInfoTypeListIndex]);
         if(this.checkNoChara(display))
         {
            return -1;
         }
         return this.masterIndex(display);
      }

      // Whether master entry `entry` holds a character (the list has padding
      // entries: [-1] or an empty code, as checkNoChara sees them).
      private static function isCharaEntry(entry:*) : Boolean
      {
         var info:Array = entry as Array;
         if(info == null || info.length <= 0)
         {
            return false;
         }
         var first:Array = info[0] as Array;
         return first != null && first[VarTypeCode] != InvalidCode;
      }

      // Called from waitStart until it returns true: sends the list to the
      // mod once, then waits (CharaSearchWaitFrames at most) for its first
      // answer, so the grid opens already in order. Without the mod's answer
      // only the CaC moves to the front.
      private function startCharaSearch() : Boolean
      {
         try
         {
            if(this.m_master_list == null)
            {
               this.m_master_list = this.m_chara_list.concat();
               this.m_master_shift = CharaListShiftCount.concat();
               this.m_own_master = this.findOwnMaster();
               this.m_search_token = String(Math.floor(Math.random() * 1000000000)) + "-" + this.m_master_list.length;
               this.m_callback.CallbackUserDataString("user",SendType_CharaSearchList,this.m_search_token + "\n" + this.describeMasterList());
            }
            this.m_callback.CallbackUserData("user",SendType_CharaSearchPoll,-1);
            var state:String = this.readSearchState();
            if(state == null && this.m_search_wait < CharaSearchWaitFrames)
            {
               this.m_search_wait++;
               return false;
            }
            this.applySearchState(state,false);
            this.resetStartPicks();
         }
         catch(e:Error)
         {
            trace("[CHARASELE] character search: " + e.toString());
         }
         return true;
      }

      // The other players' starting cursors, picked again over the shown
      // grid. waitUnlock picks them before the grid is reordered (in the
      // game's list order) and setView keeps each on its character, so the
      // second player started on the game list's first entry wherever it
      // ended up (another CaC, or far down the grid) instead of the next
      // one after the CaC. Same rule as waitUnlock: the first unlocked
      // entry, CaCs skipped unless the game lets the others pick one
      // (FlagSelectAvatar, or a custom list); changeCurrentPlayer then
      // moves off a CaC already picked.
      private function resetStartPicks() : void
      {
         var start:int = this.nextStartPick(-1);
         if(start < 0)
         {
            return;
         }
         var player:int = 0;
         while(player < this.m_select_info.length)
         {
            if(player != PlayerIndexOwn)
            {
               this.m_select_info[player][SelectInfoTypeListIndex] = start;
               this.m_select_info[player][SelectInfoTypeVarIndex] = 0;
            }
            player++;
         }
      }

      // The first entry after grid position `after` (-1 = from the top,
      // wrapping past the end) another player may start on: unlocked, not
      // padding, CaCs only when the game lets the others pick one
      // (FlagSelectAvatar, or a custom list). -1 when there is none.
      private function nextStartPick(after:int) : int
      {
         var avatarsAllowed:Boolean = this.m_flag_customlist || Boolean(this.m_callback.GetUserDataInt(ReceiveType_FlagSelectAvatar));
         var step:int = 1;
         while(step <= this.m_chara_list_num)
         {
            var display:int = (after + step) % this.m_chara_list_num;
            if(display >= 0 && !this.checkNoChara(display) && (avatarsAllowed || !this.checkAvatar(display)) && this.checkUnlockChara(display))
            {
               return display;
            }
            step++;
         }
         return -1;
      }

      // Where the player who just decided picked (their cursor's grid
      // position, kept by setSelectChara), or -1 when changeCurrentPlayer
      // goes back (cancel) or there was no player. Read before the player
      // index moves.
      private function decidedPick(decided:Boolean) : int
      {
         var player:int = this.m_current_player_index;
         if(!decided || player < 0 || PlayerMax <= player)
         {
            return -1;
         }
         return int(this.m_select_info[player][SelectInfoTypeListIndex]);
      }

      // After a decide, the next player's cursor starts on the next entry
      // after the one just picked, not on its start pick from the top of
      // the grid. Going back keeps the earlier player's own pick.
      private function startAfterDecided(decidedAt:int) : void
      {
         var player:int = this.m_current_player_index;
         if(decidedAt < 0 || player < 0 || PlayerMax <= player)
         {
            return;
         }
         var start:int = this.nextStartPick(decidedAt);
         if(start >= 0)
         {
            this.m_select_info[player][SelectInfoTypeListIndex] = start;
            this.m_select_info[player][SelectInfoTypeVarIndex] = 0;
         }
      }

      // The loaded CaC: the avatar entry whose save slot is the one of the
      // game's first table entry (the slot recieveList gives the CaC);
      // otherwise the first avatar entry, otherwise the first entry.
      private function findOwnMaster() : int
      {
         var slot:int = this.m_callback.GetUserDataInt(ReceiveType_AvatarSlotID);
         var firstAvatar:int = -1;
         var index:int = 0;
         while(index < this.m_master_list.length)
         {
            if(isCharaEntry(this.m_master_list[index]))
            {
               var first:Array = this.m_master_list[index][0];
               if(first[VarTypeCode] == AvatarCode)
               {
                  if(int(first[VarTypeAvatarSlotID]) == slot)
                  {
                     return index;
                  }
                  if(firstAvatar < 0)
                  {
                     firstAvatar = index;
                  }
               }
            }
            index++;
         }
         return firstAvatar >= 0 ? firstAvatar : 0;
      }

      // One line per master entry: "key\tcode,code" (the codes of its
      // variations, for the name search). The key names the favorite:
      // "<code>_<costume>" of the first variation, "HUM@<save slot>" for a
      // CaC, empty for padding.
      private function describeMasterList() : String
      {
         var lines:Array = new Array();
         var index:int = 0;
         while(index < this.m_master_list.length)
         {
            var key:String = "";
            var codes:Array = new Array();
            if(isCharaEntry(this.m_master_list[index]))
            {
               var info:Array = this.m_master_list[index];
               var first:Array = info[0];
               if(first[VarTypeCode] == AvatarCode)
               {
                  key = AvatarCode + "@" + int(first[VarTypeAvatarSlotID]);
               }
               else
               {
                  key = first[VarTypeCode] + "_" + int(first[VarTypeMid]);
               }
               var variation:int = 0;
               while(variation < info.length)
               {
                  if(info[variation] is Array)
                  {
                     codes.push(info[variation][VarTypeCode]);
                  }
                  variation++;
               }
            }
            lines.push(key + "\t" + codes.join(","));
            index++;
         }
         return lines.join("\n");
      }

      // The mod's answer if it is for this list, else null.
      private function readSearchState() : String
      {
         var value:* = this.m_callback.GetUserDataString(ReceiveType_CharaSearchState);
         if(value == null || this.m_search_token == null)
         {
            return null;
         }
         var text:String = String(value);
         if(text.indexOf(this.m_search_token + "\t") != 0)
         {
            return null;
         }
         return text;
      }

      // "3,17,40" -> [3, 17, 40]
      private static function numberList(text:String) : Array
      {
         var numbers:Array = new Array();
         if(text == null || text == "")
         {
            return numbers;
         }
         var parts:Array = text.split(",");
         var index:int = 0;
         while(index < parts.length)
         {
            if(parts[index] != "")
            {
               numbers.push(int(parts[index]));
            }
            index++;
         }
         return numbers;
      }

      // Every frame from main: the cursor's entry to the mod (the favorite
      // toggle and the search box run there), and its answer applied once
      // the grid is idle (no player change, decide or page scroll running).
      private function pollCharaSearch() : void
      {
         if(this.m_master_list == null)
         {
            return;
         }
         this.settleFavoriteStars();
         this.m_callback.CallbackUserData("user",SendType_CharaSearchPoll,this.currentMaster());
         var state:String = this.readSearchState();
         if(state == null || state == this.m_search_state)
         {
            return;
         }
         if(this.m_flag_change_player || this.m_flag_decide || this.m_flag_change_page || this.m_count_change_disp > 0)
         {
            return;
         }
         try
         {
            this.applySearchState(state,true);
         }
         catch(e:Error)
         {
            this.m_search_state = state;
            trace("[CHARASELE] character search: " + e.toString());
         }
      }

      // Applies the mod's answer "token\tfilter\tfavorites" (filter "*" =
      // everything, else master indices; favorites: master indices).
      // `redraw`: the grid is up (from main), not still loading.
      private function applySearchState(state:String, redraw:Boolean) : void
      {
         this.m_search_state = state;
         var filter:Array = null;
         this.m_fav = new Object();
         if(state != null)
         {
            var parts:Array = state.split("\t");
            if(parts.length > 1 && parts[1] != "*")
            {
               filter = numberList(parts[1]);
            }
            var favorites:Array = parts.length > 2 ? numberList(parts[2]) : new Array();
            var index:int = 0;
            while(index < favorites.length)
            {
               this.m_fav[int(favorites[index])] = true;
               index++;
            }
         }
         var before:int = this.currentMaster();
         var order:Array = this.computeOrder(filter);
         if(!this.sameView(order))
         {
            this.setView(order);
            if(redraw)
            {
               this.refreshView();
            }
         }
         if(redraw)
         {
            this.m_flag_change_disp = true;
         }
         if(this.currentMaster() != before)
         {
            this.sendCharaInfo(this.m_current_player_index);
         }
      }

      // The grid order: the CaC (always shown), then the shown favorites,
      // then the other shown entries, each in master order.
      private function computeOrder(filter:Array) : Array
      {
         var count:int = this.m_master_list.length;
         var order:Array = new Array();
         if(count <= 0)
         {
            return order;
         }
         var shown:Array = new Array(count);
         var index:int = 0;
         if(filter == null)
         {
            while(index < count)
            {
               shown[index] = true;
               index++;
            }
         }
         else
         {
            while(index < filter.length)
            {
               var listed:int = int(filter[index]);
               if(listed >= 0 && listed < count)
               {
                  shown[listed] = true;
               }
               index++;
            }
         }
         order.push(this.m_own_master);
         index = 0;
         while(index < count)
         {
            if(index != this.m_own_master && shown[index] == true && this.m_fav[index] == true)
            {
               order.push(index);
            }
            index++;
         }
         index = 0;
         while(index < count)
         {
            if(index != this.m_own_master && shown[index] == true && this.m_fav[index] != true)
            {
               order.push(index);
            }
            index++;
         }
         return order;
      }

      private function sameView(order:Array) : Boolean
      {
         if(this.m_view == null || this.m_view.length != order.length)
         {
            return false;
         }
         var index:int = 0;
         while(index < order.length)
         {
            if(int(this.m_view[index]) != int(order[index]))
            {
               return false;
            }
            index++;
         }
         return true;
      }

      // Makes `order` (master indices) the grid: the list, faces and shift
      // counts follow it, every player's pick keeps its character (a decided
      // one is kept in the grid even when filtered out; an undecided pick
      // that is gone moves to the first unlocked entry after the CaC).
      private function setView(order:Array) : void
      {
         var count:int = this.m_master_list.length;
         var index:int = 0;
         var player:int = 0;
         // the picks as master indices, by the old view
         var decided:Array = new Array();
         while(index < this.m_selected_index_list.length)
         {
            var picked:int = int(this.m_selected_index_list[index]);
            decided.push(picked >= 0 ? this.masterIndex(picked) : -1);
            if(picked >= 0 && order.indexOf(decided[index]) < 0)
            {
               order.push(decided[index]);
            }
            index++;
         }
         var picks:Array = new Array();
         while(player < this.m_select_info.length)
         {
            picks.push(this.masterIndex(int(this.m_select_info[player][SelectInfoTypeListIndex])));
            player++;
         }
         var newDisplay:Array = new Array(count);
         var list:Array = new Array();
         var faces:Array = new Array();
         CharaListShiftCount.length = 0;
         index = 0;
         while(index < order.length)
         {
            var master:int = int(order[index]);
            newDisplay[master] = index;
            list.push(this.m_master_list[master]);
            CharaListShiftCount.push(this.m_master_shift[master]);
            faces.push(this.faceFor(master));
            index++;
         }
         this.m_view = order;
         this.m_chara_list = list;
         if(this.m_master_faces != null)
         {
            this.m_chara_face = faces;
         }
         this.m_chara_list_num = order.length;
         this.m_chara_num_column = int((order.length - 1) / IndexNumRow) + 1;
         index = 0;
         while(index < decided.length)
         {
            if(decided[index] >= 0)
            {
               this.m_selected_index_list[index] = int(newDisplay[decided[index]]);
            }
            index++;
         }
         var fallback:int = 0;
         index = 1;
         while(index < order.length)
         {
            if(this.checkUnlockChara(index) && !this.checkNoChara(index))
            {
               fallback = index;
               break;
            }
            index++;
         }
         player = 0;
         while(player < this.m_select_info.length)
         {
            var shownAt:* = newDisplay[int(picks[player])];
            if(shownAt === undefined)
            {
               this.m_select_info[player][SelectInfoTypeListIndex] = fallback;
               this.m_select_info[player][SelectInfoTypeVarIndex] = 0;
            }
            else
            {
               this.m_select_info[player][SelectInfoTypeListIndex] = int(shownAt);
            }
            player++;
         }
         var current:int = this.m_current_player_index;
         if(current < 0 || PlayerMax <= current)
         {
            return;
         }
         var display:int = int(this.m_select_info[current][SelectInfoTypeListIndex]);
         this.m_select_row = display % IndexNumRow;
         this.m_select_column = int(display / IndexNumRow);
         this.m_select_var = int(this.m_select_info[current][SelectInfoTypeVarIndex]);
         // the page: as updatePage would scroll it, never past the last column
         var lastStart:int = Math.max(0,this.m_chara_num_column - IndexNumColumn);
         if(this.m_select_column_start > lastStart)
         {
            this.m_select_column_start = lastStart;
         }
         if(this.m_select_column < this.m_select_column_start)
         {
            this.m_select_column_start = this.m_select_column;
         }
         else if(this.m_select_column_start + IndexNumColumn <= this.m_select_column)
         {
            this.m_select_column_start = this.m_select_column - IndexNumColumn + 1;
         }
      }

      // After setView on the open grid: the columns as startMain shows
      // them (sets 1-8 by the column count; every icon on, updateCharaIconUnit
      // hides the empty ones), the cursor; main redraws the icons
      // (m_flag_change_disp).
      private function refreshView() : void
      {
         var select:MovieClip = this.m_timeline.cha_select;
         var column:int = 1;
         while(column < 9)
         {
            var icons:MovieClip = select["chara_icn_set0" + column];
            if(icons)
            {
               icons.visible = column <= this.m_chara_num_column;
               var row:int = 1;
               while(row <= IndexNumRow)
               {
                  if(icons["nest_charaselect0" + row])
                  {
                     icons["nest_charaselect0" + row].visible = true;
                  }
                  row++;
               }
            }
            column++;
         }
         this.setSelectChara();
      }

      // After startMain (which made the faces of the grid it opened with):
      // the faces by master index; the others are made when first shown.
      private function keepMasterFaces() : void
      {
         if(this.m_master_list == null)
         {
            return;
         }
         this.m_master_faces = new Array(this.m_master_list.length);
         var index:int = 0;
         while(index < this.m_chara_list_num)
         {
            this.m_master_faces[this.masterIndex(index)] = this.m_chara_face[index];
            index++;
         }
      }

      // Master entry `master`'s face, made as startMain makes them.
      private function faceFor(master:int) : Bitmap
      {
         if(this.m_master_faces == null)
         {
            return null;
         }
         if(this.m_master_faces[master] === undefined)
         {
            var face:Bitmap = null;
            if(isCharaEntry(this.m_master_list[master]))
            {
               face = new Bitmap(null);
               IggyFunctions.setTextureForBitmap(face,this.m_callback.GetUserDataString(ReceiveType_ImageStrStart + master));
               face.scaleX = 256 / face.width;
               face.scaleY = 128 / face.height;
            }
            this.m_master_faces[master] = face;
         }
         return this.m_master_faces[master];
      }

      // The gold star on the left of a favorite's icon, halfway down
      // (`icon`: a grid icon or the cursor's, each holding chara_img). It is
      // a text field in the character name's font, which has the star (the
      // style list uses the same one).
      private function showFavoriteStar(icon:MovieClip, display:int) : void
      {
         if(icon == null)
         {
            return;
         }
         var star:TextField = icon.getChildByName(FavoriteStarName) as TextField;
         var wanted:Boolean = this.m_master_list != null && this.m_fav[this.masterIndex(display)] == true && !this.checkNoChara(display);
         if(!wanted)
         {
            if(star)
            {
               star.visible = false;
            }
            return;
         }
         try
         {
            // While the screen opens the icon has no children yet (a layout
            // dump of the icon showed only the star): the timeline adds
            // the face and frame later, above anything added before.
            var tile:Rectangle = this.iconBounds(icon,star);
            if(tile == null)
            {
               if(star)
               {
                  star.visible = false;
               }
               return;
            }
            if(star == null)
            {
               star = this.makeFavoriteStar();
            }
            // Topmost by re-adding: setChildIndex left the stars made at open
            // under the face (only stars made later showed).
            if(star.parent != icon || icon.getChildIndex(star) != icon.numChildren - 1)
            {
               if(star.parent != null)
               {
                  star.parent.removeChild(star);
               }
               icon.addChild(star);
            }
            // The field's size is fixed (makeFavoriteStar): Iggy's measured
            // text size is wrong right after the text is set, which put the
            // first stars off their icon or hid them.
            star.x = tile.x + tile.width * FavoriteStarInset;
            star.y = tile.y + (tile.height - star.height) / 2;
            star.visible = true;
         }
         catch(e:Error)
         {
            trace("[CHARASELE] favorite star: " + e.toString());
         }
      }

      // Stars made while the screen fades in (the saved favorites') copy the
      // name field's state at that moment and never drew, although a dump of
      // the grid showed them there, visible, on top and in place; stars made
      // later drew. Once the name has faded in, every star made so far is
      // remade from its settled state (once per screen).
      private function settleFavoriteStars() : void
      {
         if(this.m_stars_settled)
         {
            return;
         }
         var nameField:TextField = this.m_timeline.cha_parameter.sys_charaName.sys_charaName;
         if(nameField.alpha < 1 || nameField.text.length == 0)
         {
            return;
         }
         this.m_stars_settled = true;
         try
         {
            var icons:Array = [this.m_timeline.cha_select_cur.chara_sel];
            var column:int = -1;
            while(column <= IndexNumColumn)
            {
               var iconSet:MovieClip = this.m_timeline.cha_select["chara_icn_set0" + (column + 1)];
               var row:int = 0;
               while(iconSet != null && row < IndexNumRow)
               {
                  icons.push(iconSet["nest_charaselect0" + (row + 1)]);
                  row++;
               }
               column++;
            }
            var index:int = 0;
            while(index < icons.length)
            {
               var icon:MovieClip = icons[index];
               index++;
               var old:TextField = icon != null ? icon.getChildByName(FavoriteStarName) as TextField : null;
               if(old != null)
               {
                  var star:TextField = this.makeFavoriteStar();
                  star.x = old.x;
                  star.y = old.y;
                  star.visible = old.visible;
                  icon.removeChild(old);
                  icon.addChild(star);
               }
            }
         }
         catch(e:Error)
         {
            trace("[CHARASELE] favorite star settle: " + e.toString());
         }
      }

      // The star's text field: the character name's font at a fixed size,
      // left-aligned in a fixed box (never measured; see showFavoriteStar).
      // 2 = the text field's gutter on each side.
      private function makeFavoriteStar() : TextField
      {
         var star:TextField = copyNameField(this.m_timeline.cha_parameter.sys_charaName.sys_charaName);
         var format:* = star.defaultTextFormat;
         format.size = FavoriteStarFontSize;
         // setTextFormat below overrides the html's font colour with the
         // format's (the name's white), so the gold goes in the format too.
         format.color = 0xFFC800;
         format.align = "left";
         format.leading = 0;
         star.defaultTextFormat = format;
         star.name = FavoriteStarName;
         star.autoSize = "none";
         star.rotation = 0;
         star.scaleX = 1;
         star.scaleY = 1;
         // copyNameField copies the name's alpha, which is 0 while the
         // screen fades in: the stars made as it opens (the saved
         // favorites') stayed invisible.
         star.alpha = 1;
         star.width = FavoriteStarFontSize * 1.5 + 4;
         star.height = FavoriteStarFontSize * 1.2 + 4;
         star.htmlText = FavoriteStarHtml;
         star.setTextFormat(format);
         return star;
      }

      // A blank text field in `source`'s font, size, place and look. Iggy
      // draws only fonts the movie already uses, so the star borrows the
      // character name's.
      private static function copyNameField(source:TextField) : TextField
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

      // The icon's tile in its own space: its frame, chara_icn (-64,-32
      // 128x64, measured with a layout dump of the icon). Not the picture:
      // Iggy gives chara_img no mask, and the face bitmap is 205 wide, so a
      // star placed by it sat half a tile left of (or right of) its own.
      private function iconBounds(icon:MovieClip, star:DisplayObject) : Rectangle
      {
         var frame:DisplayObject = icon.chara_icn;
         if(frame == null)
         {
            return null;
         }
         var bounds:Rectangle = frame.getBounds(icon);
         if(bounds.width <= 0 || bounds.height <= 0)
         {
            return null;
         }
         return bounds;
      }

      private function updateVariation() : *
      {
         var _loc2_:int = 0;
         var _loc3_:MovieClip = null;
         var _loc4_:Boolean = false;
         var _loc5_:Boolean = false;
         var _loc1_:int = this.calcCharaListIndex(this.m_select_row,this.m_select_column);
         if(this.checkNoChara(_loc1_))
         {
            _loc1_ = 0;
         }
         _loc2_ = 0;
         while(IndexNumCharaVariationNewIcon > _loc2_)
         {
            _loc3_ = this.getMcVariationNewIcon(_loc2_);
            _loc4_ = this.checkUnlockVar(_loc1_,_loc2_);
            if(_loc4_)
            {
               _loc5_ = this.checkVariationNewIconVisible(_loc1_,_loc2_);
               if(_loc3_.visible != _loc5_)
               {
                  if(this.m_icon_show_wait[_loc2_] < 1)
                  {
                     this.m_icon_show_wait[_loc2_] += 1;
                  }
                  else
                  {
                     this.m_icon_show_wait[_loc2_] = 0;
                     _loc3_.visible = _loc5_;
                  }
               }
               else
               {
                  this.m_icon_show_wait[_loc2_] = 0;
               }
            }
            else
            {
               _loc3_.visible = false;
            }
            trace("m_icon_show_wait[" + _loc2_ + "]:" + this.m_icon_show_wait[_loc2_] + " visible:" + _loc3_.visible);
            _loc2_++;
         }
      }
      
      private function updateDisply() : *
      {
         var _loc7_:* = undefined;
         trace("updateDisplay");
         var _loc1_:int = int(this.m_select_info[this.m_current_player_index][SelectInfoTypeListIndex]);
         var _loc2_:Array = this.getCharaInfo(_loc1_);
         this.updateCharaIcon();
         var _loc3_:String = this.m_callback.GetUserDataString(ReceiveType_CharaNameStr);
         var _loc4_:String = "";
         if(_loc2_[this.m_select_var][VarTypeFlag_GK2] == true)
         {
            _loc4_ = this.m_callback.GetUserDataString(ReceiveType_NameOption_GK2);
         }
         else if(_loc2_[this.m_select_var][VarTypeFlag_CGK] == true)
         {
            _loc4_ = this.m_callback.GetUserDataString(ReceiveType_NameOption_CGK);
         }
         else if(_loc2_[this.m_select_var][VarTypeFlag_KFK] == true)
         {
            _loc4_ = this.m_callback.GetUserDataString(ReceiveType_NameOption_KFK);
         }
         _loc3_ += _loc4_;
         var _loc5_:int = this.m_callback.GetUserDataInt(ReceiveType_NameOption_LvNum);
         if(_loc5_ != -1)
         {
            _loc7_ = this.m_callback.GetUserDataString(ReceiveType_NameOption_LvText);
            _loc3_ += " (";
            _loc3_ += _loc7_;
            _loc3_ += _loc5_.toString();
            _loc3_ += ")";
         }
         Utility.SetAutoSizedhtmlText(this.m_timeline.cha_parameter.sys_charaName.sys_charaName,_loc3_);
         var _loc6_:String = this.m_callback.GetUserDataString(ReceiveType_VariationNameStr);
         this.m_timeline.cha_parameter.nest_clothes.sys_charavari_name.sys_charavari_name.htmlText = _loc6_;
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
         var _loc3_:int = this.masterIndex(param1) * CharaVarIndexNum + param2;
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
         this.changeLR(true);
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
         this.changeLR(false);
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
         if(Boolean(this.m_current_player_index != PlayerIndexOwn) && Boolean(this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle)) && !this.m_callback.GetUserDataInt(ReceiveType_Flag2pController))
         {
            return;
         }
         var _loc1_:int = 0;
         var _loc2_:* = 0;
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
         var _loc3_:int = 0;
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
         var _loc5_:int = this.getVarIndexNum(_loc4_);
         var _loc6_:int = 0;
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
         var _loc2_:int = 0;
         if(this.m_flag_change_player)
         {
            return;
         }
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_FlagUseCancel);
         if(!_loc1_)
         {
            _loc2_ = this.m_callback.GetUserDataInt(ReceiveType_FlagLocalBattle);
            if(_loc2_)
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
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_PlayerEnmNum);
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

