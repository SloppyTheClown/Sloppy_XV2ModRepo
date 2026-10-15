package action_script
{
   import flash.display.DisplayObject;
   import flash.display.DisplayObjectContainer;
   import flash.display.MovieClip;
   import flash.events.Event;
   import flash.events.KeyboardEvent;
   import flash.external.ExternalInterface;
   import flash.geom.Rectangle;
   import flash.text.TextField;
   import flash.ui.Keyboard;
   
   public class AvatarMyset
   {
      
      public static const MysetNum:int = 7;
      
      public static const CustomizeNum:int = 2;
      
      public static const CostumeElemNum:int = 7;
      
      public static const SkillElemNum:int = 8;
      
      public static const ReceiveType_ModeType:int = 0;
      
      public static const ReceiveType_RequestResume:int = 1;
      
      public static const ReceiveType_CostumeStrStart:int = 2;
      
      public static const ReceiveType_CostumeStrEnd:int = ReceiveType_CostumeStrStart + CostumeElemNum - 1;
      
      public static const ReceiveType_SkillStrStart:int = ReceiveType_CostumeStrEnd + 1;
      
      public static const ReceiveType_SkillStrEnd:int = ReceiveType_SkillStrStart + SkillElemNum - 1;
      
      public static const ReceiveType_Num:int = ReceiveType_SkillStrEnd + 1;
      
      public static const SendType_RequestEquipData:int = 0;
      
      public static const SendType_RequestConfirmDialog:int = 1;
      
      public static const SendType_EquipMysetID:int = 2;
      
      public static const SendType_RegistMysetID:int = 3;
      
      public static const SendType_MysetCancel:int = 4;
      
      // BattleStyles: the preset's battle style comes appended to the last
      // skill string (user data ReceiveType_SkillStrEnd) after a tab; the
      // panel's layout goes to the mod's log once (sendType below, a string).
      internal static const StyleSeparator:String = "\t";

      internal static const SendType_StyleLayoutDump:int = 9901;

      // The label's text ends in a tab, which keeps the game's msg-id
      // replacement off it (as on the Customize screen's Battle Style slot).
      internal static const StyleRowLabel:String = "Battle Style\t";

      public static const ModeType_MysetMenu:int = 0;
      
      public static const ModeType_RegistShortCut:int = 1;
      
      public static const CustomizeType_Equip:int = 0;
      
      public static const CustomizeType_Regist:int = 1;
      
      private static const StringList:Object = {
         "MysetItemEquip1":"MST_BTN_00_001",
         "MysetItemEquip2":"MST_BTN_00_002",
         "MysetItemEquip3":"MST_BTN_00_003",
         "MysetItemEquip4":"MST_BTN_00_004",
         "MysetItemEquip5":"MST_BTN_00_005",
         "MysetItemEquip6":"MST_BTN_00_006",
         "MysetItemEquip7":"MST_BTN_00_007",
         "MysetItemRegist1":"MST_BTN_01_001",
         "MysetItemRegist2":"MST_BTN_01_002",
         "MysetItemRegist3":"MST_BTN_01_003",
         "MysetItemRegist4":"MST_BTN_01_004",
         "MysetItemRegist5":"MST_BTN_01_005",
         "MysetItemRegist6":"MST_BTN_01_006",
         "MysetItemRegist7":"MST_BTN_01_007",
         "CustomizeItem1":"MST_ALM_00_000",
         "CustomizeItem2":"MST_ALM_00_001",
         "EquipHeaderCostume":"MST_ALM_00_000",
         "EquipHeaderSkill":"MST_ALM_01_000",
         "EquipCostumeBust":"MST_STR_00_000",
         "EquipCostumePants":"MST_STR_00_001",
         "EquipCostumeGloves":"MST_STR_00_002",
         "EquipCostumeShoes":"MST_STR_00_003",
         "EquipAccessory":"MST_STR_00_004",
         "EpuipTalisman":"MST_STR_00_005",
         "EpuipMaterial":"MST_STR_00_006",
         "EpuipSkillUlt1":"MST_STR_01_000",
         "EpuipSkillUlt2":"MST_STR_01_001",
         "EpuipSkillSpa1":"MST_STR_01_002",
         "EpuipSkillSpa2":"MST_STR_01_003",
         "EpuipSkillSpa3":"MST_STR_01_004",
         "EpuipSkillSpa4":"MST_STR_01_005",
         "EpuipSkillMet1":"MST_STR_01_007",
         "EpuipSkillEsc":"MST_STR_01_006",
         "Invalid":null
      };
      
      private static const CostumeItemArray:Array = [[0,StringList.EquipCostumeBust],[1,StringList.EquipCostumePants],[2,StringList.EquipCostumeGloves],[3,StringList.EquipCostumeShoes],[4,StringList.EquipAccessory],[6,StringList.EpuipMaterial],[5,StringList.EpuipTalisman]];
      
      private static const SkillItemArray:Array = [[3,StringList.EpuipSkillSpa2],[2,StringList.EpuipSkillSpa1],[4,StringList.EpuipSkillSpa3],[5,StringList.EpuipSkillSpa4],[1,StringList.EpuipSkillUlt2],[0,StringList.EpuipSkillUlt1],[6,StringList.EpuipSkillMet1],[7,StringList.EpuipSkillEsc]];
      
      private var m_callback:Callback = null;
      
      private var m_menu_list:ButtonList = null;
      
      private var m_timeline:MovieClip = null;
      
      private var m_customize_type:int;
      
      private var m_is_request_quit_menu:Boolean = false;
      
      private var m_is_start_wait:Boolean = false;

      // BattleStyles: the added Battle Style row (label, value), made once.
      private var m_style_row_made:Boolean = false;

      private var m_style_label:MovieClip = null;

      private var m_style_value:MovieClip = null;
      
      public function AvatarMyset()
      {
         super();
         this.m_callback = new Callback(ReceiveType_Num);
         this.m_menu_list = new ButtonList(MysetNum);
         this.m_timeline = null;
         try
         {
            ExternalInterface.addCallback("CallQuitMenu",this.CallQuitMenu);
         }
         catch(e:Error)
         {
            trace("addCallback - Error");
         }
      }
      
      public function Initialize(param1:MovieClip) : void
      {
         this.m_timeline = param1;
         this.m_timeline.visible = false;
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitStart);
      }
      
      private function waitStart(param1:Event) : void
      {
         if(!this.isReadyMysetMenu())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStart);
         this.initMysetMenu();
         this.Start();
      }
      
      public function Start() : void
      {
         this.m_timeline.visible = true;
         this.startMysetMenu();
      }
      
      private function waitMenuItem() : void
      {
         this.m_is_start_wait = true;
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitMenuItem);
      }
      
      private function endMyset(param1:Boolean) : void
      {
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         if(param1)
         {
            this.m_callback.CallbackDecide(1);
         }
         else
         {
            this.m_callback.CallbackCancel();
         }
         this.endMysetMenu();
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitEnd);
      }
      
      private function waitEnd(param1:Event) : void
      {
         if(!this.isEndMysetMenu())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEnd);
         this.m_callback.CallbackExit();
      }
      
      private function initMysetMenu() : void
      {
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.initMysetList);
         this.initCustomizeList();
         this.initEquipList();
      }
      
      private function isReadyMysetMenu() : Boolean
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ModeType))
         {
            return false;
         }
         return true;
      }
      
      private function startMysetMenu() : void
      {
         this.startCustomizeList();
         this.startEquipList();
         this.requestEquipList(0);
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitMenuItem);
      }
      
      private function endMysetMenu() : void
      {
         this.endMysetList();
         this.endCustomizeList();
         this.endEquipList();
      }
      
      private function isEndMysetMenu() : Boolean
      {
         if(!this.isEndMysetList())
         {
            return false;
         }
         if(!this.isEndCustomizeList())
         {
            return false;
         }
         if(!this.isEndEquipList())
         {
            return false;
         }
         return true;
      }
      
      private function selectMysetMenu() : Boolean
      {
         if(this.m_timeline.menulist.visible)
         {
            this.requestEquipList(this.m_menu_list.GetSelectUserData());
            return true;
         }
         if(this.m_timeline.menulist2.visible)
         {
            return true;
         }
         return false;
      }
      
      private function decideMysetMenu() : Boolean
      {
         if(this.m_timeline.menulist.visible)
         {
            this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
            this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitCompDecideMysetList);
            return true;
         }
         if(this.m_timeline.menulist2.visible)
         {
            this.m_callback.CallbackDecide(this.m_menu_list.GetSelectUserData());
            this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
            this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitCompDecideCustomizeList);
            this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.initMysetList);
            return true;
         }
         return false;
      }
      
      private function cancelMysetMenu() : Boolean
      {
         if(this.m_timeline.menulist.visible)
         {
            this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
            this.endMysetList();
            this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitEndCancelMysetList);
            this.m_callback.CallbackUserData("user",SendType_MysetCancel,0);
            return true;
         }
         if(this.m_timeline.menulist2.visible)
         {
            this.endMyset(false);
            return true;
         }
         return false;
      }
      
      private function waitCompDecideMysetList(param1:Event) : void
      {
         if(!this.isCompDecideMysetMenu())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitCompDecideMysetList);
         this.m_callback.SetUserDataValidFlag(ReceiveType_RequestResume,false);
         this.m_callback.CallbackUserData("user",SendType_RequestConfirmDialog,this.m_customize_type);
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitStandbyMysetMenu);
      }
      
      private function waitEndCancelMysetList(param1:Event) : void
      {
         if(!this.isEndMysetList())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEndCancelMysetList);
         this.startCustomizeList();
         this.requestEquipList(0);
      }
      
      private function waitCompDecideCustomizeList(param1:Event) : void
      {
         if(!this.isCompDecideMysetMenu())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitCompDecideCustomizeList);
         this.endCustomizeList();
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitEndDecideCustomizeList);
      }
      
      private function waitEndDecideCustomizeList(param1:Event) : void
      {
         if(!this.isEndCustomizeList())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEndDecideCustomizeList);
         this.startMysetList();
         this.requestEquipList(this.m_menu_list.GetSelectUserData());
      }
      
      private function waitStandbyMysetMenu(param1:Event) : void
      {
         var _loc3_:int = 0;
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_RequestResume))
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStandbyMysetMenu);
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveType_RequestResume);
         if(_loc2_)
         {
            _loc3_ = this.m_menu_list.GetSelectUserData();
            if(this.m_customize_type == CustomizeType_Equip)
            {
               this.m_callback.CallbackUserData("user",SendType_EquipMysetID,_loc3_);
               this.cancelMysetMenu();
               return;
            }
            if(this.m_customize_type == CustomizeType_Regist)
            {
               this.m_callback.CallbackUserData("user",SendType_RegistMysetID,_loc3_);
               this.requestEquipList(0);
            }
         }
         this.m_timeline.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
      }
      
      private function isCompDecideMysetMenu() : Boolean
      {
         var _loc1_:MovieClip = this.m_menu_list.GetButton(this.m_menu_list.GetSelectButton());
         if(_loc1_.currentLabel != "push_comp")
         {
            return false;
         }
         return true;
      }
      
      private function initMysetList(param1:Event) : void
      {
         var _loc4_:MovieClip = null;
         var _loc5_:String = null;
         var _loc2_:* = this.m_menu_list ? this.m_menu_list.GetSelectUserData() : CustomizeType_Equip;
         var _loc3_:int = 0;
         while(_loc3_ < MysetNum)
         {
            _loc4_ = this.m_timeline.menulist.room_menu_inact["btn" + (_loc3_ + 1)];
            _loc5_ = _loc2_ == CustomizeType_Equip ? StringList["MysetItemEquip" + (_loc3_ + 1)] : StringList["MysetItemRegist" + (_loc3_ + 1)];
            Utility.SetAutoSizedhtmlText(_loc4_.sys_menu.sys,_loc5_);
            _loc3_++;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.initMysetList);
      }
      
      private function startMysetList() : void
      {
         var _loc2_:MovieClip = null;
         this.m_timeline.menulist.visible = true;
         this.m_timeline.menulist2.visible = false;
         this.m_menu_list.ClearButton();
         var _loc1_:int = 1;
         while(_loc1_ <= MysetNum)
         {
            _loc2_ = this.m_timeline.menulist.room_menu_inact["btn" + _loc1_];
            _loc2_.gotoAndPlay("off");
            this.m_menu_list.AddButton(_loc2_,_loc1_);
            _loc1_++;
         }
         this.m_menu_list.ChangeButton(0);
         this.m_timeline.menulist.gotoAndPlay("start");
         this.m_timeline.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
      }
      
      private function endMysetList() : void
      {
         if(this.isEndMysetList())
         {
            return;
         }
         this.m_timeline.menulist.gotoAndPlay("end");
      }
      
      private function isEndMysetList() : Boolean
      {
         if(this.m_timeline.menulist.currentLabel != "end_comp")
         {
            return false;
         }
         return true;
      }
      
      private function initCustomizeList() : void
      {
         var _loc2_:MovieClip = null;
         this.m_customize_type = CustomizeType_Equip;
         var _loc1_:int = 0;
         while(_loc1_ < CustomizeNum)
         {
            _loc2_ = this.m_timeline.menulist2.room_menu_inact["btn" + (_loc1_ + 1)];
            _loc2_.sys_menu.sys.text = StringList["CustomizeItem" + (_loc1_ + 1)];
            _loc1_++;
         }
      }
      
      private function startCustomizeList() : *
      {
         var _loc2_:MovieClip = null;
         this.m_timeline.menulist.visible = false;
         this.m_timeline.menulist2.visible = true;
         this.m_menu_list.ClearButton();
         var _loc1_:int = 0;
         while(_loc1_ < CustomizeNum)
         {
            _loc2_ = this.m_timeline.menulist2.room_menu_inact["btn" + (_loc1_ + 1)];
            _loc2_.gotoAndPlay("off");
            this.m_menu_list.AddButton(_loc2_,_loc1_);
            _loc1_++;
         }
         this.m_menu_list.ChangeButton(this.m_customize_type);
         this.m_timeline.menulist2.gotoAndPlay("start");
         this.m_timeline.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
      }
      
      private function endCustomizeList() : void
      {
         if(this.isEndCustomizeList())
         {
            return;
         }
         this.m_timeline.menulist2.gotoAndPlay("end");
         this.m_customize_type = this.m_menu_list.GetSelectUserData();
      }
      
      private function isEndCustomizeList() : Boolean
      {
         if(this.m_timeline.menulist2.currentLabel != "end_comp")
         {
            return false;
         }
         return true;
      }
      
      private function initEquipList() : void
      {
         var _loc2_:int = 0;
         var _loc1_:MovieClip = this.m_timeline.menuwin._CMN_P_preset_plt_root._CMN_P_preset_plt;
         _loc1_.po_cate_01.sys.text = StringList.EquipHeaderCostume;
         _loc1_.po_cate_02.sys.text = StringList.EquipHeaderSkill;
         _loc2_ = 0;
         while(_loc2_ < CostumeElemNum)
         {
            Utility.SetAutoSizedText(_loc1_["sys_sb_a_0" + (_loc2_ + 1)].sys,CostumeItemArray[_loc2_][1]);
            Utility.SetAutoSizedhtmlText(_loc1_["sys_sb_b_0" + (_loc2_ + 1)].sys,"");
            _loc2_++;
         }
         _loc2_ = 0;
         while(_loc2_ < SkillElemNum)
         {
            Utility.SetAutoSizedText(_loc1_["sys_sk_a_0" + (_loc2_ + 1)].sys,SkillItemArray[_loc2_][1]);
            Utility.SetAutoSizedhtmlText(_loc1_["sys_sk_b_0" + (_loc2_ + 1)].sys,"");
            _loc2_++;
         }
         this.makeStyleRow(_loc1_);
         this.showStyle(null);
      }
      
      private function startEquipList() : void
      {
         this.m_timeline.menuwin.gotoAndPlay("start");
      }
      
      private function requestEquipList(param1:int) : void
      {
         var _loc2_:int = 0;
         _loc2_ = 0;
         while(_loc2_ < CostumeElemNum)
         {
            this.m_callback.SetUserDataValidFlag(ReceiveType_CostumeStrStart + _loc2_,false);
            _loc2_++;
         }
         _loc2_ = 0;
         while(_loc2_ < SkillElemNum)
         {
            this.m_callback.SetUserDataValidFlag(ReceiveType_SkillStrStart + _loc2_,false);
            _loc2_++;
         }
         this.m_callback.CallbackUserData("user",SendType_RequestEquipData,param1);
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitSetupEquipList);
      }
      
      private function waitSetupEquipList(param1:Event) : void
      {
         if(!this.checkSetupEquipList())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitSetupEquipList);
         this.changeEquipList();
      }
      
      private function checkSetupEquipList() : Boolean
      {
         var _loc1_:int = 0;
         _loc1_ = 0;
         while(_loc1_ < CostumeElemNum)
         {
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_CostumeStrStart + _loc1_))
            {
               return false;
            }
            _loc1_++;
         }
         _loc1_ = 0;
         while(_loc1_ < SkillElemNum)
         {
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_SkillStrStart + _loc1_))
            {
               return false;
            }
            _loc1_++;
         }
         return true;
      }
      
      private function changeEquipList() : void
      {
         var _loc2_:int = 0;
         var _loc3_:int = 0;
         var _loc1_:MovieClip = this.m_timeline.menuwin._CMN_P_preset_plt_root._CMN_P_preset_plt;
         _loc2_ = 0;
         while(_loc2_ < CostumeElemNum)
         {
            _loc3_ = ReceiveType_CostumeStrStart + CostumeItemArray[_loc2_][0];
            Utility.SetAutoSizedhtmlText(_loc1_["sys_sb_b_0" + (_loc2_ + 1)].sys,this.m_callback.GetUserDataString(_loc3_));
            _loc2_++;
         }
         _loc2_ = 0;
         while(_loc2_ < SkillElemNum)
         {
            _loc3_ = ReceiveType_SkillStrStart + SkillItemArray[_loc2_][0];
            Utility.SetAutoSizedhtmlText(_loc1_["sys_sk_b_0" + (_loc2_ + 1)].sys,textBeforeStyle(this.m_callback.GetUserDataString(_loc3_)));
            _loc2_++;
         }
         this.showStyle(styleAfterText(this.m_callback.GetUserDataString(ReceiveType_SkillStrEnd)));
      }

      // ---- BattleStyles: the Battle Style row ----

      // Shows the row with `style`, or hides it (null).
      private function showStyle(style:String) : void
      {
         if(this.m_style_label == null || this.m_style_value == null)
         {
            return;
         }
         this.m_style_label.visible = style != null;
         this.m_style_value.visible = style != null;
         if(style != null)
         {
            Utility.SetAutoSizedText(this.m_style_label.sys,StyleRowLabel);
            Utility.SetAutoSizedhtmlText(this.m_style_value.sys,style);
         }
      }

      // Adds the row: a copy of the Super Soul row's label/value one step
      // under it when that space is free (the skill column has a row more),
      // otherwise under the last skill row, with the window grown to fit.
      // Any failure leaves the vanilla panel. Sends the layout to the log.
      private function makeStyleRow(panel:MovieClip) : void
      {
         if(this.m_style_row_made)
         {
            return;
         }
         this.m_style_row_made = true;
         var notes:Array = [];
         try
         {
            this.m_style_label = copyRow(panel.sys_sb_a_07,panel.sys_sb_a_07.y - panel.sys_sb_a_06.y);
            this.m_style_value = copyRow(panel.sys_sb_b_07,panel.sys_sb_b_07.y - panel.sys_sb_b_06.y);
            panel.addChild(this.m_style_label);
            panel.addChild(this.m_style_value);
            if(overlapsRows(panel,this.m_style_label) || overlapsRows(panel,this.m_style_value))
            {
               panel.removeChild(this.m_style_label);
               panel.removeChild(this.m_style_value);
               this.m_style_label = copyRow(panel.sys_sk_a_08,panel.sys_sk_a_08.y - panel.sys_sk_a_07.y);
               this.m_style_value = copyRow(panel.sys_sk_b_08,panel.sys_sk_b_08.y - panel.sys_sk_b_07.y);
               panel.addChild(this.m_style_label);
               panel.addChild(this.m_style_value);
               notes.push("row under the skills");
               growBackgrounds(this.m_timeline.menuwin,panel.sys_sb_a_01,panel.sys_sk_a_08,this.m_style_label,notes);
            }
            else
            {
               notes.push("row under the equipment");
            }
         }
         catch(e:Error)
         {
            notes.push("no row: " + e.toString());
            removeFromParent(this.m_style_label);
            removeFromParent(this.m_style_value);
            this.m_style_label = null;
            this.m_style_value = null;
         }
         this.sendLayout("AVATAR_SET",this.m_timeline.menuwin,notes);
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

      
      private function endEquipList() : void
      {
         this.m_timeline.menuwin.gotoAndPlay("end");
      }
      
      private function isEndEquipList() : Boolean
      {
         if(this.m_timeline.menuwin.currentLabel != "end_comp")
         {
            return false;
         }
         return true;
      }
      
      private function pushU() : void
      {
         this.m_menu_list.SelectPrevButton();
         if(!this.selectMysetMenu())
         {
            return;
         }
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
      }
      
      private function pushD() : void
      {
         this.m_menu_list.SelectNextButton();
         if(!this.selectMysetMenu())
         {
            return;
         }
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
      }
      
      private function pushEnter() : void
      {
         if(!this.decideMysetMenu())
         {
            return;
         }
         this.m_menu_list.PushButton();
         this.m_callback.CallbackSe(this.m_callback.SeTypeDecide);
      }
      
      private function pushCancel() : void
      {
         if(!this.cancelMysetMenu())
         {
            return;
         }
         this.m_callback.CallbackSe(this.m_callback.SeTypeCancel);
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
            case Keyboard.ENTER:
               this.pushEnter();
               break;
            case Keyboard.ESCAPE:
               this.pushCancel();
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
      
      public function CallQuitMenu() : void
      {
         if(this.m_is_start_wait && !this.m_is_request_quit_menu)
         {
            this.endMyset(false);
            this.m_is_request_quit_menu = true;
         }
      }
      
      public function TestDestroy() : void
      {
         this.m_callback = null;
         this.m_menu_list.ClearButton();
         this.m_menu_list = null;
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStart);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEnd);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitCompDecideMysetList);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEndCancelMysetList);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitCompDecideCustomizeList);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEndDecideCustomizeList);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStandbyMysetMenu);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitSetupEquipList);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitMenuItem);
         this.m_timeline.visible = false;
         this.m_timeline = null;
      }
      
      public function TestCheckUserDataValidFlag(param1:int) : Boolean
      {
         return this.m_callback.GetUserDataValidFlag(param1);
      }
      
      public function TestGetMenuSelectData() : int
      {
         return this.m_menu_list.GetSelectUserData();
      }
      
      public function TestGetEditMysetID() : int
      {
         if(this.m_timeline.menulist.visible)
         {
            return this.TestGetMenuSelectData();
         }
         return 0;
      }
      
      public function TestGetCustomizeType() : int
      {
         return this.m_customize_type;
      }
   }
}

