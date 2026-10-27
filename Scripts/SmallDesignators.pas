{ Smaller designators: 0.8 mm text, 0.12 mm stroke, centred on the part; comments hidden }
Procedure SmallDesignators;
Var Board : IPCB_Board; Iter : IPCB_BoardIterator; Comp : IPCB_Component; N : Integer;
Begin
  Board := PCBServer.GetCurrentPCBBoard;
  If Board = Nil Then Exit;
  N := 0;
  PCBServer.PreProcess;
  Iter := Board.BoardIterator_Create;
  Iter.AddFilter_ObjectSet(MkSet(eComponentObject));
  Iter.AddFilter_LayerSet(AllLayers);
  Iter.AddFilter_Method(eProcessAll);
  Comp := Iter.FirstPCBObject;
  While Comp <> Nil Do
  Begin
    PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast, PCBM_BeginModify, c_NoEventData);
    PCBServer.SendMessageToRobots(Comp.Name.I_ObjectAddress, c_Broadcast, PCBM_BeginModify, c_NoEventData);
    Comp.Name.UseTTFonts := False;
    Comp.Name.Size  := MMsToCoord(0.8);
    Comp.Name.Width := MMsToCoord(0.12);
    PCBServer.SendMessageToRobots(Comp.Name.I_ObjectAddress, c_Broadcast, PCBM_EndModify, c_NoEventData);
    Comp.CommentOn := False;
    Comp.ChangeNameAutoposition(eAutoPos_CenterCenter);
    PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast, PCBM_EndModify, c_NoEventData);
    N := N + 1;
    Comp := Iter.NextPCBObject;
  End;
  Board.BoardIterator_Destroy(Iter);
  PCBServer.PostProcess;
  Board.ViewManager_FullUpdate;
  ShowMessage('Resized ' + IntToStr(N) + ' designators.');
End;
