package action_script
{
   import flash.display.MovieClip;
   import flash.events.Event;
   import flash.events.KeyboardEvent;
   import flash.ui.Keyboard;
   
   public class AvatarList
   {
      
      public static const ListElemMax:int = 8;
      
      public static const ReceiveType_ListSelect:int = 0;
      
      public static const ReceiveType_ListDataNum:int = 1;
      
      public static const ReceiveType_ListStrStart:int = 2;
      
      public static const ReceiveType_ListStrEnd:int = ReceiveType_ListStrStart + ListElemMax - 1;
      
      public static const ReceiveType_ListValidStart:int = ReceiveType_ListStrEnd + 1;
      
      public static const ReceiveType_ListValidEnd:int = ReceiveType_ListValidStart + ListElemMax - 1;
      
      public static const ReceiveType_ListDefault:int = ReceiveType_ListValidEnd + 1;
      
      public static const ReceiveType_Num:int = ReceiveType_ListDefault + 1;
      
      public static const SendType_RequestList:int = 0;
      
      public static const SendType_ListSelect:int = 1;
      
      // BattleStyles: the style list's search (the loader's search box).
      // The script reads the entry count only when the list opens; to show a
      // filtered list while it is open, the game sends the count again
      // (EditList 0x503970 sets user data 0 and 1) plus the entry to put the
      // cursor on in ReceiveType_ListDefault, and checkSearchRefresh redraws
      // from them. ReceiveType_ListDefault is the X button's entry (the
      // game's list opener 0x4fcac0 sends it through 0x503a50), so it is kept
      // here when the list starts and put back after a refresh.
      private var m_receiving:Boolean = false;
      
      private var m_default_valid:Boolean = false;
      
      private var m_default:int = 0;
      
      private var m_callback:Callback = null;
      
      private var m_list_box:ListBox = null;
      
      private var m_list_valid:Array = null;
      
      private var m_timeline:MovieClip = null;
      
      public function AvatarList()
      {
         super();
         this.m_callback = new Callback(ReceiveType_Num);
         this.m_list_box = new ListBox(ListElemMax);
         this.m_list_valid = new Array(ListElemMax);
         this.m_timeline = null;
      }
      
      public function Initialize(param1:MovieClip) : void
      {
         this.m_timeline = param1;
         this.m_timeline.visible = false;
         this.m_list_box.ClearItem();
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitStart);
      }
      
      private function waitStart(param1:Event) : void
      {
         if(!this.checkReceiveList())
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
      
      private function endMenu(param1:Boolean) : void
      {
         var _loc2_:int = 0;
         if(param1)
         {
            _loc2_ = this.m_list_box.GetSelectIndex();
            this.m_callback.CallbackDecide(_loc2_);
         }
         else
         {
            this.m_callback.CallbackCancel();
         }
         this.endList();
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitEnd);
      }
      
      private function waitEnd(param1:Event) : void
      {
         if(!this.isEndList())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEnd);
         this.m_callback.CallbackExit();
         this.m_callback = null;
         this.m_callback = new Callback(ReceiveType_Num);
         this.Initialize(this.m_timeline);
      }
      
      private function checkReceiveList() : Boolean
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ListSelect))
         {
            return false;
         }
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ListDataNum))
         {
            return false;
         }
         return true;
      }
      
      private function startList() : void
      {
         var _loc4_:MovieClip = null;
         this.m_list_box.ClearItem();
         this.m_list_box.SetScrlBar(this.m_timeline.win.list._CMN_M_B_scroll,this.m_timeline.win.list.dummy_scroll);
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ListDataNum);
         this.m_list_box.ClearItem();
         var _loc2_:int = 0;
         while(_loc2_ < ListElemMax)
         {
            _loc4_ = this.m_timeline.win.list["btn" + (_loc2_ + 1)];
            this.m_list_box.AddItem(_loc4_,_loc4_.sys_item.sys);
            this.m_list_valid[_loc2_] = true;
            this.m_list_box.SetItem(_loc2_,"",true);
            _loc4_.icn_check.visible = false;
            _loc2_++;
         }
         this.m_list_box.SetItemNum(_loc1_);
         var _loc3_:int = this.m_callback.GetUserDataInt(ReceiveType_ListSelect);
         this.m_list_box.SelectList(_loc3_,0);
         this.m_callback.SetUserDataValidFlag(ReceiveType_ListDataNum,false);
         this.m_default_valid = this.m_callback.GetUserDataValidFlag(ReceiveType_ListDefault);
         this.m_default = this.m_callback.GetUserDataInt(ReceiveType_ListDefault);
         this.requestList();
         this.m_timeline.win.gotoAndPlay("start");
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.checkSearchRefresh);
      }
      
      // BattleStyles: every frame while the list is open. A count sent
      // again means the entries changed: new count, cursor on the entry sent
      // in ReceiveType_ListDefault (else the first; the X button's entry is
      // put back), rows asked for again and the cursor's entry sent back as
      // for a cursor move. Waits while rows are still coming in.
      private function checkSearchRefresh(param1:Event) : void
      {
         if(this.m_receiving || !this.m_callback.GetUserDataValidFlag(ReceiveType_ListDataNum))
         {
            return;
         }
         var count:int = this.m_callback.GetUserDataInt(ReceiveType_ListDataNum);
         var select:int = 0;
         if(this.m_callback.GetUserDataValidFlag(ReceiveType_ListDefault))
         {
            select = this.m_callback.GetUserDataInt(ReceiveType_ListDefault);
         }
         this.m_callback.SetUserDataValidFlag(ReceiveType_ListDataNum,false);
         this.m_callback.m_user_data[ReceiveType_ListDefault] = this.m_default;
         this.m_callback.SetUserDataValidFlag(ReceiveType_ListDefault,this.m_default_valid);
         if(select < 0 || select >= count)
         {
            select = 0;
         }
         this.m_list_box.SetItemNum(count);
         this.m_list_box.SelectList(select,-1);
         this.requestList();
         this.sendListSelect();
      }
      
      private function requestList() : void
      {
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         var _loc1_:int = 0;
         while(_loc1_ < ListElemMax)
         {
            this.m_callback.SetUserDataValidFlag(ReceiveType_ListStrStart + _loc1_,false);
            this.m_callback.SetUserDataValidFlag(ReceiveType_ListValidStart + _loc1_,false);
            _loc1_++;
         }
         this.m_receiving = true;
         this.m_callback.CallbackUserData("user",SendType_RequestList,this.m_list_box.GetStartIndex());
         this.m_timeline.stage.addEventListener(Event.ENTER_FRAME,this.waitReceiveList);
      }
      
      private function waitReceiveList(param1:Event) : void
      {
         if(!this.checkReceiveListData())
         {
            return;
         }
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitReceiveList);
         this.m_receiving = false;
         this.updateList();
         this.m_timeline.stage.addEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
      }
      
      private function checkReceiveListData() : Boolean
      {
         var _loc1_:int = 0;
         while(_loc1_ < ListElemMax)
         {
            if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ListStrStart + _loc1_))
            {
               return false;
            }
            _loc1_++;
         }
         return true;
      }
      
      private function updateList() : void
      {
         var _loc4_:String = null;
         var _loc5_:MovieClip = null;
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ListSelect);
         var _loc2_:int = _loc1_ - this.m_list_box.GetStartIndex();
         var _loc3_:int = 0;
         while(_loc3_ < ListElemMax)
         {
            _loc4_ = this.m_callback.GetUserDataString(ReceiveType_ListStrStart + _loc3_);
            this.m_list_box.SetItem(_loc3_,_loc4_,true);
            this.m_timeline.win.list["btn" + (_loc3_ + 1)].icn_check.visible = _loc3_ == _loc2_;
            if(this.m_callback.GetUserDataValidFlag(ReceiveType_ListValidStart + _loc3_))
            {
               this.m_list_valid[_loc3_] = this.m_callback.GetUserDataInt(ReceiveType_ListValidStart + _loc3_);
            }
            else
            {
               this.m_list_valid[_loc3_] = true;
            }
            _loc5_ = this.m_timeline.win.list["btn" + (_loc3_ + 1)].sys_item;
            if(this.m_list_valid[_loc3_])
            {
               _loc5_.sys.textColor = 16777215;
            }
            else
            {
               _loc5_.sys.textColor = 8421504;
            }
            this.showFavorite(_loc5_.sys,_loc4_,this.m_list_valid[_loc3_]);
            _loc3_++;
         }
      }

      // BattleStyles.dll names a favorite style "<name>\t★": the row shows
      // the name in its usual colour and a gold star after it.
      private function showFavorite(param1:Object, param2:String, param3:Boolean) : void
      {
         if(param2 == null)
         {
            return;
         }
         var _loc4_:int = param2.indexOf("\t");
         if(_loc4_ < 0)
         {
            return;
         }
         var _loc5_:String = param2.substr(0,_loc4_).split("&").join("&amp;").split("<").join("&lt;").split(">").join("&gt;");
         var _loc6_:String = param3 ? "#FFFFFF" : "#808080";
         param1.htmlText = "<font color=\"" + _loc6_ + "\">" + _loc5_ + "</font> <font color=\"#FFC800\">★</font>";
      }
      
      private function endList() : void
      {
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.checkSearchRefresh);
         this.m_timeline.win.gotoAndPlay("end");
      }
      
      private function isEndList() : Boolean
      {
         return this.m_timeline.win.currentLabel == "end_comp";
      }
      
      private function sendListSelect() : void
      {
         var _loc1_:int = this.m_list_box.GetSelectIndex();
         this.m_callback.CallbackUserData("user",SendType_ListSelect,_loc1_);
      }
      
      private function changeListSelect(param1:int) : Boolean
      {
         var _loc5_:int = 0;
         var _loc6_:int = 0;
         var _loc7_:int = 0;
         var _loc2_:int = Math.abs(param1);
         var _loc3_:int = this.m_list_box.GetSelectIndex();
         if(_loc2_ == 1)
         {
            if(param1 < 0)
            {
               this.m_list_box.SelectPrev();
            }
            else if(0 < param1)
            {
               this.m_list_box.SelectNext();
            }
         }
         else if(_loc2_ == ListElemMax)
         {
            _loc5_ = this.m_callback.GetUserDataInt(ReceiveType_ListDataNum);
            _loc6_ = _loc3_ + param1;
            _loc7_ = this.m_list_box.GetSelectButtonIndex();
            if(_loc6_ - _loc7_ < 0)
            {
               _loc6_ = 0;
               _loc7_ = 0;
            }
            else if(_loc5_ <= _loc6_)
            {
               _loc6_ = _loc5_ - 1;
               _loc7_ = -1;
            }
            this.m_list_box.SelectList(_loc6_,_loc7_);
         }
         var _loc4_:int = this.m_list_box.GetSelectIndex();
         if(_loc3_ == _loc4_)
         {
            return false;
         }
         this.requestList();
         this.sendListSelect();
         return true;
      }
      
      private function selectDefaultList() : Boolean
      {
         if(!this.m_callback.GetUserDataValidFlag(ReceiveType_ListDefault))
         {
            return false;
         }
         var _loc1_:int = this.m_callback.GetUserDataInt(ReceiveType_ListDefault);
         var _loc2_:int = this.m_list_box.GetSelectIndex();
         this.m_list_box.SelectList(_loc1_,-1);
         var _loc3_:int = this.m_list_box.GetSelectIndex();
         if(_loc2_ != _loc3_)
         {
            this.requestList();
            return true;
         }
         return false;
      }
      
      private function pushU() : void
      {
         if(this.changeListSelect(-1))
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
         }
      }
      
      private function pushD() : void
      {
         if(this.changeListSelect(1))
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
         }
      }
      
      private function pushL() : void
      {
         if(this.changeListSelect(-ListElemMax))
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
         }
      }
      
      private function pushR() : void
      {
         if(this.changeListSelect(ListElemMax))
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeCarsol);
         }
      }
      
      private function pushX() : void
      {
         if(this.selectDefaultList())
         {
            this.sendListSelect();
            this.m_callback.CallbackSe(this.m_callback.SeTypeDecide);
         }
      }
      
      private function pushEnter() : void
      {
         if(!this.m_list_valid[this.m_list_box.GetSelectButtonIndex()])
         {
            this.m_callback.CallbackSe(this.m_callback.SeTypeNg);
            return;
         }
         this.endMenu(true);
         this.m_callback.CallbackSe(this.m_callback.SeTypeDecide);
      }
      
      private function pushCancel() : void
      {
         this.endMenu(false);
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
            case Keyboard.LEFT:
               this.pushL();
               break;
            case Keyboard.RIGHT:
               this.pushR();
               break;
            case 88:
               this.pushX();
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
      
      public function TestDestroy() : void
      {
         this.m_callback = null;
         this.m_list_box.ClearItem();
         this.m_list_box = null;
         this.m_timeline.stage.removeEventListener(KeyboardEvent.KEY_DOWN,this.checkKey);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitStart);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitEnd);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.waitReceiveList);
         this.m_timeline.stage.removeEventListener(Event.ENTER_FRAME,this.checkSearchRefresh);
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
      
      public function TestGetListStartIndex() : int
      {
         return this.m_list_box.GetStartIndex();
      }
   }
}

