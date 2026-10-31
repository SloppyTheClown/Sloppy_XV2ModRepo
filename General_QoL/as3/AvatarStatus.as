// Customize > Attributes (AVATAR_FACULTY.iggy), shared by General_QoL and
// Combined Supers: one script, each mod's part switched on by its character
// in FeatureFlags (Research_Notes\ui-iggy-and-textures.md "Attributes
// screen: one script, two mods"). Built by ClaudeTools\iggy\menu_abc.py
// qol_faculty into both mods' avatar_faculty_abc.h.
package action_script
{
   import flash.display.DisplayObject;
   import flash.display.MovieClip;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.events.KeyboardEvent;
   import flash.filters.GlowFilter;
   import flash.geom.Rectangle;
   import flash.text.TextField;
   import flash.ui.Keyboard;

   public class AvatarStatus
   {

      public static const ParamElemNum:int = 6;

      public static const DecideEffectEndFrame:int = 25;

      public static const ReceiveType_RequestEnd:int = 0;

      public static const ReceiveType_RequestDecide:int = 1;

      public static const ReceiveType_RequestReset:int = 2;

      public static const ReceiveType_StatusPoint:int = 3;

      public static const ReceiveType_ParamDataMax:int = 4;

      public static const ReceiveType_ParamDataStart:int = 5;

      public static const ReceiveType_ParamDataEnd:int = ReceiveType_ParamDataStart + ParamElemNum - 1;

      public static const ReceiveType_Num:int = ReceiveType_ParamDataEnd + 1;

      public static const SendType_RequestConfirm:int = 0;

      public static const SendType_SelectParamIndex:int = 1;

      public static const SendType_StatusPoint:int = 2;

      public static const SendType_ParamDataStart:int = 3;

      public static const SendType_ParamDataEnd:int = SendType_ParamDataStart + ParamElemNum - 1;

      public static const SendType_DecideEffSePlay:int = SendType_ParamDataEnd + 1;

      public static const ConfirmType_End:int = 0;

      public static const ConfirmType_Decide:int = 1;

      public static const ConfirmType_Reset:int = 2;

      private static const Frame_ParamColorNoEdit:int = 1;

      private static const Frame_ParamColorNoEditMax1:int = 2;

      private static const Frame_ParamColorEditing:int = 3;

      private static const Frame_ParamColorEditingMax1:int = 4;

      private static const Frame_ParamColorNoEditMax2:int = 5;

      private static const Frame_ParamColorEditingMax2:int = 6;

      private static const NormalParamMax:int = 100;

      private static const LimitBreak1Max:int = 125;

      // Which mods' parts are on. The serving DLL rewrites the two characters
      // after the colon in place (same length, so the ABC keeps its layout):
      //   [FlagFreePoints]  General_QoL free_attribute_points: '1' on, '0' off
      //   [FlagSupers]      Combined Supers "stats": 'a' additive (total = sum),
      //                     'h' highest (total = the higher one), '0' off
      // Each DLL looks for "XV2FACULTY:" in the ABC (General_QoL's
      // iggy_abc.cpp, Combined Supers' menu_scripts.cpp).
      private static const FeatureFlags:String = "XV2FACULTY:00";

      private static const FlagFreePoints:int = 11;

      private static const FlagSupers:int = 12;

      // Combined Supers: rows 5 and 6 (0-based 4, 5) are Strike Supers and
      // Ki Blast Supers (StringList StatusParam5/6, chart icon frames 5/6).
      private static const SupersStrikeIndex:int = 4;

      private static const SupersKiIndex:int = 5;

      private static const SupersLabel:String = "Supers";

      // A black backdrop over both rows (names, values, selection bar) under
      // the label and total, so the combined total stands out.
      private static const SupersBackdropAlpha:Number = 0.55;

      // Gold trim: a dark outline under a gold line under a pale inner line.
      private static const SupersGold:uint = 0xD8A838;

      private static const SupersGoldLight:uint = 0xFFE7A0;

      private static const SupersShadow:uint = 0x1A1206;

      // Label and total: white with a black outline (GlowFilter, the same
      // as SaveSync's toggle labels).
      private static const SupersTextColor:uint = 0xFFFFFF;

      private static const SupersOutlineBlur:Number = 3;

      private static const SupersOutlineStrength:Number = 4;

      // Gap between the total and the rows' value pills, and between the
      // label and the total (list units, by eye).
      private static const SupersTextGap:Number = 14;

      // The box is laid out once the menu's start animation has stopped
      // moving the rows: their bounds the same for this many frames in a row
      // (at most SupersSettleMaxFrames frames, then laid out anyway).
      private static const SupersSettleFrames:int = 3;

      private static const SupersSettleMaxFrames:int = 90;

      private static const StringList:Object = {
         "StatusParam1":"ACS_STR_00_000",
         "StatusParam2":"ACS_STR_00_001",
         "StatusParam3":"ACS_STR_00_002",
         "StatusParam4":"ACS_STR_00_003",
         "StatusParam5":"ACS_STR_00_004",
         "StatusParam6":"ACS_STR_00_005",
         "StatusPoint":"ACS_STR_02_000",
         "Invalid":null
      };

      private var m_callback:Callback = null;

      private var m_button_list:ButtonList = null;

      private var m_param_base_array:Array;

      private var m_param_edit_array:Array;

      private var m_timeline:MovieClip = null;

      private var m_is_limit_break1:Boolean = false;

      private var m_is_limit_break2:Boolean = false;

      // General_QoL: confirmed points can be taken back out (floor 0).
      private var m_free_points:Boolean = false;

      // Combined Supers: '0' off, 'a' additive, 'h' highest (FeatureFlags).
      private var m_supers_mode:String = "0";

      private var m_supers_box:Sprite = null;

      private var m_supers_label:TextField = null;

      private var m_supers_total:TextField = null;

      private var m_supers_last_bounds:Rectangle = null;

      private var m_supers_stable_frames:int = 0;

      private var m_supers_wait_frames:int = 0;

      public function AvatarStatus()
      {
         super();
         this.m_callback = new Callback(ReceiveType_Num);
         this.m_button_list = new ButtonList(ParamElemNum);
         this.m_param_base_array = new Array(ParamElemNum);
         this.m_param_edit_array = new Array(ParamElemNum);
         this.m_timeline = null;
         this.m_is_limit_break1 = false;
         this.m_is_limit_break2 = false;
         this.m_free_points = FeatureFlags.charAt(FlagFreePoints) == "1";
         this.m_supers_mode = FeatureFlags.charAt(FlagSupers);
         if(this.m_supers_mode != "a" && this.m_supers_mode != "h")
         {
            this.m_supers_mode = "0";
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
         if(!this.checkReceiveStatus())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStart);
         this.Start();
      }

      public function Start() : void
      {
         this.m_timeline.visible = true;
         this.startStatus();
         this.m_timeline.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
      }

      private function requestDecide() : Boolean
      {
         if(this.isStatusChange())
         {
            this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
            this.m_callback.SetUserDataValidFlag(ReceiveType_RequestDecide,false);
            this.m_callback.CallbackUserData("user",SendType_RequestConfirm,ConfirmType_Decide);
            this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitDecideRequest);
            return true;
         }
         return false;
      }

      private function waitDecideRequest(param1:Event) : void
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_RequestDecide))
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitDecideRequest);
         if(this.m_callback.GetUserDataInt(ReceiveType_RequestDecide))
         {
            this.requestDecideEffectStart();
         }
         else
         {
            this.m_timeline.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         }
      }

      private function requestDecideEffectStart() : void
      {
         var _loc2_:MovieClip = null;
         var _loc1_:uint = 0;
         while(_loc1_ < ParamElemNum)
         {
            if(this.isEditorParamChanged(_loc1_))
            {
               _loc2_ = this.m_button_list.GetButton(_loc1_);
               if(_loc2_)
               {
                  _loc2_.ef_decide.visible = true;
                  _loc2_.ef_decide.gotoAndPlay("start");
               }
            }
            _loc1_++;
         }
         this.m_callback.CallbackUserData("user",SendType_DecideEffSePlay,0);
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitDecideEffectEnd);
      }

      private function waitDecideEffectEnd(param1:Event) : void
      {
         var _loc4_:MovieClip = null;
         var _loc2_:Boolean = true;
         var _loc3_:uint = 0;
         while(_loc3_ < ParamElemNum)
         {
            if(this.isEditorParamChanged(_loc3_))
            {
               _loc4_ = this.m_button_list.GetButton(_loc3_);
               if(_loc4_)
               {
                  if(_loc4_.ef_decide.currentFrame != DecideEffectEndFrame)
                  {
                     _loc2_ = false;
                  }
                  else
                  {
                     _loc4_.ef_decide.visible = false;
                  }
               }
            }
            _loc3_++;
         }
         if(_loc2_)
         {
            if(this.m_callback.GetUserDataInt(ReceiveType_RequestDecide))
            {
               this.decideStatusParam();
            }
            this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitDecideEffectEnd);
            this.m_timeline.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         }
      }

      private function requestReset() : Boolean
      {
         if(this.isStatusChange())
         {
            this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
            this.m_callback.SetUserDataValidFlag(ReceiveType_RequestReset,false);
            this.m_callback.CallbackUserData("user",SendType_RequestConfirm,ConfirmType_Reset);
            this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitResetRequest);
            return true;
         }
         return false;
      }

      private function waitResetRequest(param1:Event) : void
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_RequestReset))
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitResetRequest);
         if(this.m_callback.GetUserDataInt(ReceiveType_RequestReset))
         {
            this.resetStatusParam();
         }
         this.m_timeline.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
      }

      private function requestEnd() : void
      {
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         if(this.isStatusChange())
         {
            this.m_callback.SetUserDataValidFlag(ReceiveType_RequestEnd,false);
            this.m_callback.CallbackUserData("user",SendType_RequestConfirm,ConfirmType_End);
            this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitEndRequest);
         }
         else
         {
            this.endMenu();
         }
      }

      private function waitEndRequest(param1:Event) : void
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_RequestEnd))
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEndRequest);
         if(!this.m_callback.GetUserDataInt(ReceiveType_RequestEnd))
         {
            this.m_timeline.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
            return;
         }
         this.endMenu();
      }

      private function endMenu() : void
      {
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_callback.CallbackCancel();
         this.endStatus();
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitEnd);
      }

      private function waitEnd(param1:Event) : void
      {
         if(!this.isEndStatus())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEnd);
         this.supersStop();
         this.m_callback.CallbackExit();
      }

      private function checkReceiveStatus() : Boolean
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ParamDataMax))
         {
            return false;
         }
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ParamDataMax);
         this.m_is_limit_break1 = _loc1_ > NormalParamMax;
         this.m_is_limit_break2 = _loc1_ > LimitBreak1Max;
         var _loc2_:int = 0;
         while(_loc2_ < ParamElemNum)
         {
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ParamDataStart + _loc2_))
            {
               return false;
            }
            _loc2_++;
         }
         return true;
      }

      private function startStatus() : void
      {
         this.initEditor();
         this.startEditor();
      }

      private function endStatus() : void
      {
         this.endEditor();
      }

      private function isEndStatus() : Boolean
      {
         if(!this.isEndEditor())
         {
            return false;
         }
         return true;
      }

      private function changeStatusSelect(param1:int) : void
      {
         var _loc2_:int = 0;
         _loc2_ = this.m_button_list.GetSelectButton();
         this.updateEditorButton(_loc2_);
         if(0 < param1)
         {
            this.m_button_list.SelectNextButton();
         }
         else if(param1 < 0)
         {
            this.m_button_list.SelectPrevButton();
         }
         _loc2_ = this.m_button_list.GetSelectButton();
         this.updateEditorButton(_loc2_);
         this.supersUpdate();
         this.m_callback.CallbackUserData("user",SendType_SelectParamIndex,_loc2_);
      }

      private function changeStatusParam(param1:int) : Boolean
      {
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveType_StatusPoint);
         if(_loc2_ == 0 && 0 < param1)
         {
            return false;
         }
         var _loc3_:int = this.m_button_list.GetSelectButton();
         var _loc4_:int = ReceiveType_ParamDataStart + _loc3_;
         var _loc5_:int = this.m_callback.GetUserDataInt(_loc4_) + param1;
         // General_QoL: the floor is 0, not the confirmed value, so points spent
         // and confirmed earlier can be taken back out.
         if(_loc5_ < this.editorParamFloor(_loc3_) || this.m_callback.GetUserDataInt(ReceiveType_ParamDataMax) < _loc5_)
         {
            return false;
         }
         this.m_param_edit_array[_loc3_] = _loc5_;
         _loc2_ -= param1;
         this.SetUserDataInt(_loc4_,_loc5_);
         this.SetUserDataInt(ReceiveType_StatusPoint,_loc2_);
         this.m_callback.CallbackUserData("user",SendType_ParamDataStart + _loc3_,_loc5_);
         this.m_callback.CallbackUserData("user",SendType_StatusPoint,_loc2_);
         this.updateEditorButton(_loc3_,param1);
         return true;
      }

      private function isStatusChange() : Boolean
      {
         var _loc1_:int = 0;
         while(_loc1_ < this.m_param_edit_array.length)
         {
            if(this.isEditorParamChanged(_loc1_))
            {
               return true;
            }
            _loc1_++;
         }
         return false;
      }

      private function decideStatusParam() : void
      {
         var _loc2_:int = 0;
         var _loc3_:int = 0;
         var _loc1_:int = 0;
         while(_loc1_ < this.m_param_base_array.length)
         {
            _loc2_ = this.m_param_edit_array[_loc1_] - this.m_param_base_array[_loc1_];
            if(_loc2_ != 0)
            {
               this.m_param_base_array[_loc1_] = this.m_param_edit_array[_loc1_];
               _loc3_ = ReceiveType_ParamDataStart + _loc1_;
               this.SetUserDataInt(_loc3_,this.m_param_edit_array[_loc1_]);
               this.updateEditorButton(_loc1_);
            }
            _loc1_++;
         }
      }

      private function resetStatusParam() : void
      {
         var _loc3_:int = 0;
         var _loc4_:int = 0;
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_StatusPoint);
         var _loc2_:int = 0;
         while(_loc2_ < this.m_param_edit_array.length)
         {
            _loc3_ = this.m_param_edit_array[_loc2_] - this.m_param_base_array[_loc2_];
            if(_loc3_ != 0)
            {
               _loc1_ += _loc3_;
               this.m_param_edit_array[_loc2_] = this.m_param_base_array[_loc2_];
               _loc4_ = ReceiveType_ParamDataStart + _loc2_;
               this.SetUserDataInt(_loc4_,this.m_param_edit_array[_loc2_]);
               this.SetUserDataInt(ReceiveType_StatusPoint,_loc1_);
               this.updateEditorButton(_loc2_);
            }
            _loc2_++;
         }
      }

      private function getStringColorFrame(param1:Boolean, param2:Boolean, param3:Boolean) : int
      {
         if(param1)
         {
            if(param3)
            {
               trace("editting lm2");
               return Frame_ParamColorEditingMax2;
            }
            if(param2)
            {
               trace("editting lm1");
               return Frame_ParamColorEditingMax1;
            }
            trace("editting no lm");
            return Frame_ParamColorEditing;
         }
         if(param3)
         {
            trace("no editting lm2");
            return Frame_ParamColorNoEditMax2;
         }
         if(param2)
         {
            trace("no editting lm1");
            return Frame_ParamColorNoEditMax1;
         }
         trace("no editting no lm");
         return Frame_ParamColorNoEdit;
      }

      private function initEditor() : void
      {
         var _loc4_:int = 0;
         var _loc5_:MovieClip = null;
         var _loc6_:Boolean = false;
         var _loc7_:Boolean = false;
         var _loc1_:MovieClip = this.m_timeline.menu_edit.stacusmenu;
         var _loc2_:int = this.m_callback.GetUserDataInt(ReceiveType_StatusPoint);
         _loc1_.totalpoint.sys.text = StringList.StatusPoint;
         _loc1_.totalpoint.sys_pt.text = _loc2_;
         this.m_button_list.ClearButton();
         var _loc3_:int = 0;
         while(_loc3_ < ParamElemNum)
         {
            _loc4_ = _loc3_ + 1;
            _loc5_ = _loc1_["btn" + _loc4_];
            if(_loc5_)
            {
               _loc5_.ef_decide.visible = false;
               this.m_param_base_array[_loc3_] = this.m_callback.GetUserDataInt(ReceiveType_ParamDataStart + _loc3_);
               this.m_param_edit_array[_loc3_] = this.m_param_base_array[_loc3_];
               trace("m_param_base_array[" + _loc3_ + "]:" + this.m_param_base_array[_loc3_]);
               _loc5_.sys_header.sys.text = StringList["StatusParam" + _loc4_];
               _loc5_.sys_nmb.sys.text = this.m_param_base_array[_loc3_];
               _loc6_ = this.m_is_limit_break1 && this.m_param_base_array[_loc3_] > NormalParamMax;
               _loc7_ = this.m_is_limit_break2 && this.m_param_base_array[_loc3_] > LimitBreak1Max;
               _loc5_.sys_nmb.gotoAndStop(this.getStringColorFrame(false,_loc6_,_loc7_));
               _loc5_.icn._CMN_CHART_icn.gotoAndStop(_loc4_);
               _loc5_.arw_l1.visible = false;
               _loc5_.arw_r1.visible = false;
               _loc5_.gotoAndPlay("off");
               this.m_button_list.AddButton(_loc5_,_loc3_);
               if(_loc3_ == 0)
               {
                  this.m_button_list.ChangeButton(0);
                  if(!this.isEditorParamMax(0))
                  {
                     _loc5_.arw_r1.visible = true;
                  }
                  if(_loc2_ <= 0)
                  {
                     _loc5_.arw_r1.visible = false;
                  }
                  // General_QoL: confirmed points can be taken back out.
                  if(!this.isEditorParamMin(0))
                  {
                     _loc5_.arw_l1.visible = true;
                  }
               }
            }
            _loc3_++;
         }
         if(this.m_supers_mode != "0")
         {
            this.supersStart();
         }
      }

      private function startEditor() : void
      {
         this.m_timeline.menu_edit.gotoAndPlay("start");
      }

      private function endEditor() : void
      {
         this.m_timeline.menu_edit.gotoAndPlay("end");
      }

      private function isEndEditor() : Boolean
      {
         return this.m_timeline.menu_edit.currentLabel == "end_comp";
      }

      private function updateEditorButton(param1:int, param2:int = 0) : void
      {
         var _loc3_:MovieClip = this.m_button_list.GetButton(param1);
         if(!_loc3_)
         {
            return;
         }
         var _loc4_:int = this.m_callback.GetUserDataInt(ReceiveType_StatusPoint);
         this.m_timeline.menu_edit.stacusmenu.totalpoint.sys_pt.text = _loc4_;
         var _loc5_:int = this.m_callback.GetUserDataInt(ReceiveType_ParamDataStart + param1);
         _loc3_.sys_nmb.sys.text = _loc5_;
         if(0 < param2)
         {
            _loc3_.gotoAndPlay("push_r");
         }
         else if(param2 < 0)
         {
            _loc3_.gotoAndPlay("push_l");
         }
         var _loc6_:Boolean = this.m_is_limit_break1 && _loc5_ > NormalParamMax;
         var _loc7_:Boolean = this.m_is_limit_break2 && _loc5_ > LimitBreak1Max;
         if(param1 == SupersStrikeIndex || param1 == SupersKiIndex)
         {
            this.supersUpdate();
         }
         if(this.isEditorParamMin(param1))
         {
            _loc3_.arw_l1.visible = false;
         }
         else
         {
            _loc3_.arw_l1.visible = true;
         }
         if(this.isEditorParamMax(param1))
         {
            _loc3_.arw_r1.visible = false;
         }
         else
         {
            _loc3_.arw_r1.visible = true;
         }
         if(_loc4_ == 0)
         {
            _loc3_.arw_r1.visible = false;
         }
         // General_QoL: "edited" colour when the value differs from the saved one
         // (vanilla: when above it, the only way it could differ).
         if(!this.isEditorParamChanged(param1))
         {
            _loc3_.sys_nmb.gotoAndStop(this.getStringColorFrame(false,_loc6_,_loc7_));
         }
         else
         {
            _loc3_.sys_nmb.gotoAndStop(this.getStringColorFrame(true,_loc6_,_loc7_));
         }
      }

      // The lowest value a row can be set to: General_QoL 0, vanilla the
      // confirmed value.
      private function editorParamFloor(param1:int) : int
      {
         return this.m_free_points ? 0 : int(this.m_param_base_array[param1]);
      }

      // The left arrow shows down to the floor (General_QoL 0, vanilla the
      // confirmed value). The "changed" uses are in isEditorParamChanged.
      private function isEditorParamMin(param1:int) : Boolean
      {
         return this.m_param_edit_array[param1] <= this.editorParamFloor(param1);
      }

      // The value differs from the confirmed one (General_QoL: either way;
      // vanilla can only go above it - its own test was !isEditorParamMin).
      private function isEditorParamChanged(param1:int) : Boolean
      {
         if(this.m_free_points)
         {
            return this.m_param_edit_array[param1] != this.m_param_base_array[param1];
         }
         return this.m_param_edit_array[param1] > this.m_param_base_array[param1];
      }

      // Combined Supers: the two rows' names give way to one "Supers" label
      // with the combined total; the box is drawn once the rows stop moving.
      private function supersStart() : void
      {
         var strikeRow:MovieClip = this.m_button_list.GetButton(SupersStrikeIndex);
         var kiRow:MovieClip = this.m_button_list.GetButton(SupersKiIndex);
         if(!strikeRow || !kiRow)
         {
            this.m_supers_mode = "0";
            return;
         }
         this.supersStop();
         this.m_supers_last_bounds = null;
         this.m_supers_stable_frames = 0;
         this.m_supers_wait_frames = 0;
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.supersWaitSettled);
      }

      // Combined Supers: the two rows' bounds in the list's space, without the
      // parts that come and go (the decide effect and the arrows).
      private function supersRowsBounds(menu:MovieClip) : Rectangle
      {
         var bounds:Rectangle = null;
         var rows:Array = [this.m_button_list.GetButton(SupersStrikeIndex),this.m_button_list.GetButton(SupersKiIndex)];
         var rowIndex:int = 0;
         var row:MovieClip = null;
         var childIndex:int = 0;
         var part:DisplayObject = null;
         var partBounds:Rectangle = null;
         while(rowIndex < rows.length)
         {
            row = rows[rowIndex];
            childIndex = 0;
            while(childIndex < row.numChildren)
            {
               part = row.getChildAt(childIndex);
               if(part && part.visible && part != row.ef_decide && part != row.arw_l1 && part != row.arw_r1)
               {
                  partBounds = part.getBounds(menu);
                  if(partBounds.width > 0 && partBounds.height > 0)
                  {
                     bounds = bounds == null ? partBounds : bounds.union(partBounds);
                  }
               }
               childIndex++;
            }
            rowIndex++;
         }
         return bounds;
      }

      // Combined Supers: waits (bounded) for the start animation to stop
      // moving the rows, then lays out the box once.
      private function supersWaitSettled(param1:Event) : void
      {
         var menu:MovieClip = this.m_timeline.menu_edit.stacusmenu;
         var bounds:Rectangle = menu ? this.supersRowsBounds(menu) : null;
         this.m_supers_wait_frames++;
         if(bounds != null && this.m_supers_last_bounds != null && bounds.equals(this.m_supers_last_bounds))
         {
            this.m_supers_stable_frames++;
         }
         else
         {
            this.m_supers_stable_frames = 0;
         }
         this.m_supers_last_bounds = bounds;
         if(this.m_supers_stable_frames < SupersSettleFrames && this.m_supers_wait_frames < SupersSettleMaxFrames)
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.supersWaitSettled);
         if(bounds != null)
         {
            this.supersLayout(menu,bounds);
         }
      }

      // Combined Supers: a blank text field in `source`'s font (Iggy draws only
      // fonts the movie already uses), scaled to look like `source` does in
      // `space` - the same way as SaveSync's saveSyncCopyField/saveSyncMakeField.
      private static function supersCopyField(source:TextField, space:DisplayObject, color:int) : TextField
      {
         var field:TextField = new TextField();
         var format:* = source.getTextFormat();
         format.align = "left";
         format.leading = 0;
         if(color >= 0)
         {
            format.color = color;
         }
         field.defaultTextFormat = format;
         field.embedFonts = source.embedFonts;
         field.selectable = false;
         field.mouseEnabled = false;
         field.multiline = false;
         field.wordWrap = false;
         field.autoSize = "left";
         field.filters = [new GlowFilter(0,1,SupersOutlineBlur,SupersOutlineBlur,SupersOutlineStrength)];
         var sourceBounds:Rectangle = source.getBounds(space);
         var sourceLocalHeight:Number = source.scaleY != 0 ? source.height / Math.abs(source.scaleY) : 0;
         var scale:Number = sourceLocalHeight > 0 ? sourceBounds.height / sourceLocalHeight : 1;
         field.scaleX = scale;
         field.scaleY = scale;
         return field;
      }

      // Combined Supers: the gold box around both rows with a dim backdrop,
      // chain links on its left and right sides, and the "Supers" label +
      // total on the seam between the rows, right-aligned to the value pills.
      // The rows keep their own names and values (dimmed, still editable).
      private function supersLayout(menu:MovieClip, rows:Rectangle) : void
      {
         var strikeRow:MovieClip = this.m_button_list.GetButton(SupersStrikeIndex);
         var kiRow:MovieClip = this.m_button_list.GetButton(SupersKiIndex);
         // Vertically the rows' parts reach past the bars (the text fields
         // are taller than the bar), so the box goes from the gap above the
         // Strike row to the gap below the Ki row: the value pills sit in the
         // middle of their bars, and half the row pitch is the bar's half
         // height plus half the gap between bars.
         var strikePill:Rectangle = strikeRow.sys_nmb.getBounds(menu);
         var kiPill:Rectangle = kiRow.sys_nmb.getBounds(menu);
         var strikeMiddle:Number = strikePill.y + strikePill.height / 2;
         var kiMiddle:Number = kiPill.y + kiPill.height / 2;
         var halfPitch:Number = (kiMiddle - strikeMiddle) / 2;
         // Pad = room left/right of the rows; corner = the trim's corner
         // rounding (both in the list's units, by eye).
         var pad:Number = 3;
         var corner:Number = 10;
         var left:Number = rows.x - pad;
         var top:Number = strikeMiddle - halfPitch;
         var width:Number = rows.width + pad * 2;
         var height:Number = kiMiddle + halfPitch - top;
         var seam:Number = (strikeMiddle + kiMiddle) / 2;
         this.m_supers_box = new Sprite();
         this.m_supers_box.mouseEnabled = false;
         var g:* = this.m_supers_box.graphics;
         g.lineStyle();
         g.beginFill(0,SupersBackdropAlpha);
         g.drawRoundRect(left,top,width,height,corner,corner);
         g.endFill();
         g.lineStyle(5,SupersShadow,0.6);
         g.drawRoundRect(left,top,width,height,corner,corner);
         g.lineStyle(2.5,SupersGold,1);
         g.drawRoundRect(left,top,width,height,corner,corner);
         g.lineStyle(1,SupersGoldLight,0.7);
         g.drawRoundRect(left + 2.5,top + 2.5,width - 5,height - 5,corner - 3,corner - 3);
         this.supersDrawChain(g,left,seam);
         this.supersDrawChain(g,left + width,seam);
         menu.addChild(this.m_supers_box);
         var pillLeft:Number = Math.min(strikePill.x,kiPill.x);
         // Left empty here: supersUpdate takes (x, y) as the right edge and
         // the middle while the field has no text yet.
         this.m_supers_total = supersCopyField(strikeRow.sys_nmb.sys,menu,SupersTextColor);
         this.m_supers_total.x = pillLeft - SupersTextGap;
         this.m_supers_total.y = seam;
         menu.addChild(this.m_supers_total);
         // The label ends a three-digit total's width left of the total, so
         // it stays put while the total changes.
         this.m_supers_total.text = "888";
         var totalRoom:Number = this.m_supers_total.width;
         this.m_supers_total.text = "";
         this.m_supers_label = supersCopyField(strikeRow.sys_header.sys,menu,SupersTextColor);
         this.m_supers_label.text = SupersLabel;
         this.m_supers_label.x = pillLeft - SupersTextGap - totalRoom - SupersTextGap - this.m_supers_label.width;
         this.m_supers_label.y = seam - this.m_supers_label.height / 2;
         menu.addChild(this.m_supers_label);
         this.supersUpdate();
      }

      // Combined Supers: a short chain (link, edge-on link, link) standing on
      // the box's side at (x, y): a dark outline pass, then the gold pass.
      private function supersDrawChain(g:*, x:Number, y:Number) : void
      {
         var pass:int = 0;
         var outline:Boolean = false;
         while(pass < 2)
         {
            outline = pass == 0;
            g.lineStyle(outline ? 4.5 : 2,outline ? SupersShadow : SupersGold,outline ? 0.7 : 1);
            g.drawRoundRect(x - 3.5,y - 14,7,11,6,6);
            g.drawRoundRect(x - 3.5,y + 3,7,11,6,6);
            g.moveTo(x,y - 6);
            g.lineTo(x,y + 6);
            pass++;
         }
      }

      // Combined Supers: the combined total (sum for additive, the higher one
      // for highest).
      private function supersUpdate() : void
      {
         if(this.m_supers_mode == "0" || !this.m_button_list)
         {
            return;
         }
         if(this.m_supers_total == null)
         {
            return;
         }
         var strike:int = this.m_callback.GetUserDataInt(ReceiveType_ParamDataStart + SupersStrikeIndex);
         var ki:int = this.m_callback.GetUserDataInt(ReceiveType_ParamDataStart + SupersKiIndex);
         var total:int = this.m_supers_mode == "h" ? int(Math.max(strike,ki)) : strike + ki;
         var right:Number = this.m_supers_total.x + this.m_supers_total.width;
         var middle:Number = this.m_supers_total.y + this.m_supers_total.height / 2;
         if(this.m_supers_total.text == "")
         {
            right = this.m_supers_total.x;
            middle = this.m_supers_total.y;
         }
         this.m_supers_total.text = String(total);
         this.m_supers_total.x = right - this.m_supers_total.width;
         this.m_supers_total.y = middle - this.m_supers_total.height / 2;
      }

      // Combined Supers: takes the box and fields back out (menu closing or
      // laid out again).
      private function supersStop() : void
      {
         if(this.m_timeline && this.m_timeline.stage)
         {
            this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.supersWaitSettled);
         }
         var parts:Array = [this.m_supers_box,this.m_supers_label,this.m_supers_total];
         var partIndex:int = 0;
         var part:DisplayObject = null;
         while(partIndex < parts.length)
         {
            part = parts[partIndex];
            if(part != null && part.parent != null)
            {
               part.parent.removeChild(part);
            }
            partIndex++;
         }
         this.m_supers_box = null;
         this.m_supers_label = null;
         this.m_supers_total = null;
      }

      private function isEditorParamMax(param1:int) : Boolean
      {
         return this.m_callback.GetUserDataInt(ReceiveType_ParamDataMax) <= this.m_param_edit_array[param1];
      }

      private function pushU() : void
      {
         this.changeStatusSelect(-1);
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
      }

      private function pushD() : void
      {
         this.changeStatusSelect(1);
         this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
      }

      private function pushL() : void
      {
         if(this.changeStatusParam(-1))
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypePageChnage);
         }
      }

      private function pushR() : void
      {
         if(this.changeStatusParam(1))
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypePageChnage);
         }
      }

      private function pushCancel() : void
      {
         this.requestEnd();
         this.m_callback.CallbackSe(this.m_callback.SeTypeCancel);
      }

      private function pushEnter() : void
      {
         if(this.requestDecide())
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeDecide);
         }
      }

      private function pushX() : void
      {
         if(this.requestReset())
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeCancel);
         }
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
         this.supersStop();
         this.m_callback = null;
         this.m_button_list.ClearButton();
         this.m_button_list = null;
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStart);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitDecideRequest);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitResetRequest);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEndRequest);
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
   }
}

