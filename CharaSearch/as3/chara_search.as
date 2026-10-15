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

