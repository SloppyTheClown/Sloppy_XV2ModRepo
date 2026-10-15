package action_script
{
   import flash.display.MovieClip;
   import flash.events.Event;
   import flash.filters.GlowFilter;
   import flash.geom.Point;
   import flash.geom.Rectangle;
   import flash.text.TextField;
   import flash.text.TextFieldAutoSize;
   import flash.text.TextFormat;
   import flash.utils.getTimer;
   
   public class LobbyMap
   {
      
      internal static const MemberMax:int = 3;
      
      internal static const StandbyMax:int = 8;
      
      public static const State_Wait:int = 0;
      
      public static const State_AtartAnim:int = 1;
      
      public static const State_EndAnim:int = 2;
      
      public static const State_Hide:int = 3;
      
      internal static const MapSizeMax:int = 720 / 2;
      
      internal static const StandbyInfo_Command:int = 0;
      
      internal static const StandbyInfo_UserId:int = 1;
      
      internal static const StandbyInfo_UserName:int = 2;
      
      internal static const StandbyInfoNum:int = 3;
      
      internal static const StandbyCommand_Add:int = 0;
      
      internal static const StandbyCommand_Delete:int = 1;
      
      internal static const Event_Max:int = 22;
      
      internal static const MainArea_Num:int = 0;
      
      internal static const BaseAreaMax:int = 5;
      
      internal static const BeaConDispTime:int = 3 * 1000;
      
      internal static const Depth_BaseArea:int = 2;
      
      internal static const Depth_Player:int = 8;
      
      internal static const Depth_BeaCon:int = 9;
      
      internal static const Depth_EventStart:int = 10;
      
      internal static const IconListPageMax:int = 2;
      
      internal static const IconListLen:int = 9;
      
      private var m_mc_map:MovieClip = null;
      
      private var m_obj_num:int;
      
      private var m_obj_array:Array;
      
      private var m_mc_party:MovieClip = null;
      
      private var m_state:int;
      
      private var m_icon0_array:Array = new Array();
      
      private var m_icon1_array:Array = new Array();
      
      private var m_icon2_array:Array = new Array();
      
      private var m_icon3_array:Array = new Array();
      
      private var m_icon4_array:Array = new Array();
      
      private var m_icon5_array:Array = new Array();
      
      private var m_icon6_array:Array = new Array();
      
      private var m_icon7_array:Array = new Array();
      
      private var m_icon8_array:Array = new Array();
      
      private var m_icon9_array:Array = new Array();
      
      private var m_icon10_array:Array = new Array();
      
      private var m_icon11_array:Array = new Array();
      
      private var m_icon12_array:Array = new Array();
      
      private var m_icon13_array:Array = new Array();
      
      private var m_icon14_array:Array = new Array();
      
      private var m_icon15_array:Array = new Array();
      
      private var m_icon16_array:Array = new Array();
      
      private var m_icon17_array:Array = new Array();
      
      private var m_icon18_array:Array = new Array();
      
      private var m_icon19_array:Array = new Array();
      
      private var m_icon20_array:Array = new Array();
      
      private var m_icon21_array:Array = new Array();
      
      private var m_icon_array:Array = [this.m_icon0_array,this.m_icon1_array,this.m_icon2_array,this.m_icon3_array,this.m_icon4_array,this.m_icon5_array,this.m_icon6_array,this.m_icon7_array,this.m_icon8_array,this.m_icon9_array,this.m_icon10_array,this.m_icon11_array,this.m_icon12_array,this.m_icon13_array,this.m_icon14_array,this.m_icon15_array,this.m_icon16_array,this.m_icon17_array,this.m_icon18_array,this.m_icon19_array,this.m_icon20_array,this.m_icon21_array];
      
      private var m_icon_count:Array = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
      
      private var m_beacon_array:Array = new Array();
      
      private var m_area_index:int = 0;
      
      private var m_beacon_index:int = 0;
      
      private var m_icon_depth:Array = [14,6,9,8,11,10,4,1,7,7,3,15,2,2,13,12,5,5,5,5,5,16];
      
      private var m_icon_list_name_text:Array = new Array();
      
      private var m_icon_list_visible:Array = new Array();
      
      private var m_is_icon_visible_update:Boolean = false;
      
      private var m_icon_list_page_no:int = 0;
      
      public function LobbyMap(param1:MovieClip)
      {
         var _loc2_:int = 0;
         super();
         this.m_mc_map = param1;
         this.m_mc_map.visible = false;
         this.m_mc_map.gotoAndStop("init");
         this.m_mc_map.map_inact.icn_own.visible = false;
         this.m_mc_map.map_inact.icn_own_outside.visible = false;
         this.m_mc_map.map_inact.icn_beacon.visible = false;
         var _loc3_:int = this.m_mc_map.map_inact.numChildren - 1;
         this.m_mc_map.map_inact.setChildIndex(this.m_mc_map.map_inact.icn_own,_loc3_);
         this.m_mc_map.map_inact.setChildIndex(this.m_mc_map.map_inact.icn_own_outside,_loc3_);
         var _loc4_:int = 0;
         while(_loc4_ < Event_Max)
         {
            this.m_mc_map.map_inact["icn_" + (_loc4_ + 1)].visible = false;
            _loc4_++;
         }
         var _loc5_:int = 0;
         while(_loc5_ < BaseAreaMax)
         {
            this.m_mc_map.map_inact["icn_base" + (_loc5_ + 1)].visible = false;
            _loc5_++;
         }
         this.m_state = State_Hide;
         this.m_obj_array = new Array(MemberMax);
         this.clearObj();
      }
      
      public function GetState() : int
      {
         return this.m_state;
      }
      
      public function Destroy() : void
      {
         this.m_mc_map.stage.removeEventListener(Event.ENTER_FRAME,this.waitStart);
         this.m_mc_map.stage.removeEventListener(Event.ENTER_FRAME,this.waitEnd);
         this.m_mc_map.visible = false;
         this.m_mc_map = null;
      }
      
      public function UpdateBeaCon() : *
      {
         var _loc4_:MovieClip = null;
         var _loc1_:int = getTimer();
         var _loc2_:uint = this.m_beacon_array.length;
         var _loc3_:uint = 0;
         while(_loc3_ < _loc2_)
         {
            _loc4_ = this.m_beacon_array[_loc3_];
            if(_loc4_ != null)
            {
               if(_loc1_ > _loc4_.m_start_time + BeaConDispTime)
               {
                  this.m_mc_map.map_inact.removeChild(this.m_beacon_array[_loc3_]);
                  this.m_beacon_array[_loc3_] = null;
               }
            }
            _loc3_++;
         }
      }
      
      public function InitIconList() : void
      {
         this.m_mc_map.icn_list.gotoAndStop("wait");
         this.m_icon_list_page_no = 0;
         this.SetListPage(this.m_icon_list_page_no);
      }
      
      public function UpdateIconList() : void
      {
         var _loc1_:Boolean = false;
         if(this.m_is_icon_visible_update)
         {
            if(this.SetPageIconVisible(this.m_icon_list_page_no))
            {
               this.m_is_icon_visible_update = false;
            }
         }
         if(this.m_mc_map.icn_list.currentLabel == "push_out_l_comp")
         {
            this.SetListPage(this.m_icon_list_page_no);
            this.m_mc_map.icn_list.gotoAndPlay("push_in_l");
         }
         else if(this.m_mc_map.icn_list.currentLabel == "push_out_r_comp")
         {
            this.SetListPage(this.m_icon_list_page_no);
            this.m_mc_map.icn_list.gotoAndPlay("push_in_r");
         }
         else if(this.m_mc_map.icn_list.currentLabel == "push_in_l_comp" || this.m_mc_map.icn_list.currentLabel == "push_in_r_comp")
         {
            this.m_mc_map.icn_list.gotoAndStop("wait");
         }
      }
      
      public function Start() : void
      {
         if(this.m_state != State_Hide)
         {
            return;
         }
         this.m_state = State_AtartAnim;
         this.m_mc_map.visible = true;
         this.m_mc_map.gotoAndPlay("start");
         this.InitIconList();
         this.m_mc_map.stage.addEventListener(Event.ENTER_FRAME,this.waitStart);
      }
      
      private function waitStart(param1:Event) : void
      {
         if(this.m_mc_map.currentLabel != "wait")
         {
            return;
         }
         this.m_mc_map.stage.removeEventListener(Event.ENTER_FRAME,this.waitStart);
         this.m_state = State_Wait;
      }
      
      public function End() : void
      {
         if(this.m_state != State_Wait)
         {
            return;
         }
         this.m_state = State_EndAnim;
         this.m_mc_map.gotoAndPlay("end");
         this.m_mc_map.stage.addEventListener(Event.ENTER_FRAME,this.waitEnd);
      }
      
      private function waitEnd(param1:Event) : void
      {
         if(this.m_mc_map.currentFrame < this.m_mc_map.totalFrames)
         {
            return;
         }
         this.m_mc_map.stage.removeEventListener(Event.ENTER_FRAME,this.waitEnd);
         this.m_mc_map.visible = false;
         this.m_state = State_Hide;
      }
      
      public function SetArea(param1:int) : void
      {
         this.m_area_index = param1;
      }
      
      public function SetPlayer(param1:int, param2:int, param3:int) : void
      {
         this.m_mc_map.map_inact.icn_own.visible = false;
         this.m_mc_map.map_inact.icn_own_outside.visible = false;
         var _loc4_:int = param1;
         var _loc5_:int = param2;
         var _loc6_:int = param3;
         var _loc7_:int = int(this.m_mc_map.map_inact.point1.x);
         var _loc8_:int = int(this.m_mc_map.map_inact.point2.x);
         var _loc9_:int = _loc8_ - _loc7_;
         _loc4_ = (_loc4_ / 100 as Number) * _loc9_;
         _loc5_ = (_loc5_ / 100 as Number) * _loc9_;
         _loc4_ += _loc7_;
         _loc5_ += this.m_mc_map.map_inact.point1.y;
         if(_loc4_ < -MapSizeMax)
         {
            _loc4_ = -MapSizeMax;
         }
         if(MapSizeMax < _loc4_)
         {
            _loc4_ = MapSizeMax;
         }
         if(_loc5_ < -MapSizeMax)
         {
            _loc5_ = -MapSizeMax;
         }
         if(MapSizeMax < _loc5_)
         {
            _loc5_ = MapSizeMax;
         }
         var _loc10_:Boolean = this.m_area_index == MainArea_Num;
         var _loc11_:MovieClip = _loc10_ ? this.m_mc_map.map_inact.icn_own : this.m_mc_map.map_inact.icn_own_outside;
         this.m_mc_map.map_inact.setChildIndex(_loc11_,Depth_Player);
         _loc11_.visible = true;
         _loc11_.x = _loc4_;
         _loc11_.y = _loc5_;
         _loc11_.rotation = _loc6_;
      }
      
      public function SetEvent(param1:Object) : void
      {
         if(param1.id == undefined)
         {
            return;
         }
         if(param1.x == undefined)
         {
            return;
         }
         if(param1.y == undefined)
         {
            return;
         }
         var _loc2_:int = param1.id as int;
         var _loc3_:int = param1.x as int;
         var _loc4_:int = param1.y as int;
         var _loc5_:int = int(this.m_mc_map.map_inact.point1.x);
         var _loc6_:int = int(this.m_mc_map.map_inact.point2.x);
         var _loc7_:int = _loc6_ - _loc5_;
         _loc3_ = (_loc3_ / 100 as Number) * _loc7_;
         _loc4_ = (_loc4_ / 100 as Number) * _loc7_;
         _loc3_ += _loc5_;
         _loc4_ += this.m_mc_map.map_inact.point1.y;
         if(_loc3_ < -MapSizeMax)
         {
            _loc3_ = -MapSizeMax;
         }
         if(MapSizeMax < _loc3_)
         {
            _loc3_ = MapSizeMax;
         }
         if(_loc4_ < -MapSizeMax)
         {
            _loc4_ = -MapSizeMax;
         }
         if(MapSizeMax < _loc4_)
         {
            _loc4_ = MapSizeMax;
         }
         var _loc8_:MovieClip = this.createEventIcon(_loc2_,_loc3_,_loc4_);
         if(_loc8_ == null)
         {
            return;
         }
         // General_QoL: the mentor's name next to the instructor icons.
         if(_loc8_ is po_icn_sensei || _loc8_ is po_icn_sensei2)
         {
            this.addNameLabel(_loc8_,eventName(param1));
         }
      }

      // ---- General_QoL: mentor names on the map ---------------------------
      // General_QoL.dll appends the lobby NPC's name to each "Event:" line as
      // c0=..,c1=..: one UTF-16 code unit per field, in decimal, because
      // LobbyHUD.parseMapInfo runs parseInt on every value.
      private static function eventName(param1:Object) : String
      {
         var name:String = "";
         var index:int = 0;
         while(index < 64 && param1["c" + index] != undefined)
         {
            name += String.fromCharCode(int(param1["c" + index]));
            index++;
         }
         return name;
      }

      // Font size of the label relative to the legend's text (the list under
      // the map), measured on screen: 0.8 looked too big in game, 2/3 of it.
      internal static const NameLabelScale:Number = 0.8 * 2 / 3;

      // Where the text starts, relative to the icon's right edge. The icon's
      // bounds reach past its visible circle, so a gap in map units barely
      // changed the visible ~23 px gap (screenshot, 2026-10-05); pulling the
      // text back by 40% of the icon's width roughly halves it. A TextField
      // also puts its text 2 units in from its left edge (Flash's gutter).
      internal static const NameLabelPullBack:Number = 0.4;
      internal static const TextFieldGutter:Number = 2;

      // Dark yellow, picked by eye; the black glow keeps it readable.
      internal static const NameLabelColor:uint = 0xC8A01E;

      // A text label to the right of the icon, in the legend's font (Iggy
      // draws only fonts the movie already uses), kept on the icon as
      // qol_name_label so it goes when the icon does.
      private function addNameLabel(icon:MovieClip, name:String) : void
      {
         if(name.length == 0)
         {
            return;
         }
         var map:MovieClip = this.m_mc_map.map_inact;
         var label:TextField = new TextField();
         var legend:TextField = null;
         if(this.m_mc_map.icn_list != null && this.m_mc_map.icn_list.list != null && this.m_mc_map.icn_list.list.sys_icn_1 != null)
         {
            legend = this.m_mc_map.icn_list.list.sys_icn_1.sys as TextField;
         }
         if(legend != null)
         {
            var format:TextFormat = legend.getTextFormat();
            var size:Number = format.size != null ? Number(format.size) : 12;
            format.size = Math.max(4,Math.round(size * NameLabelScale * screenScale(legend) / screenScale(map)));
            format.align = "left";
            format.color = NameLabelColor;
            label.defaultTextFormat = format;
            label.embedFonts = legend.embedFonts;
         }
         label.selectable = false;
         label.mouseEnabled = false;
         label.wordWrap = false;
         label.multiline = false;
         label.autoSize = TextFieldAutoSize.LEFT;
         label.text = name;
         label.filters = [new GlowFilter(0,1,3,3,4)];
         var bounds:Rectangle = icon.getBounds(map);
         label.x = bounds.right - bounds.width * NameLabelPullBack - TextFieldGutter;
         label.y = (bounds.top + bounds.bottom) / 2 - label.height / 2;
         map.addChild(label);
         icon.qol_name_label = label;
      }

      // Horizontal scale from the object's own units to the screen.
      private static function screenScale(object:Object) : Number
      {
         var origin:Point = object.localToGlobal(new Point(0,0));
         var unit:Point = object.localToGlobal(new Point(100,0));
         var scale:Number = (unit.x - origin.x) / 100;
         return scale > 0 ? scale : 1;
      }

      private function removeNameLabel(icon:MovieClip) : void
      {
         var label:TextField = icon.qol_name_label as TextField;
         if(label != null && label.parent != null)
         {
            label.parent.removeChild(label);
         }
         icon.qol_name_label = null;
      }

      // Labels above every icon (ChangeDepth re-stacks the icons).
      private function raiseNameLabels() : void
      {
         var map:MovieClip = this.m_mc_map.map_inact;
         for each(var icons:Array in this.m_icon_array)
         {
            for each(var icon:MovieClip in icons)
            {
               if(icon != null && icon.qol_name_label != null && icon.qol_name_label.parent == map)
               {
                  map.setChildIndex(icon.qol_name_label,map.numChildren - 1);
               }
            }
         }
      }
      // ---------------------------------------------------------------------

      public function SetBaseArea(param1:Object) : void
      {
         if(param1.id == undefined)
         {
            return;
         }
         if(param1.x == undefined)
         {
            return;
         }
         if(param1.y == undefined)
         {
            return;
         }
         var _loc2_:int = param1.id as int;
         var _loc3_:int = param1.x as int;
         var _loc4_:int = param1.y as int;
         var _loc5_:int = int(this.m_mc_map.map_inact.point1.x);
         var _loc6_:int = int(this.m_mc_map.map_inact.point2.x);
         var _loc7_:int = _loc6_ - _loc5_;
         _loc3_ = (_loc3_ / 100 as Number) * _loc7_;
         _loc4_ = (_loc4_ / 100 as Number) * _loc7_;
         _loc3_ += _loc5_;
         _loc4_ += this.m_mc_map.map_inact.point1.y;
         if(_loc3_ < -MapSizeMax)
         {
            _loc3_ = -MapSizeMax;
         }
         if(MapSizeMax < _loc3_)
         {
            _loc3_ = MapSizeMax;
         }
         if(_loc4_ < -MapSizeMax)
         {
            _loc4_ = -MapSizeMax;
         }
         if(MapSizeMax < _loc4_)
         {
            _loc4_ = MapSizeMax;
         }
         var _loc8_:MovieClip = this.m_mc_map.map_inact["icn_base" + _loc2_];
         _loc8_.visible = true;
         _loc8_.x = _loc3_;
         _loc8_.y = _loc4_;
         this.m_mc_map.map_inact.setChildIndex(_loc8_,Depth_BaseArea);
      }
      
      public function SetBeaCon(param1:Object) : void
      {
         if(param1.x == undefined)
         {
            return;
         }
         if(param1.y == undefined)
         {
            return;
         }
         var _loc2_:int = param1.x as int;
         var _loc3_:int = param1.y as int;
         var _loc4_:int = int(this.m_mc_map.map_inact.point1.x);
         var _loc5_:int = int(this.m_mc_map.map_inact.point2.x);
         var _loc6_:int = _loc5_ - _loc4_;
         _loc2_ = (_loc2_ / 100 as Number) * _loc6_;
         _loc3_ = (_loc3_ / 100 as Number) * _loc6_;
         _loc2_ += _loc4_;
         _loc3_ += this.m_mc_map.map_inact.point1.y;
         if(_loc2_ < -MapSizeMax)
         {
            _loc2_ = -MapSizeMax;
         }
         if(MapSizeMax < _loc2_)
         {
            _loc2_ = MapSizeMax;
         }
         if(_loc3_ < -MapSizeMax)
         {
            _loc3_ = -MapSizeMax;
         }
         if(MapSizeMax < _loc3_)
         {
            _loc3_ = MapSizeMax;
         }
         var _loc7_:MovieClip = this.createBeaCon();
         if(_loc7_ == null)
         {
            return;
         }
         _loc7_.x = _loc2_;
         _loc7_.y = _loc3_;
         this.m_mc_map.map_inact.setChildIndex(_loc7_,Depth_BeaCon);
      }
      
      public function GetObjNum() : int
      {
         return this.m_obj_num;
      }
      
      private function clearObj() : void
      {
         this.m_obj_num = 0;
         var _loc1_:int = 0;
         while(_loc1_ < MemberMax)
         {
            this.m_obj_array[_loc1_] = -1;
            _loc1_++;
         }
      }
      
      private function createEventIcon(param1:int, param2:int, param3:int) : MovieClip
      {
         var _loc7_:Array = null;
         var _loc8_:int = 0;
         var _loc9_:int = 0;
         var _loc4_:MovieClip = this.m_mc_map.map_inact["icn_" + param1];
         param2 += _loc4_.x;
         param3 += _loc4_.y;
         var _loc5_:int = 0;
         loop0:
         while(_loc5_ < this.m_icon_array.length)
         {
            _loc7_ = this.m_icon_array[_loc5_];
            _loc8_ = 0;
            while(true)
            {
               if(_loc8_ >= _loc7_.length)
               {
                  _loc5_++;
                  continue loop0;
               }
               if(_loc7_[_loc8_] != null)
               {
                  if(_loc7_[_loc8_].x == param2 && _loc7_[_loc8_].y == param3)
                  {
                     _loc9_ = _loc7_[_loc8_].name as int;
                     if(_loc9_ >= this.m_icon_depth[param1 - 1])
                     {
                        break;
                     }
                     this.removeNameLabel(_loc7_[_loc8_]);
                     this.m_mc_map.map_inact.removeChild(_loc7_[_loc8_]);
                     _loc7_[_loc8_] = null;
                  }
               }
               _loc8_++;
            }
            return null;
         }
         var _loc6_:MovieClip = new _loc4_.constructor();
         this.m_mc_map.map_inact.addChild(_loc6_);
         _loc6_.visible = true;
         _loc6_.width = _loc4_.width;
         _loc6_.height = _loc4_.height;
         _loc6_.x = param2;
         _loc6_.y = param3;
         _loc6_.name = "" + this.m_icon_depth[param1 - 1];
         this.m_icon_array[param1 - 1][this.m_icon_count[param1 - 1]] = _loc6_;
         this.m_icon_count[param1 - 1] += 1;
         return _loc6_;
      }
      
      private function removeEventIcon() : void
      {
         var _loc3_:* = 0;
         var _loc1_:int = 0;
         while(_loc1_ < this.m_icon_array.length)
         {
            _loc3_ = int(this.m_icon_array[_loc1_].length - 1);
            while(_loc3_ >= 0)
            {
               if(this.m_icon_array[_loc1_][_loc3_])
               {
                  this.m_icon_array[_loc1_][_loc3_].visible = false;
                  this.removeNameLabel(this.m_icon_array[_loc1_][_loc3_]);
                  this.m_mc_map.map_inact.removeChild(this.m_icon_array[_loc1_][_loc3_]);
                  this.m_icon_array[_loc1_][_loc3_] = null;
               }
               _loc3_--;
            }
            _loc1_++;
         }
         var _loc2_:* = 0;
         while(_loc2_ < this.m_icon_count.length)
         {
            this.m_icon_count[_loc2_] = 0;
            _loc2_++;
         }
      }
      
      private function createBeaCon() : MovieClip
      {
         var _loc1_:MovieClip = this.m_mc_map.map_inact.icn_beacon;
         var _loc2_:MovieClip = new _loc1_.constructor();
         this.m_mc_map.map_inact.addChild(_loc2_);
         _loc2_.visible = true;
         this.m_beacon_array[this.m_beacon_index] = _loc2_;
         _loc2_.m_start_time = getTimer();
         ++this.m_beacon_index;
         return _loc2_;
      }
      
      public function Clear() : void
      {
         this.removeEventIcon();
      }
      
      public function ChangeDepth() : void
      {
         var _loc6_:int = 0;
         var _loc7_:int = 0;
         var _loc8_:int = 0;
         var _loc1_:int = int(this.m_icon_depth.length);
         var _loc2_:Array = new Array(_loc1_);
         var _loc3_:Array = new Array(_loc1_);
         var _loc4_:int = 0;
         while(_loc4_ < _loc1_)
         {
            _loc2_[_loc4_] = this.m_icon_depth[_loc4_];
            _loc3_[_loc4_] = _loc4_;
            _loc4_++;
         }
         _loc4_ = 0;
         while(_loc4_ < _loc1_ - 1)
         {
            _loc7_ = 0;
            while(_loc7_ < _loc1_ - _loc4_ - 1)
            {
               if(_loc2_[_loc7_] > _loc2_[_loc7_ + 1])
               {
                  _loc8_ = int(_loc2_[_loc7_]);
                  _loc2_[_loc7_] = _loc2_[_loc7_ + 1];
                  _loc2_[_loc7_ + 1] = _loc8_;
                  _loc8_ = int(_loc3_[_loc7_]);
                  _loc3_[_loc7_] = _loc3_[_loc7_ + 1];
                  _loc3_[_loc7_ + 1] = _loc8_;
               }
               _loc7_++;
            }
            _loc4_++;
         }
         var _loc5_:int = 0;
         for each(_loc6_ in _loc3_)
         {
            _loc7_ = 0;
            while(_loc7_ < this.m_icon_array[_loc6_].length)
            {
               if(this.m_icon_array[_loc6_][_loc7_] != null)
               {
                  this.m_mc_map.map_inact.setChildIndex(this.m_icon_array[_loc6_][_loc7_],Depth_EventStart + _loc5_);
                  _loc5_++;
               }
               _loc7_++;
            }
         }
         this.raiseNameLabels();
      }
      
      public function AddListPage() : void
      {
         if(this.m_is_icon_visible_update)
         {
            return;
         }
         ++this.m_icon_list_page_no;
         if(this.m_icon_list_page_no >= IconListPageMax)
         {
            this.m_icon_list_page_no = 0;
         }
         this.m_mc_map.icn_list.gotoAndPlay("push_out_r");
         this.m_mc_map.icn_list.spin.spn_r.gotoAndPlay("push");
      }
      
      public function DecListPage() : void
      {
         if(this.m_is_icon_visible_update)
         {
            return;
         }
         --this.m_icon_list_page_no;
         if(this.m_icon_list_page_no < 0)
         {
            this.m_icon_list_page_no = IconListPageMax - 1;
         }
         this.m_mc_map.icn_list.gotoAndPlay("push_out_l");
         this.m_mc_map.icn_list.spin.spn_l.gotoAndPlay("push");
      }
      
      public function SetListPage(param1:int) : void
      {
         var _loc2_:String = param1 + 1 + "/" + IconListPageMax;
         Utility.SetAutoSizedhtmlText(this.m_mc_map.icn_list.spin.sys,_loc2_);
         this.SetPageIconName(param1);
         this.m_mc_map.icn_list.list.gotoAndStop(param1 + 1);
         this.m_is_icon_visible_update = true;
      }
      
      public function SetIconName(param1:int, param2:String) : void
      {
         this.m_icon_list_name_text[param1] = param2;
      }
      
      public function SetIconVisible(param1:int, param2:Boolean) : void
      {
         this.m_icon_list_visible[param1] = param2;
      }
      
      public function SetPageIconName(param1:int) : void
      {
         if(param1 >= IconListPageMax)
         {
            return;
         }
         var _loc2_:int = param1 * IconListLen;
         var _loc3_:int = 0;
         while(_loc3_ < IconListLen)
         {
            this.m_mc_map.icn_list.list["sys_icn_" + (_loc3_ + 1)].visible = true;
            if(_loc2_ + _loc3_ < this.m_icon_list_name_text.length)
            {
               Utility.SetAutoSizedhtmlText(this.m_mc_map.icn_list.list["sys_icn_" + (_loc3_ + 1)].sys,this.m_icon_list_name_text[_loc2_ + _loc3_]);
            }
            else
            {
               this.m_mc_map.icn_list.list["sys_icn_" + (_loc3_ + 1)].sys.text = "";
            }
            _loc3_++;
         }
      }
      
      private function SetPageIconVisible(param1:int) : Boolean
      {
         var _loc5_:int = 0;
         if(param1 >= IconListPageMax)
         {
            return false;
         }
         var _loc2_:Boolean = true;
         var _loc3_:int = param1 * IconListLen;
         var _loc4_:int = 0;
         while(_loc4_ < IconListLen)
         {
            _loc5_ = _loc3_ + _loc4_;
            if(_loc5_ < this.m_icon_list_name_text.length)
            {
               this.m_mc_map.icn_list.list["sys_icn_" + (_loc4_ + 1)].visible = this.m_icon_list_visible[_loc5_];
               if(this.m_mc_map.icn_list.list["icon_" + (_loc5_ + 1)] != undefined)
               {
                  this.m_mc_map.icn_list.list["icon_" + (_loc5_ + 1)].visible = this.m_icon_list_visible[_loc5_];
               }
               else
               {
                  trace("not exist | icon instance : icon_" + (_loc5_ + 1));
                  _loc2_ = false;
               }
            }
            _loc4_++;
         }
         return _loc2_;
      }
   }
}

