// AVATAR_SLOT.iggy's action_script.AvatarSlot (the CaC save slot list),
// decompiled by FFDec, with SaveSync's toggle column added: a "SaveSync"
// header and an ON / OFF toggle right of every slot, empty ones included.
// RIGHT moves into the column, LEFT / ESC back to the slots, ENTER toggles
// the selected slot's row; UP / DOWN move between slots as before.
// Everything added is marked "SaveSync". Built into
// SaveSync\src\avatarslot_abc.h by ClaudeTools\iggy\menu_abc.py (menu
// ss_avatarslot).
//
// Talks to SaveSync.dll through the menu's AS callback (sendTypes the game
// ignores): 9910 asks for the toggles, 9911 toggles slot `value`, 9912
// sends a line for the mod's log, 9913 says slot `value` was picked to
// load (no answer). The answer comes back in user data 2
// (ReceiveType_ResultDeleteSlot, which the script reads only while a delete
// waits): 0x10000 | the bit mask of the toggled slots.
package action_script
{
   import flash.display.DisplayObject;
   import flash.display.MovieClip;
   import flash.events.Event;
   import flash.events.KeyboardEvent;
   import flash.filters.GlowFilter;
   import flash.geom.Rectangle;
   import flash.text.TextField;
   import flash.ui.Keyboard;

   public class AvatarSlot
   {

      // SaveSync: the messages and the answer (see the top of the file)
      public static const SendType_SaveSyncGetToggles:int = 9910;

      public static const SendType_SaveSyncToggle:int = 9911;

      public static const SendType_SaveSyncLog:int = 9912;

      public static const SendType_SaveSyncLoad:int = 9913;

      public static const SaveSyncAnswerTag:int = 65536;

      public static const SaveSyncHeaderText:String = "SaveSync";

      public static const SaveSyncOnColor:int = 16762880;

      public static const SaveSyncOffColor:int = 10132122;

      // frames the menu must stay fully faded in before the column is made:
      // text fields made during the fade-in never drew (Chara Search's stars)
      public static const SaveSyncSettleFrames:int = 3;

      // the gap between the column and the slot buttons on its right, in the buttons' space
      public static const SaveSyncGap:Number = 24;

      // a black outline around the column's text so it reads over the
      // bright backdrop (same glow as General QoL's lobby map labels)
      public static const SaveSyncOutlineBlur:Number = 3;

      public static const SaveSyncOutlineStrength:Number = 4;

      public static const SlotElemMax:int = 8;
      
      public static const ReceiveType_ModeType:int = 0;
      
      public static const ReceiveType_LastSlotID:int = 1;
      
      public static const ReceiveType_ResultDeleteSlot:int = 2;
      
      public static const ReceiveType_SlotNameStart:int = 3;
      
      public static const ReceiveType_SlotNameEnd:int = ReceiveType_SlotNameStart + SlotElemMax - 1;
      
      public static const ReceiveType_SlotLevelStart:int = ReceiveType_SlotNameEnd + 1;
      
      public static const ReceiveType_SlotLevelEnd:int = ReceiveType_SlotLevelStart + SlotElemMax - 1;
      
      public static const ReceiveType_SelectSlotValidity:int = ReceiveType_SlotLevelEnd + 1;
      
      public static const ReceiveType_Num:int = ReceiveType_SelectSlotValidity + 1;
      
      public static const SendType_NotifyChangeSlot:int = 0;
      
      public static const SendType_RequestDeleteSlot:int = 1;
      
      public static const ModeType_Limited:int = 0;
      
      public static const ModeType_Unlocked:int = 1;
      
      private static const StringList:Object = {
         "SlotLocked":"ASL_STR_00_000",
         "SlotNoData":"ASL_STR_01_001",
         "HeaderLevel":"ASL_STR_00_001",
         "LevelLocked":"",
         "LevelNoData":"",
         "Invalid":null
      };
      
      private var m_callback:Callback = null;
      
      private var m_button_list:ButtonList = null;
      
      private var m_timeline:MovieClip = null;

      // SaveSync state
      private var m_ss_mask:int = 0;

      private var m_ss_known:Boolean = false;

      private var m_ss_waiting:Boolean = false;

      private var m_ss_in_column:Boolean = false;

      private var m_ss_parent:MovieClip = null;

      private var m_ss_fields:Array = null;

      private var m_ss_header:TextField = null;

      // the column's right edge (in the buttons' space): its text is
      // right-aligned so it stays next to the list
      private var m_ss_right:Number = 0;

      private var m_ss_settled_frames:int = 0;

      private var m_ss_dumped:Boolean = false;

      public function AvatarSlot()
      {
         super();
         this.m_callback = new Callback(ReceiveType_Num);
         this.m_button_list = new ButtonList(SlotElemMax);
         this.m_timeline = null;
      }
      
      public function Initialize(param1:MovieClip) : void
      {
         this.m_timeline = param1;
         this.m_timeline.visible = false;
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitStart);
      }
      
      private function waitStart(param1:Event) : void
      {
         if(!this.checkReceiveSlotMenu())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStart);
         this.Start();
      }
      
      public function Start() : void
      {
         this.m_timeline.visible = true;
         this.startSlotMenu();
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.saveSyncPoll);
         this.saveSyncRequest(SendType_SaveSyncGetToggles,0);
      }

      private function endMenu(param1:Boolean) : void
      {
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.saveSyncPoll);
         this.m_ss_in_column = false;
         if(param1)
         {
            // SaveSync: the picked slot is synced before the game loads it
            this.m_callback.CallbackUserData("user",SendType_SaveSyncLoad,this.m_button_list.GetSelectUserData());
            this.m_callback.CallbackDecide(this.m_button_list.GetSelectUserData());
         }
         else
         {
            this.m_callback.CallbackCancel();
         }
         this.endSlotMenu();
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitEnd);
      }
      
      private function waitEnd(param1:Event) : void
      {
         if(!this.isEndSlotMenu())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEnd);
         this.m_callback.CallbackExit();
      }
      
      private function checkReceiveSlotMenu() : Boolean
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ModeType))
         {
            return false;
         }
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_LastSlotID))
         {
            return false;
         }
         return true;
      }
      
      private function startSlotMenu() : void
      {
         this.initSlotMenuList();
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_LastSlotID);
         if(_loc1_ != this.m_button_list.GetUserData(_loc1_))
         {
            _loc1_ = 0;
         }
         this.m_button_list.ChangeButton(_loc1_);
         this.m_timeline.menu_family.gotoAndPlay("start");
         this.notifyChangeSlot();
      }
      
      private function endSlotMenu() : void
      {
         this.m_timeline.menu_family.gotoAndPlay("end");
      }
      
      private function isEndSlotMenu() : Boolean
      {
         if(this.m_timeline.menu_family.currentLabel != "end_comp")
         {
            return false;
         }
         return true;
      }
      
      private function initSlotMenuList() : void
      {
         var _loc2_:MovieClip = null;
         var _loc3_:Boolean = false;
         var _loc4_:String = null;
         var _loc5_:String = null;
         this.m_button_list.ClearButton();
         var _loc1_:int = 0;
         while(_loc1_ < SlotElemMax)
         {
            _loc2_ = this.m_timeline.menu_family.contents["btn" + (_loc1_ + 1)];
            if(_loc2_)
            {
               _loc3_ = true;
               _loc4_ = StringList.SlotNoData;
               _loc5_ = StringList.LevelNoData;
               if(this.m_callback.GetUserDataValidFlag(ReceiveType_SlotNameStart + _loc1_) && this.m_callback.GetUserDataValidFlag(ReceiveType_SlotLevelStart + _loc1_))
               {
                  _loc4_ = this.m_callback.GetUserDataString(ReceiveType_SlotNameStart + _loc1_);
                  _loc2_.sys_lv.sys.text = StringList.HeaderLevel;
                  _loc5_ = _loc2_.sys_lv.sys.text + this.m_callback.GetUserDataInt(ReceiveType_SlotLevelStart + _loc1_);
               }
               if(this.m_callback.GetUserDataInt(ReceiveType_ModeType) == ModeType_Limited && 0 < _loc1_)
               {
                  _loc3_ = false;
                  _loc4_ = StringList.SlotLocked;
                  _loc5_ = StringList.LevelLocked;
               }
               _loc2_.sys_family.sys.text = _loc4_;
               Utility.SetAutoSizedhtmlText(_loc2_.sys_lv.sys,_loc5_);
               _loc2_.gotoAndPlay("off");
               if(_loc3_)
               {
                  this.m_button_list.AddButton(_loc2_,_loc1_);
               }
               else
               {
                  _loc2_.gotoAndPlay("inv");
               }
            }
            _loc1_++;
         }
      }
      
      private function changeSlotMenuSelect(param1:int) : Boolean
      {
         var _loc2_:int = this.m_button_list.GetSelectButton();
         if(0 < param1)
         {
            this.m_button_list.SelectNextButton();
         }
         else if(param1 < 0)
         {
            this.m_button_list.SelectPrevButton();
         }
         if(_loc2_ == this.m_button_list.GetSelectButton())
         {
            return false;
         }
         this.notifyChangeSlot();
         return true;
      }
      
      private function notifyChangeSlot() : void
      {
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_callback.SetUserDataValidFlag(ReceiveType_SelectSlotValidity,false);
         this.m_callback.CallbackUserData("user",SendType_NotifyChangeSlot,this.m_button_list.GetSelectUserData());
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitReceiveSlotValidity);
      }
      
      private function waitReceiveSlotValidity(param1:Event) : void
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_SelectSlotValidity))
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitReceiveSlotValidity);
         this.m_timeline.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
      }
      
      private function requestDeleteSlot() : Boolean
      {
         if(!this.m_callback.GetUserDataInt(ReceiveType_SelectSlotValidity))
         {
            return false;
         }
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_callback.SetUserDataValidFlag(ReceiveType_ResultDeleteSlot,false);
         var _loc1_:int = 0;
         while(_loc1_ < SlotElemMax)
         {
            this.m_callback.SetUserDataValidFlag(ReceiveType_SlotNameStart + _loc1_,false);
            this.m_callback.SetUserDataValidFlag(ReceiveType_SlotLevelStart + _loc1_,false);
            _loc1_++;
         }
         this.m_callback.CallbackUserData("user",SendType_RequestDeleteSlot,this.m_button_list.GetSelectUserData());
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitReceiveResultDeleteSlot);
         return true;
      }
      
      private function waitReceiveResultDeleteSlot(param1:Event) : void
      {
         var _loc2_:int = 0;
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ResultDeleteSlot))
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitReceiveResultDeleteSlot);
         if(this.m_callback.GetUserDataInt(ReceiveType_ResultDeleteSlot))
         {
            _loc2_ = this.m_button_list.GetSelectUserData();
            this.initSlotMenuList();
            this.m_button_list.ChangeButton(_loc2_);
            this.notifyChangeSlot();
         }
         this.m_timeline.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
      }
      
      private function pushU() : void
      {
         if(this.changeSlotMenuSelect(-1))
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
         }
      }
      
      private function pushD() : void
      {
         if(this.changeSlotMenuSelect(1))
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
         }
      }
      
      private function pushCancel() : void
      {
         this.endMenu(false);
         this.m_callback.CallbackSe(this.m_callback.SeTypeCancel);
      }
      
      private function pushEnter() : void
      {
         this.endMenu(true);
         this.m_callback.CallbackSe(this.m_callback.SeTypeDecide);
      }
      
      private function pushX() : void
      {
         if(this.requestDeleteSlot())
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeDecide);
         }
         else
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeNg);
         }
      }
      
      // SaveSync: asks the mod for the toggles (9910) or to flip one (9911);
      // the answer arrives in user data 2 a frame or so later (saveSyncPoll).
      private function saveSyncRequest(sendType:int, value:int) : void
      {
         this.m_callback.SetUserDataValidFlag(ReceiveType_ResultDeleteSlot,false);
         this.m_ss_waiting = true;
         this.m_callback.CallbackUserData("user",sendType,value);
      }

      private function saveSyncLog(text:String) : void
      {
         this.m_callback.CallbackUserDataString("user",SendType_SaveSyncLog,text);
      }

      // SaveSync: the slot's button, or null
      private function saveSyncButton(slot:int) : MovieClip
      {
         var contents:MovieClip = this.m_timeline.menu_family.contents;
         return contents != null ? contents["btn" + (slot + 1)] as MovieClip : null;
      }

      // SaveSync: whether the menu and everything above it are fully faded in
      private function saveSyncFadedIn(clip:DisplayObject) : Boolean
      {
         while(clip != null)
         {
            if(clip.alpha < 0.99 || !clip.visible)
            {
               return false;
            }
            clip = clip.parent;
         }
         return true;
      }

      // SaveSync, every frame while the list is open: only flag checks,
      // apart from making the column once (again if the timeline swapped
      // the buttons' clip).
      private function saveSyncPoll(event:Event) : void
      {
         if(this.m_ss_waiting && this.m_callback.GetUserDataValidFlag(ReceiveType_ResultDeleteSlot))
         {
            var answer:int = this.m_callback.GetUserDataInt(ReceiveType_ResultDeleteSlot);
            if((answer & SaveSyncAnswerTag) != 0)
            {
               this.m_ss_waiting = false;
               this.m_ss_known = true;
               this.m_ss_mask = answer & 255;
               this.m_callback.SetUserDataValidFlag(ReceiveType_ResultDeleteSlot,false);
               this.saveSyncDraw();
            }
         }
         var contents:MovieClip = this.m_timeline.menu_family.contents;
         if(contents == null)
         {
            return;
         }
         if(contents != this.m_ss_parent)
         {
            if(!this.saveSyncFadedIn(contents))
            {
               this.m_ss_settled_frames = 0;
               return;
            }
            this.m_ss_settled_frames++;
            if(this.m_ss_settled_frames < SaveSyncSettleFrames)
            {
               return;
            }
            this.saveSyncMakeColumn(contents);
         }
      }

      // SaveSync: a blank text field in `source`'s font (Iggy draws only
      // fonts the movie already uses) - Chara Search's copyNameField.
      private static function saveSyncCopyField(source:TextField) : TextField
      {
         var field:TextField = new TextField();
         field.defaultTextFormat = source.getTextFormat();
         field.embedFonts = source.embedFonts;
         field.selectable = false;
         field.mouseEnabled = false;
         field.multiline = false;
         field.wordWrap = false;
         field.filters = source.filters;
         field.alpha = 1;
         return field;
      }

      // SaveSync: one text field of the column, in the slot name's font at
      // the level's size, scaled like the level text is on screen.
      private function saveSyncMakeField(contents:MovieClip, button:MovieClip) : TextField
      {
         var field:TextField = saveSyncCopyField(button.sys_family.sys);
         var levelField:TextField = button.sys_lv.sys;
         var format:* = field.defaultTextFormat;
         format.size = levelField.getTextFormat().size;
         format.align = "left";
         format.leading = 0;
         field.defaultTextFormat = format;
         field.autoSize = "left";
         var levelBounds:Rectangle = levelField.getBounds(contents);
         var levelLocalHeight:Number = levelField.scaleY != 0 ? levelField.height / Math.abs(levelField.scaleY) : 0;
         var scale:Number = levelLocalHeight > 0 ? levelBounds.height / levelLocalHeight : 1;
         field.scaleX = scale;
         field.scaleY = scale;
         field.filters = [new GlowFilter(0,1,SaveSyncOutlineBlur,SaveSyncOutlineBlur,SaveSyncOutlineStrength)];
         contents.addChild(field);
         return field;
      }

      // SaveSync: the header and one toggle per slot, left of the list and
      // right-aligned to it, each centred on its button's row.
      private function saveSyncMakeColumn(contents:MovieClip) : void
      {
         this.m_ss_parent = contents;
         try
         {
            if(this.m_ss_fields != null)
            {
               for each(var old:TextField in this.m_ss_fields)
               {
                  if(old != null && old.parent != null)
                  {
                     old.parent.removeChild(old);
                  }
               }
            }
            if(this.m_ss_header != null && this.m_ss_header.parent != null)
            {
               this.m_ss_header.parent.removeChild(this.m_ss_header);
            }
            this.m_ss_fields = new Array(SlotElemMax);
            this.m_ss_header = null;
            // left of the list: the stats popup covers the space on its right
            var left:Number = 100000;
            var slot:int = 0;
            while(slot < SlotElemMax)
            {
               var button:MovieClip = this.saveSyncButton(slot);
               if(button != null)
               {
                  left = Math.min(left,button.getBounds(contents).left);
               }
               slot++;
            }
            this.m_ss_right = left - SaveSyncGap;
            var dump:String = "layout: contents alpha " + contents.alpha;
            slot = 0;
            while(slot < SlotElemMax)
            {
               button = this.saveSyncButton(slot);
               if(button != null)
               {
                  var bounds:Rectangle = button.getBounds(contents);
                  var field:TextField = this.saveSyncMakeField(contents,button);
                  field.text = "> OFF";
                  if(this.m_ss_header == null)
                  {
                     this.m_ss_header = this.saveSyncMakeField(contents,button);
                     this.m_ss_header.textColor = 16777215;
                     this.m_ss_header.text = SaveSyncHeaderText;
                     this.m_ss_header.x = this.m_ss_right - this.m_ss_header.width;
                     this.m_ss_header.y = bounds.y - this.m_ss_header.height - 4;
                     dump += "; field " + Math.round(field.width) + "x" + Math.round(field.height) + " scale " + field.scaleX + " size " + field.defaultTextFormat.size + " font " + field.defaultTextFormat.font;
                  }
                  field.x = this.m_ss_right - field.width;
                  field.y = bounds.y + (bounds.height - field.height) / 2;
                  this.m_ss_fields[slot] = field;
                  dump += "; btn" + (slot + 1) + " " + Math.round(bounds.x) + "," + Math.round(bounds.y) + " " + Math.round(bounds.width) + "x" + Math.round(bounds.height);
               }
               slot++;
            }
            if(!this.m_ss_dumped)
            {
               this.m_ss_dumped = true;
               this.saveSyncLog(dump);
            }
            this.saveSyncDraw();
         }
         catch(e:Error)
         {
            this.saveSyncLog("column error: " + e.toString());
         }
      }

      // SaveSync: ON (gold) / OFF (grey) per slot, "> " on the selected
      // slot's toggle while the column has the focus; hidden for slots the
      // game shows as locked, and until the mod has answered.
      private function saveSyncDraw() : void
      {
         if(this.m_ss_fields == null)
         {
            return;
         }
         var limited:Boolean = this.m_callback.GetUserDataInt(ReceiveType_ModeType) == ModeType_Limited;
         var selected:int = this.m_button_list.GetSelectUserData();
         var slot:int = 0;
         while(slot < SlotElemMax)
         {
            var field:TextField = this.m_ss_fields[slot];
            if(field != null)
            {
               var on:Boolean = (this.m_ss_mask & 1 << slot) != 0;
               field.visible = this.m_ss_known && !(limited && 0 < slot);
               field.text = (this.m_ss_in_column && slot == selected ? "> " : "") + (on ? "ON" : "OFF");
               field.textColor = on ? SaveSyncOnColor : SaveSyncOffColor;
               // autoSize changed the width: keep the right edge at the list
               field.x = this.m_ss_right - field.width;
            }
            slot++;
         }
         if(this.m_ss_header != null)
         {
            this.m_ss_header.visible = this.m_ss_known;
         }
      }

      // SaveSync: the keys while the column has the focus; false = not
      // handled here (UP / DOWN go on to the slot list).
      private function saveSyncKey(keyCode:int) : Boolean
      {
         if(!this.m_ss_in_column)
         {
            if(keyCode == Keyboard.LEFT && this.m_ss_known && this.m_ss_fields != null)
            {
               this.saveSyncFocusColumn(true);
               this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
               return true;
            }
            return false;
         }
         switch(keyCode)
         {
            case Keyboard.RIGHT:
               this.saveSyncFocusColumn(false);
               this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
               return true;
            case Keyboard.ESCAPE:
               this.saveSyncFocusColumn(false);
               this.m_callback.CallbackSe(this.m_callback.SeTypeCancel);
               return true;
            case Keyboard.UP:
            case Keyboard.DOWN:
               // the slot list moves as usual (the stats follow); the arrow
               // follows and the newly selected row's highlight goes off again
               if(keyCode == Keyboard.UP)
               {
                  this.pushU();
               }
               else
               {
                  this.pushD();
               }
               this.saveSyncFocusColumn(true);
               return true;
            case Keyboard.ENTER:
               if(this.m_ss_waiting)
               {
                  return true;
               }
               this.saveSyncRequest(SendType_SaveSyncToggle,this.m_button_list.GetSelectUserData());
               this.m_callback.CallbackSe(this.m_callback.SeTypeDecide);
               return true;
            case 88:
               this.m_callback.CallbackSe(this.m_callback.SeTypeNg);
               return true;
            case Keyboard.LEFT:
               return true;
            default:
               return false;
         }
      }

      // SaveSync: moves the cursor into the column (the selected slot row
      // shows unselected, "off", so ENTER reads as the toggle's) or back to
      // the row ("on", the label the list plays on its selected button).
      private function saveSyncFocusColumn(inColumn:Boolean) : void
      {
         this.m_ss_in_column = inColumn;
         var button:MovieClip = this.saveSyncButton(this.m_button_list.GetSelectUserData());
         try
         {
            if(button != null)
            {
               button.gotoAndPlay(inColumn ? "off" : "on");
            }
         }
         catch(e:Error)
         {
            this.saveSyncLog("row highlight: " + e.toString());
         }
         this.saveSyncDraw();
      }

      private function checkKey(param1:KeyboardEvent) : void
      {
         if(this.saveSyncKey(param1.keyCode))
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
            case Keyboard.ESCAPE:
               this.pushCancel();
               break;
            case Keyboard.ENTER:
               this.pushEnter();
               break;
            case 88:
               this.pushX();
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
         if(this.m_timeline && this.m_timeline.stage)
         {
            this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.saveSyncPoll);
         }
         this.m_callback = null;
         this.m_button_list.ClearButton();
         this.m_button_list = null;
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStart);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitReceiveSlotValidity);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitReceiveResultDeleteSlot);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEnd);
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
      
      public function TestGetSelectSlotID() : int
      {
         return this.m_button_list.GetSelectUserData();
      }
   }
}

