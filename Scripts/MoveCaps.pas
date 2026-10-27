Procedure MoveCaps;
Var
  Board : IPCB_Board;
  Iter  : IPCB_BoardIterator;
  Comp  : IPCB_Component;
  N     : Integer;
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
    If (Comp.Name.Text = 'C83') Or (Comp.Name.Text = 'C85') Then
    Begin
      PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast, PCBM_BeginModify, c_NoEventData);
      If Comp.Name.Text = 'C83' Then
        Comp.MoveToXY(Board.XOrigin + MMsToCoord(118), Board.YOrigin + MMsToCoord(21))
      Else
        Comp.MoveToXY(Board.XOrigin + MMsToCoord(127), Board.YOrigin + MMsToCoord(21));
      PCBServer.SendMessageToRobots(Comp.I_ObjectAddress, c_Broadcast, PCBM_EndModify, c_NoEventData);
      N := N + 1;
    End;
    Comp := Iter.NextPCBObject;
  End;
  Board.BoardIterator_Destroy(Iter);
  PCBServer.PostProcess;
  Board.ViewManager_FullUpdate;
  ShowMessage('Moved ' + IntToStr(N) + ' caps.');
End;
