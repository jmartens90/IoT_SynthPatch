// Widen board outline 5 mm on each side (99 -> 109 mm, 22HP), move edge tracks along.
// v2: collect tracks first, modify after iteration; no origin change.
Procedure WidenBoardB;
Var
  Board : IPCB_Board;
  BO    : IPCB_BoardOutline;
  Seg   : TPolySegment;
  It    : IPCB_BoardIterator;
  T     : IPCB_Track;
  L     : TInterfaceList;
  i, XL, XR, D, Tol, nV : Integer;
Begin
  Board := PCBServer.GetCurrentPCBBoard;
  If Board = Nil Then Begin ShowMessage('No PCB'); Exit; End;
  XL  := Board.XOrigin;
  XR  := Board.XOrigin + MMsToCoord(99);
  D   := MMsToCoord(5);
  Tol := MMsToCoord(0.05);

  L := TInterfaceList.Create;
  It := Board.BoardIterator_Create;
  It.AddFilter_ObjectSet(MkSet(eTrackObject));
  It.AddFilter_LayerSet(MkSet(eMechanical1, eInternalPlane1, eInternalPlane2));
  It.AddFilter_Method(eProcessAll);
  T := It.FirstPCBObject;
  While T <> Nil Do
  Begin
    If T.Component = Nil Then L.Add(T);
    T := It.NextPCBObject;
  End;
  Board.BoardIterator_Destroy(It);

  PCBServer.PreProcess;
  For i := 0 To L.Count - 1 Do
  Begin
    T := L.Items[i];
    PCBServer.SendMessageToRobots(T.I_ObjectAddress, c_Broadcast, PCBM_BeginModify, c_NoEventData);
    If Abs(T.X1 - XL) < Tol Then T.X1 := T.X1 - D Else If Abs(T.X1 - XR) < Tol Then T.X1 := T.X1 + D;
    If Abs(T.X2 - XL) < Tol Then T.X2 := T.X2 - D Else If Abs(T.X2 - XR) < Tol Then T.X2 := T.X2 + D;
    PCBServer.SendMessageToRobots(T.I_ObjectAddress, c_Broadcast, PCBM_EndModify, c_NoEventData);
  End;

  nV := 0;
  BO := Board.BoardOutline;
  BO.BeginModify;
  For i := 0 To BO.PointCount - 1 Do
  Begin
    Seg := BO.Segments[i];
    If Abs(Seg.vx - XL) < Tol Then Begin Seg.vx := Seg.vx - D; nV := nV + 1; End
    Else If Abs(Seg.vx - XR) < Tol Then Begin Seg.vx := Seg.vx + D; nV := nV + 1; End;
    BO.Segments[i] := Seg;
  End;
  BO.EndModify;
  PCBServer.PostProcess;
  Board.ViewManager_FullUpdate;
  ShowMessage('Outline vertices moved: ' + IntToStr(nV) + ', edge tracks: ' + IntToStr(L.Count));
  L.Free;
End;
