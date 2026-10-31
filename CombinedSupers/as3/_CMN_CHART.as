// The stat radar chart (CMN_CHART.iggy, imported at runtime by the
// Attributes screen, character select and the player data charts). Combined
// Supers serves this script only while it combines the supers, so the chart
// shows the two supers corners as one stat: a chain link on the edge between
// them (icn_5 = Strike Supers, icn_6 = Ki Blast Supers, the same frames as
// the Attributes rows' icons) and a "SUP" label just outside it.
// Vanilla part (vars, frame scripts) as exported by FFDec; built by
// ClaudeTools\iggy\menu_abc.py cs_chart into src\cmn_chart_abc.h.
package
{
   import flash.display.DisplayObject;
   import flash.display.DisplayObjectContainer;
   import flash.display.MovieClip;
   import flash.display.Sprite;
   import flash.events.Event;
   import flash.text.TextField;

   public dynamic class _CMN_CHART extends MovieClip
   {

      private static const LinkGold:uint = 0xD8A838;

      private static const LinkGoldLight:uint = 0xFFE7A0;

      private static const LinkShadow:uint = 0x1A1206;

      private static const LabelText:String = "SUP";

      private static const LabelColor:uint = 0xFFD860;

      // Link size and label distance as fractions of the STR-BLA edge length
      // (by eye: the link about as tall as the icons' labels).
      private static const LinkSizeOfEdge:Number = 0.14;

      private static const LabelDistanceOfEdge:Number = 0.2;

      // Laid out once the start animation has stopped moving the icons:
      // their positions the same for this many frames in a row, or after
      // LinkMaxFrames frames at most.
      private static const LinkSettleFrames:int = 3;

      private static const LinkMaxFrames:int = 90;

      // The font for "SUP" is copied from a text field found under the root;
      // the search stops after this many display objects.
      private static const FontSearchLimit:int = 300;

      public var color_fill_after:MovieClip;

      public var color_fill_before:MovieClip;

      public var color_line_after:MovieClip;

      public var dammy:MovieClip;

      public var dammy_def:MovieClip;

      public var fill_dammy:MovieClip;

      public var icn_1:MovieClip;

      public var icn_1arw:MovieClip;

      public var icn_2:MovieClip;

      public var icn_2arw:MovieClip;

      public var icn_3:MovieClip;

      public var icn_3arw:MovieClip;

      public var icn_4:MovieClip;

      public var icn_4arw:MovieClip;

      public var icn_5:MovieClip;

      public var icn_5arw:MovieClip;

      public var icn_6:MovieClip;

      public var icn_6arw:MovieClip;

      public var plt:MovieClip;

      private var m_link:Sprite = null;

      private var m_link_frames:int = 0;

      private var m_link_stable_frames:int = 0;

      private var m_link_last_key:String = "";

      public function _CMN_CHART()
      {
         super();
         addFrameScript(16,this.frame17,22,this.frame23);
         addEventListener(Event.ENTER_FRAME,this.supersWaitIcons);
      }

      internal function frame17() : *
      {
         stop();
      }

      internal function frame23() : *
      {
         stop();
      }

      // Waits (bounded) until the six corner icons exist and stand still.
      private function supersWaitIcons(param1:Event) : void
      {
         var icons:Array = [this.icn_1,this.icn_2,this.icn_3,this.icn_4,this.icn_5,this.icn_6];
         var key:String = "";
         var i:int = 0;
         this.m_link_frames++;
         while(i < icons.length)
         {
            if(icons[i] == null)
            {
               key = "";
               break;
            }
            key += icons[i].x + "," + icons[i].y + ";";
            i++;
         }
         if(key != "" && key == this.m_link_last_key)
         {
            this.m_link_stable_frames++;
         }
         else
         {
            this.m_link_stable_frames = 0;
         }
         this.m_link_last_key = key;
         if(this.m_link_stable_frames < LinkSettleFrames && this.m_link_frames < LinkMaxFrames)
         {
            return;
         }
         removeEventListener(Event.ENTER_FRAME,this.supersWaitIcons);
         if(key != "")
         {
            this.supersDrawLink(icons);
         }
      }

      // The link on the middle of the icn_5-icn_6 edge, along the edge, and
      // "SUP" outside it (away from the chart's centre).
      private function supersDrawLink(icons:Array) : void
      {
         var centreX:Number = 0;
         var centreY:Number = 0;
         var i:int = 0;
         while(i < icons.length)
         {
            centreX += icons[i].x / icons.length;
            centreY += icons[i].y / icons.length;
            i++;
         }
         var edgeX:Number = this.icn_6.x - this.icn_5.x;
         var edgeY:Number = this.icn_6.y - this.icn_5.y;
         var edgeLength:Number = Math.sqrt(edgeX * edgeX + edgeY * edgeY);
         if(edgeLength <= 0)
         {
            return;
         }
         var middleX:Number = (this.icn_5.x + this.icn_6.x) / 2;
         var middleY:Number = (this.icn_5.y + this.icn_6.y) / 2;
         var outX:Number = middleX - centreX;
         var outY:Number = middleY - centreY;
         var outLength:Number = Math.sqrt(outX * outX + outY * outY);
         if(outLength > 0)
         {
            outX /= outLength;
            outY /= outLength;
         }
         var size:Number = edgeLength * LinkSizeOfEdge;
         this.m_link = new Sprite();
         this.m_link.mouseEnabled = false;
         this.m_link.mouseChildren = false;
         var chain:Sprite = new Sprite();
         this.supersDrawChain(chain.graphics,size);
         chain.x = middleX;
         chain.y = middleY;
         chain.rotation = Math.atan2(edgeY,edgeX) * 180 / Math.PI;
         this.m_link.addChild(chain);
         var labelX:Number = middleX + outX * edgeLength * LabelDistanceOfEdge;
         var labelY:Number = middleY + outY * edgeLength * LabelDistanceOfEdge;
         var label:DisplayObject = this.supersMakeLabel(size);
         label.x = labelX - label.width / 2;
         label.y = labelY - label.height / 2;
         this.m_link.addChild(label);
         addChild(this.m_link);
      }

      // Two interlocked links lying along the x axis, centred on 0: a dark
      // outline pass, a gold pass and a pale highlight on the gold.
      private function supersDrawChain(g:*, size:Number) : void
      {
         var linkLength:Number = size * 1.3;
         var linkHeight:Number = size * 0.7;
         var overlap:Number = size * 0.35;
         var pass:int = 0;
         var widths:Array = [size * 0.32,size * 0.16,size * 0.05];
         var colors:Array = [LinkShadow,LinkGold,LinkGoldLight];
         var alphas:Array = [0.7,1,0.8];
         while(pass < 3)
         {
            g.lineStyle(widths[pass],colors[pass],alphas[pass]);
            g.drawRoundRect(-linkLength + overlap / 2,-linkHeight / 2,linkLength,linkHeight,linkHeight,linkHeight);
            g.drawRoundRect(-overlap / 2,-linkHeight / 2,linkLength,linkHeight,linkHeight,linkHeight);
            pass++;
         }
      }

      // "SUP" in the chart's own font when a text field is found to copy
      // (Iggy draws only fonts the movie already has), else drawn by lines.
      private function supersMakeLabel(size:Number) : DisplayObject
      {
         var source:TextField = supersFindField(this.root != null ? this.root : this);
         if(source == null)
         {
            return supersDrawLetters(size);
         }
         var field:TextField = new TextField();
         var format:* = source.getTextFormat();
         format.align = "left";
         format.leading = 0;
         format.color = LabelColor;
         field.defaultTextFormat = format;
         field.embedFonts = source.embedFonts;
         field.selectable = false;
         field.mouseEnabled = false;
         field.multiline = false;
         field.wordWrap = false;
         field.autoSize = "left";
         field.filters = source.filters;
         field.text = LabelText;
         // Text about as tall as the link.
         var scale:Number = field.height > 0 ? size * 1.1 / field.height : 1;
         field.scaleX = scale;
         field.scaleY = scale;
         return field;
      }

      // The first text field under `start`, breadth first, looking at no
      // more than FontSearchLimit objects.
      private static function supersFindField(start:DisplayObject) : TextField
      {
         var queue:Array = [start];
         var visited:int = 0;
         var node:DisplayObject = null;
         var container:DisplayObjectContainer = null;
         var i:int = 0;
         while(queue.length > 0 && visited < FontSearchLimit)
         {
            node = queue.shift();
            visited++;
            if(node is TextField)
            {
               return TextField(node);
            }
            container = node as DisplayObjectContainer;
            if(container != null)
            {
               i = 0;
               while(i < container.numChildren)
               {
                  queue.push(container.getChildAt(i));
                  i++;
               }
            }
         }
         return null;
      }

      // "SUP" in strokes, `size` tall, top left at 0 (fallback only).
      private static function supersDrawLetters(size:Number) : DisplayObject
      {
         var letters:Sprite = new Sprite();
         var g:* = letters.graphics;
         var w:Number = size * 0.6;
         var gap:Number = size * 0.25;
         var pass:int = 0;
         var x:Number = 0;
         while(pass < 2)
         {
            g.lineStyle(pass == 0 ? size * 0.28 : size * 0.13,pass == 0 ? LinkShadow : LabelColor,1);
            x = 0;
            // S
            g.moveTo(x + w,0);
            g.lineTo(x,0);
            g.lineTo(x,size / 2);
            g.lineTo(x + w,size / 2);
            g.lineTo(x + w,size);
            g.lineTo(x,size);
            x += w + gap;
            // U
            g.moveTo(x,0);
            g.lineTo(x,size);
            g.lineTo(x + w,size);
            g.lineTo(x + w,0);
            x += w + gap;
            // P
            g.moveTo(x,size);
            g.lineTo(x,0);
            g.lineTo(x + w,0);
            g.lineTo(x + w,size / 2);
            g.lineTo(x,size / 2);
            pass++;
         }
         return letters;
      }
   }
}
