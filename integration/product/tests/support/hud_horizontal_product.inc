// Exact unchanged product horizontal IME drawing block, checked by tools/verify_scope.py.
 if(imeVisible&&!vertical&&!app->findOpen()){
  lcd.fillRect(0,compositionY-4,300,1,black);
  auto textAt=[&](std::string_view text,int x,int y,Color color,int limit){
   for(size_t at=0;at<text.size();at=editor::nextUtf8Boundary(text,at)){
    const auto cp=s3app::display::scalar(text,at);const auto g=font.glyph(cp);
    if(x+g.advance>limit){glyph(0x2026,std::max(8,limit-28),y,color);break;}
    glyph(cp,x,y,color);x+=g.advance;
   }
  };
  if(auto* composition=ime->composition()){
   const auto snap=composition->snapshot();const auto selectedView=ime->selectedViewport(font);int selectedX=8;
   for(size_t m=0;m<snap.member_count;++m){const auto& member=snap.members[m];
    const auto first=std::max(selectedView.begin,member.text_begin);
    const auto last=std::min(selectedView.end,member.text_begin+member.text_size);
    if(first>=last)continue;
    const auto part=snap.selected_text.substr(first,last-first);
    textAt(part,selectedX,compositionY+(ime->raw().empty()?0:s3app::ImeCompositionFontMetrics::horizontal_selected_y),member.valid?black:Color::Red,294);
    for(size_t at=0;at<part.size();at=editor::nextUtf8Boundary(part,at))selectedX+=font.glyph(s3app::display::scalar(part,at)).advance;
   }
   if(selectedView.left)lcd.fillRect(3,compositionY+12,2,5,Color::Red);
   if(selectedView.right)lcd.fillRect(296,compositionY+12,2,5,Color::Red);
  }
  s3app::paintHorizontalRaw(*ime,compositionY,{
   [](void*,int x,int y){lcd.pixel(x,y,black);},
   [](void*,int x,int y,unsigned w,unsigned h){lcd.fillRect(x,y,w,h,Color::Red);},nullptr});
  int x=8;
  const auto* view=ime->view();const bool candidatesAvailable=ime->candidateAvailable();
  if(view){for(size_t r=0;r<view->count;++r){const auto& row=view->rows[r];x=8;const int y=compositionY+48+int(r)*34;
   for(size_t c=0;c<row.count;++c){const auto& item=row.items[c];const bool selected=candidatesAvailable&&row.index==view->row&&c==view->col;
    app->candidateVisual.captureHorizontal({item.text.data(),item.bytes},x,y,item.width);
    if(candidatesAvailable&&row.index==view->row){char labelText[2]={char('1'+c),0};label(labelText,x,y+8,selected?Color::Red:black);}
    textAt({item.text.data(),item.bytes},x+14,y,selected?Color::Red:black,std::min(294,x+int(item.width)-4));x+=item.width;
   }
  }}
  const auto& pending=app->candidateVisual;
  if(pending.pending()){
   for(size_t i=0;i<pending.text.count;++i){const auto& item=pending.text.items[i];
    textAt({item.text.data(),item.bytes},item.box.x+14,item.box.y,black,item.limit);
   }
  }
  if(ime->boundaryVisible(millis())){auto rect=ime_device::boundaryRect(ime->boundaryEdge(),compositionY,view?view->count:0);
   if(ime->boundaryEdge()==ime_device::BoundaryEdge::RawLeft||ime->boundaryEdge()==ime_device::BoundaryEdge::RawRight){rect.y=compositionY+s3app::ImeCompositionFontMetrics::horizontal_ink_y;rect.height=s3app::ImeCompositionFontMetrics::horizontal_ink_height;}
   lcd.fillRect(rect.x,rect.y,rect.width,rect.height,Color::Red);}
 }
