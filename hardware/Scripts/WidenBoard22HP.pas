// Widen board outline 5 mm on each side (99 -> 109 mm, 22HP), move edge tracks along.
Procedure WidenBoard;
Var
  Board : IPCB_Board;
  BO    : IPCB_BoardOutline;
  Seg   : TPolySegment;
  It    : IPCB_BoardIterator;
  T     : IPCB_Track;
  i, X0, XL, XR, D, Tol, nT : Integer;
  LS    : TLayerSet;
Begin
  Board := PCBServer.GetCurrentPCBBoard;
  If Board = Nil Then Begin ShowMessage('No PCB'); Exit; End;
  X0  := Board.XOrigin;
  XL  := X0;
  XR  := X0 + MMsToCoord(99);
  D   := MMsToCoord(5);
  Tol := MMsToCoord(0.05);
  PCBServer.PreProcess;

  BO := Board.BoardOutline;
  BO.BeginModify;
  For i := 0 To BO.PointCount - 1 Do
  Begin
    Seg := BO.Segments[i];
    If Abs(Seg.vx - XL) < Tol Then Seg.vx := Seg.vx - D
    Else If Abs(Seg.vx - XR) < Tol Then Seg.vx := Seg.vx + D;
    BO.Segments[i] := Seg;
  End;
  BO.EndModify;
  BO.Invalidate;
  BO.Rebuild;
  BO.Validate;

  nT := 0;
  It := Board.BoardIterator_Create;
  It.AddFilter_ObjectSet(MkSet(eTrackObject));
  It.AddFilter_LayerSet(AllLayers);
  It.AddFilter_Method(eProcessAll);
  T := It.FirstPCBObject;
  While T <> Nil Do
  Begin
    If (T.Component = Nil) And ((T.Layer = eMechanical1) Or (T.Layer = eInternalPlane1) Or (T.Layer = eInternalPlane2)) Then
    Begin
      PCBServer.SendMessageToRobots(T.I_ObjectAddress, c_Broadcast, PCBM_BeginModify, c_NoEventData);
      If Abs(T.X1 - XL) < Tol Then T.X1 := T.X1 - D Else If Abs(T.X1 - XR) < Tol Then T.X1 := T.X1 + D;
      If Abs(T.X2 - XL) < Tol Then T.X2 := T.X2 - D Else If Abs(T.X2 - XR) < Tol Then T.X2 := T.X2 + D;
      PCBServer.SendMessageToRobots(T.I_ObjectAddress, c_Broadcast, PCBM_EndModify, c_NoEventData);
      nT := nT + 1;
    End;
    T := It.NextPCBObject;
  End;
  Board.BoardIterator_Destroy(It);

  // keep the lower-left corner as origin
  Board.XOrigin := X0 - D;

  PCBServer.PostProcess;
  Board.UpdateBoardOutline;
  Board.ViewManager_FullUpdate;
  ShowMessage('Widened outline, edge tracks touched: ' + IntToStr(nT));
End;
