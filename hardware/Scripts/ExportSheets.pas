{ Exports every component with its position, size and component classes (one class per schematic sheet).
  Run with the PCB open: File > Run Script > ExportSheets.pas > ExportSheets. }
Procedure ExportSheets;
Var
  Board  : IPCB_Board;
  Iter   : IPCB_BoardIterator;
  CIter  : IPCB_BoardIterator;
  Comp   : IPCB_Component;
  Cls    : IPCB_ObjectClass;
  L      : TStringList;
  S      : String;
  R      : TCoordRect;
  i      : Integer;
Begin
  Board := PCBServer.GetCurrentPCBBoard;
  If Board = Nil Then Exit;
  L := TStringList.Create;
  R := Board.BoardOutline.BoundingRectangle;
  L.Add('B;' + FloatToStr(CoordToMMs(R.Left - Board.XOrigin)) + ';' + FloatToStr(CoordToMMs(R.Bottom - Board.YOrigin)) + ';' +
        FloatToStr(CoordToMMs(R.Right - Board.XOrigin)) + ';' + FloatToStr(CoordToMMs(R.Top - Board.YOrigin)));
  CIter := Board.BoardIterator_Create;
  CIter.AddFilter_ObjectSet(MkSet(eClassObject));
  CIter.AddFilter_LayerSet(AllLayers);
  CIter.AddFilter_Method(eProcessAll);
  Cls := CIter.FirstPCBObject;
  While Cls <> Nil Do
  Begin
    If Cls.MemberKind = eClassMemberKind_Component Then
    Begin
      S := 'K;' + Cls.Name;
      Iter := Board.BoardIterator_Create;
      Iter.AddFilter_ObjectSet(MkSet(eComponentObject));
      Iter.AddFilter_LayerSet(AllLayers);
      Iter.AddFilter_Method(eProcessAll);
      Comp := Iter.FirstPCBObject;
      While Comp <> Nil Do
      Begin
        If Cls.IsMember(Comp) Then S := S + ';' + Comp.Name.Text;
        Comp := Iter.NextPCBObject;
      End;
      Board.BoardIterator_Destroy(Iter);
      L.Add(S);
    End;
    Cls := CIter.NextPCBObject;
  End;
  Board.BoardIterator_Destroy(CIter);
  Iter := Board.BoardIterator_Create;
  Iter.AddFilter_ObjectSet(MkSet(eComponentObject));
  Iter.AddFilter_LayerSet(AllLayers);
  Iter.AddFilter_Method(eProcessAll);
  Comp := Iter.FirstPCBObject;
  While Comp <> Nil Do
  Begin
    R := Comp.BoundingRectangleNoNameComment;
    L.Add('C;' + Comp.Name.Text + ';' + Comp.Pattern + ';' +
          FloatToStr(CoordToMMs(Comp.X - Board.XOrigin)) + ';' + FloatToStr(CoordToMMs(Comp.Y - Board.YOrigin)) + ';' +
          FloatToStr(Comp.Rotation) + ';' + IntToStr(Comp.Layer) + ';' +
          FloatToStr(CoordToMMs(R.Left - Board.XOrigin)) + ';' + FloatToStr(CoordToMMs(R.Bottom - Board.YOrigin)) + ';' +
          FloatToStr(CoordToMMs(R.Right - Board.XOrigin)) + ';' + FloatToStr(CoordToMMs(R.Top - Board.YOrigin)) + ';' +
          Comp.SourceHierarchicalPath + ';' + Comp.SourceDesignator);
    Comp := Iter.NextPCBObject;
  End;
  Board.BoardIterator_Destroy(Iter);
  L.SaveToFile('C:\Users\12502784\Documents\Altium_designs\IoT_SYNTHPATCH\Scripts\sheets_export.txt');
  L.Free;
  ShowMessage('Export done');
End;
